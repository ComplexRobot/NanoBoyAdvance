/*
 * Copyright (C) 2024 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <nba/common/dsp/resampler/cosine.hpp>
#include <nba/common/dsp/resampler/cubic.hpp>
#include <nba/common/dsp/resampler/nearest.hpp>
#include <nba/common/dsp/resampler/sinc.hpp>

#include "apu.hpp"
#include "ogg.h"

namespace nba::core {

APU::APU(
  Scheduler& scheduler,
  DMA& dma,
  Bus& bus,
  std::shared_ptr<Config> config
)   : mmio(scheduler)
    , scheduler(scheduler)
    , dma(dma)
    , mp2k(bus)
    , config(config) {
  scheduler.Register(Scheduler::EventClass::APU_mixer, this, &APU::StepMixer);
  scheduler.Register(Scheduler::EventClass::APU_sequencer, this, &APU::StepSequencer);
}

APU::~APU() {
  config->audio_dev->Close();
}

void APU::Reset() {
  mmio.fifo[0].Reset();
  mmio.fifo[1].Reset();
  mmio.psg1.Reset();
  mmio.psg2.Reset();
  mmio.psg3.Reset(WaveChannel::ResetWaveRAM::Yes);
  mmio.psg4.Reset();
  mmio.soundcnt.Reset();
  mmio.bias.Reset();
  fifo_pipe[0] = {};
  fifo_pipe[1] = {};

  resolution_old = 0;
  sampleCount = 0;
  startTime = std::chrono::steady_clock::now();
  scheduler.Add(std::round(16777216.0 / ADJUSTED_SAMPLE_RATE), Scheduler::EventClass::APU_mixer);
  scheduler.Add(BaseChannel::s_cycles_per_step, Scheduler::EventClass::APU_sequencer);

  mp2k.Reset();
  mp2k_read_index = {};

  auto audio_dev = config->audio_dev;
  audio_dev->Close();
  audio_dev->Open(this, (AudioDevice::Callback)AudioCallback);

  using Interpolation = Config::Audio::Interpolation;

  buffer = std::make_shared<StereoRingBlockBuffer<s16, RING_BUFFER_SIZE>>();
  resampleBuffer = std::make_shared<StereoRingBuffer<float, 1>>();

  switch(config->audio.interpolation) {
  case Interpolation::Cosine:
    resampler = std::make_unique<CosineStereoResampler<float>>(resampleBuffer);
    break;
  case Interpolation::Cubic:
    resampler = std::make_unique<CubicStereoResampler<float>>(resampleBuffer);
    break;
  case Interpolation::Sinc_32:
    resampler = std::make_unique<SincStereoResampler<float, 32>>(resampleBuffer);
    break;
  case Interpolation::Sinc_64:
    resampler = std::make_unique<SincStereoResampler<float, 64>>(resampleBuffer);
    break;
  case Interpolation::Sinc_128:
    resampler = std::make_unique<SincStereoResampler<float, 128>>(resampleBuffer);
    break;
  case Interpolation::Sinc_256:
    resampler = std::make_unique<SincStereoResampler<float, 256>>(resampleBuffer);
    break;
  }

  resampler->SetSampleRates(mmio.bias.GetSampleRate(), audio_dev->GetSampleRate());
}

void APU::OnTimerOverflow(int timer_id, int times) {
  auto const& soundcnt = mmio.soundcnt;

  if(!soundcnt.master_enable) {
    return;
  }

  constexpr DMA::Occasion occasion[2] = { DMA::Occasion::FIFO0, DMA::Occasion::FIFO1 };

  for(int fifo_id = 0; fifo_id < 2; fifo_id++) {
    if(soundcnt.dma[fifo_id].timer_id == timer_id) {
      auto& fifo = mmio.fifo[fifo_id];
      auto& pipe = fifo_pipe[fifo_id];

      if(fifo.Count() <= 3) {
        dma.Request(occasion[fifo_id]);
      }

      if(pipe.size == 0 && fifo.Count() > 0) {
        pipe.word = fifo.ReadWord();
        pipe.size = 4;
      }

      s8 sample = (s8)(u8)pipe.word;

      if(pipe.size > 0) {
        pipe.word >>= 8;
        pipe.size--;
      }

      latch[fifo_id] = sample;
    }
  }
}

void APU::StepMixer() {
  constexpr int psg_volume_tab[4] = { 1, 2, 4, 0 };
  constexpr int dma_volume_tab[2] = { 2, 4 };

  auto& psg = mmio.soundcnt.psg;
  auto& dma = mmio.soundcnt.dma;

  auto psg_volume = psg_volume_tab[psg.volume];

  StereoSample<float> sampleOut;

  if(mp2k.IsEngaged()) {
    StereoSample<float> sample { 0, 0 };

    auto mp2k_sample = mp2k.ReadSample();

    for(int channel = 0; channel < 2; channel++) {
      s16 psg_sample = 0;

      if(psg.enable[channel][0]) psg_sample += mmio.psg1.GetSample();
      if(psg.enable[channel][1]) psg_sample += mmio.psg2.GetSample();
      if(psg.enable[channel][2]) psg_sample += mmio.psg3.GetSample();
      if(psg.enable[channel][3]) psg_sample += mmio.psg4.GetSample();

      sample[channel] += psg_sample * psg_volume * (psg.master[channel] + 1) / (32.0 * 0x200);

      /* TODO: we assume that MP2K sends right channel to FIFO A and left channel to FIFO B,
       * but we haven't verified that this is actually correct.
       */
      for(int fifo = 0; fifo < 2; fifo++) {
        if(dma[fifo].enable[channel]) {
          sample[channel] += mp2k_sample[fifo] * dma_volume_tab[dma[fifo].volume] * 0.25;
        }
      }
    }

    if(!mmio.soundcnt.master_enable) sample = {};

    OGG::ProcessSample(sample, scheduler);

    sampleOut = sample;

  } else {
    StereoSample<s16> sample { 0, 0 };

    auto& bias = mmio.bias;

    for(int channel = 0; channel < 2; channel++) {
      s16 psg_sample = 0;

      if(psg.enable[channel][0]) psg_sample += mmio.psg1.GetSample();
      if(psg.enable[channel][1]) psg_sample += mmio.psg2.GetSample();
      if(psg.enable[channel][2]) psg_sample += mmio.psg3.GetSample();
      if(psg.enable[channel][3]) psg_sample += mmio.psg4.GetSample();

      sample[channel] += psg_sample * psg_volume * (psg.master[channel] + 1) >> 5;

      for(int fifo = 0; fifo < 2; fifo++) {
        if(dma[fifo].enable[channel]) {
          sample[channel] += latch[fifo] * dma_volume_tab[dma[fifo].volume];
        }
      }

      sample[channel] += mmio.bias.level;
      sample[channel]  = std::clamp(sample[channel], s16(0), s16(0x3FF));
      sample[channel] -= 0x200;
    }

    if(!mmio.soundcnt.master_enable) sample = {};

    sampleOut = {
      sample[0] * (1.0f / 512),
      sample[1] * (1.0f / 512)
    };

    OGG::ProcessSample(sampleOut, scheduler);

  }

  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  if ((sampleCount + 1) / std::chrono::duration<double>{ now - startTime }.count() <= config->audio_dev->GetSampleRate()) {
    ++sampleCount;

    const float volume = std::clamp(config->audio.volume, 0, 100) / 100.0f;

    std::lock_guard guard{ buffer_mutex };
    buffer->Push({
      (s16)std::clamp<s32>(std::round(sampleOut[1] * volume * 32767.5f), -32768, 32767),
      (s16)std::clamp<s32>(std::round(sampleOut[0] * volume * 32767.5f), -32768, 32767),
    });
  }
    
  constexpr double CYCLES_PER_SAMPLE = 16777216 / ADJUSTED_SAMPLE_RATE;
  u64 nowTimeStamp = scheduler.GetTimestampNow();
  u64 nextTimeStamp = std::round(std::round(nowTimeStamp / CYCLES_PER_SAMPLE + 1) * CYCLES_PER_SAMPLE);
  scheduler.Add(nextTimeStamp - nowTimeStamp, Scheduler::EventClass::APU_mixer);
  
}

void APU::StepSequencer() {
  mmio.psg1.Tick();
  mmio.psg2.Tick();
  mmio.psg3.Tick();
  mmio.psg4.Tick();

  scheduler.Add(BaseChannel::s_cycles_per_step, Scheduler::EventClass::APU_sequencer);
}

} // namespace nba::core
