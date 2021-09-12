/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <platform/device/sdl_audio_device.hpp>
#include <QApplication>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>

#include "main_window.hpp"

MainWindow::MainWindow(QApplication* app, QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("NanoBoyAdvance");

  screen = std::make_shared<Screen>(this);
  setCentralWidget(screen.get());

  /* Read the configuration initially so that the correct configuration will be displayed. */
  ReadConfig();

  auto menubar = new QMenuBar(this);
  setMenuBar(menubar);

  CreateFileMenu(menubar);
  CreateOptionsMenu(menubar);
  CreateHelpMenu(menubar);

  /* Set emulator config */
  config->video_dev = screen;
  config->audio_dev = std::make_shared<nba::SDL2_AudioDevice>();
  config->input_dev = input_device;
  emulator = std::make_unique<nba::Emulator>(config);

  app->installEventFilter(this);
}

void MainWindow::CreateFileMenu(QMenuBar* menubar) {
  auto file_menu = menubar->addMenu(tr("&File"));
  auto file_open = file_menu->addAction(tr("&Open"));
  auto file_close = file_menu->addAction(tr("&Close"));

  connect(file_open, &QAction::triggered, this, &MainWindow::FileOpen);
  connect(file_close, &QAction::triggered, &QApplication::quit);
}

void MainWindow::CreateOptionsMenu(QMenuBar* menubar) {
  auto options_menu = menubar->addMenu(tr("&Options"));

  auto options_bios_menu = options_menu->addMenu(tr("BIOS"));
  auto options_bios_path = options_bios_menu->addAction(tr("Select path"));
  CreateBooleanOption(options_bios_menu, "Skip intro", &config->skip_bios);
  connect(options_bios_path, &QAction::triggered, [this] {
    QFileDialog dialog{this};
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter("Game Boy Advance BIOS (*.bin)");

    if (!dialog.exec()) {
      return;
    }

    config->bios_path = dialog.selectedFiles().at(0).toStdString();
    WriteConfig();
  });

  auto options_cartridge_menu = options_menu->addMenu(tr("Cartridge"));
  CreateBooleanOption(options_cartridge_menu, "Force RTC", &config->force_rtc);
  CreateSelectionOption(options_cartridge_menu->addMenu(tr("Save Type")), {
    { "Detect",      nba::Config::BackupType::Detect },
    { "SRAM",        nba::Config::BackupType::SRAM   },
    { "FLASH 64K",   nba::Config::BackupType::FLASH_64  },
    { "FLASH 128K",  nba::Config::BackupType::FLASH_128 },
    { "EEPROM 512B", nba::Config::BackupType::EEPROM_4  },
    { "EEPROM 8K",   nba::Config::BackupType::EEPROM_64 }
  }, &config->backup_type);

  auto options_audio_menu = options_menu->addMenu(tr("Audio"));
  CreateSelectionOption(options_audio_menu->addMenu("Resampler"), {
    { "Cosine",   nba::Config::Audio::Interpolation::Cosine },
    { "Cubic",    nba::Config::Audio::Interpolation::Cubic  },
    { "Sinc-64",  nba::Config::Audio::Interpolation::Sinc_64  },
    { "Sinc-128", nba::Config::Audio::Interpolation::Sinc_128 },
    { "Sinc-256", nba::Config::Audio::Interpolation::Sinc_256 }
  }, &config->audio.interpolation);

  auto options_hq_audio_menu = options_audio_menu->addMenu("HQ audio mixer");
  CreateBooleanOption(options_hq_audio_menu, "Enable", &config->audio.mp2k_hle_enable);
  CreateBooleanOption(options_hq_audio_menu, "Use cubic filter", &config->audio.mp2k_hle_cubic);
}

void MainWindow::CreateBooleanOption(QMenu* menu, const char* name, bool* underlying) {
  auto action = menu->addAction(QString{name});
  auto config = this->config;

  action->setCheckable(true);
  action->setChecked(*underlying);

  connect(action, &QAction::triggered, [=](bool checked) {
    *underlying = checked;
    WriteConfig();
  });
}

void MainWindow::CreateHelpMenu(QMenuBar* menubar) {
  auto help_menu = menubar->addMenu(tr("&?"));
  auto about_app = help_menu->addAction(tr("About NanoBoyAdvance"));

  about_app->setMenuRole(QAction::AboutRole);
  connect(about_app, &QAction::triggered, [&] {
    QMessageBox box{ this };
    box.setTextFormat(Qt::RichText);
    box.setText(tr("NanoBoyAdvance is a Game Boy Advance emulator with a focus on high accuracy.<br><br>"
                   "Copyright © 2015 - 2021 fleroviux<br><br>"
                   "NanoBoyAdvance is licensed under the GPLv3 or any later version.<br><br>"
                   "GitHub: <a href=\"https://github.com/fleroviux/NanoBoyAdvance\">https://github.com/fleroviux/NanoBoyAdvance</a><br><br>"
                   "Game Boy Advance is a registered trademark of Nintendo Co., Ltd."));
    box.setWindowTitle(tr("About NanoBoyAdvance"));
    box.exec();
  });
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
  auto type = event->type();

  if (type != QEvent::KeyPress && type != QEvent::KeyRelease) {
    return QObject::eventFilter(obj, event);
  }

  auto key = dynamic_cast<QKeyEvent*>(event)->key();
  auto pressed = type == QEvent::KeyPress;

  /* TODO: support dynamic key mapping. */
  switch (key) {
    case Qt::Key_A: {
      input_device->SetKeyStatus(nba::InputDevice::Key::A, pressed);
      return true;
    }
    case Qt::Key_S: {
      input_device->SetKeyStatus(nba::InputDevice::Key::B, pressed);
      return true;
    }
    case Qt::Key_D: {
      input_device->SetKeyStatus(nba::InputDevice::Key::L, pressed);
      return true;
    }
    case Qt::Key_F: {
      input_device->SetKeyStatus(nba::InputDevice::Key::R, pressed);
      return true;
    }
    case Qt::Key_Return: {
      input_device->SetKeyStatus(nba::InputDevice::Key::Start, pressed);
      return true;
    }
    case Qt::Key_Backspace: {
      input_device->SetKeyStatus(nba::InputDevice::Key::Select, pressed);
      return true;
    }
    case Qt::Key_Up: {
      input_device->SetKeyStatus(nba::InputDevice::Key::Up, pressed);
      return true;
    }
    case Qt::Key_Down: {
      input_device->SetKeyStatus(nba::InputDevice::Key::Down, pressed);
      return true;
    }
    case Qt::Key_Left: {
      input_device->SetKeyStatus(nba::InputDevice::Key::Left, pressed);
      return true;
    }
    case Qt::Key_Right: {
      input_device->SetKeyStatus(nba::InputDevice::Key::Right, pressed);
      return true;
    }
    case Qt::Key_Space: {
      framelimiter.SetFastForward(pressed);
      return true;
    }
    default: {
      return QObject::eventFilter(obj, event);
    }
  }
}

void MainWindow::FileOpen() {
  using StatusCode = nba::Emulator::StatusCode;

  QFileDialog dialog {this};
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setNameFilter("Game Boy Advance ROMs (*.gba *.agb)");

  if (!dialog.exec()) {
    return;
  }

  auto file = dialog.selectedFiles().at(0);

  if (emulator_state == EmulationState::Running) {
    emulator_state = EmulationState::Stopped;
    while (emulator_thread_running) ;
  }

  /* Load emulator configuration */
  ReadConfig();

  /* TODO: move message boxes to another method. */
  switch (emulator->LoadGame(file.toStdString())) {
    case StatusCode::GameNotFound: {
      QMessageBox box {this};
      box.setText(tr("Cannot find file: ") + QFileInfo(file).fileName());
      box.setIcon(QMessageBox::Critical);
      box.setWindowTitle(tr("File not found"));
      box.exec();
      return;
    }
    case StatusCode::BiosNotFound: {
      QMessageBox box {this};
      box.setText(tr("Please reference a BIOS file in the configuration.\n\n"
                     "Cannot open BIOS: ") + QString{config->bios_path.c_str()});
      box.setIcon(QMessageBox::Critical);
      box.setWindowTitle(tr("BIOS not found"));
      box.exec();
      return;
    }
    case StatusCode::GameWrongSize: {
      QMessageBox box {this};
      box.setIcon(QMessageBox::Critical);
      box.setText(tr("The file you opened exceeds the maximum size of 32 MiB."));
      box.setWindowTitle(tr("ROM exceeds maximum size"));
      box.exec();
      return;
    }
    case StatusCode::BiosWrongSize: {
      QMessageBox box {this};
      box.setIcon(QMessageBox::Critical);
      box.setText(tr("Your BIOS file exceeds the maximum size of 16 KiB."));
      box.setWindowTitle(tr("BIOS file exceeds maximum size"));
      box.exec();
      return;
    }
  }

  emulator->Reset();
  emulator_state = EmulationState::Running;
  framelimiter.Reset();

  emulator_thread = std::thread([this] {
    emulator_thread_running = true;

    while (emulator_state == EmulationState::Running) {
      framelimiter.Run([&] {
        emulator->Frame();
      }, [&](int fps) {
        this->setWindowTitle(QString{ (std::string("NanoBoyAdvance [") + std::to_string(fps) + std::string(" fps]")).c_str() });
      });
    }

    emulator_thread_running = false;
  });

  emulator_thread.detach();
}
