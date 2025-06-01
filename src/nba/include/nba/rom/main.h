#pragma once

#include <nba/integer.hpp>

typedef volatile u16 vu16;
typedef u8  bool8;
typedef u16 bool16;

struct OamData
{
  /*0x00*/ u32 y:8;
  /*0x01*/ u32 affineMode:2;  // 0x1, 0x2 -> 0x4
  u32 objMode:2;     // 0x4, 0x8 -> 0xC
  u32 mosaic:1;      // 0x10
  u32 bpp:1;         // 0x20
  u32 shape:2;       // 0x40, 0x80 -> 0xC0

  /*0x02*/ u32 x:9;
  u32 matrixNum:5;   // bits 3/4 are h-flip/v-flip if not in affine mode
  u32 size:2;        // 0x4000, 0x8000 -> 0xC000

  /*0x04*/ u16 tileNum:10;    // 0x3FF
  u16 priority:2;    // 0x400, 0x800 -> 0xC00
  u16 paletteNum:4;
  /*0x06*/ u16 affineParam;
};

typedef u32 MainCallback;
typedef u32 IntrCallback;
typedef u32 IntrFunc;

extern IntrFunc gIntrTable[];

struct Main
{
    /*0x000*/ MainCallback callback1;
    /*0x004*/ MainCallback callback2;

    /*0x008*/ MainCallback savedCallback;

    /*0x00C*/ IntrCallback vblankCallback;
    /*0x010*/ IntrCallback hblankCallback;
    /*0x014*/ IntrCallback vcountCallback;
    /*0x018*/ IntrCallback serialCallback;

    /*0x01C*/ vu16 intrCheck;

    /*0x020*/ u32 vblankCounter1;
    /*0x024*/ u32 vblankCounter2;

    /*0x028*/ u16 heldKeysRaw;           // held keys without L=A remapping
    /*0x02A*/ u16 newKeysRaw;            // newly pressed keys without L=A remapping
    /*0x02C*/ u16 heldKeys;              // held keys with L=A remapping
    /*0x02E*/ u16 newKeys;               // newly pressed keys with L=A remapping
    /*0x030*/ u16 newAndRepeatedKeys;    // newly pressed keys plus key repeat
    /*0x032*/ u16 keyRepeatCounter;      // counts down to 0, triggering key repeat
    /*0x034*/ bool16 watchedKeysPressed; // whether one of the watched keys was pressed
    /*0x036*/ u16 watchedKeysMask;       // bit mask for watched keys

    /*0x038*/ struct OamData oamBuffer[128];

    /*0x438*/ u8 state;

    /*0x439*/ u8 oamLoadDisabled:1;
    /*0x439*/ u8 inBattle:1;
    /*0x439*/ u8 field_439_x4:1;

    u16 currentSong;
};

extern struct Main gMain;
extern bool8 gSoftResetDisabled;
extern bool8 gLinkVSyncDisabled;

extern const u8 gGameVersion;
extern const u8 gGameLanguage;

void AgbMain(void);
void SetMainCallback2(MainCallback callback);
void InitKeys(void);
void SetVBlankCallback(IntrCallback callback);
void SetHBlankCallback(IntrCallback callback);
void SetVCountCallback(IntrCallback callback);
void SetSerialCallback(IntrCallback callback);
void InitFlashTimer(void);
void DoSoftReset(void);
void ClearPokemonCrySongs(void);
void RestoreSerialTimer3IntrHandlers(void);
void SetVBlankCounter1Ptr(u32 *ptr);
void DisableVBlankCounter1(void);
void StartTimer1(void);
void SeedRngAndSetTrainerId(void);
u16 GetGeneratedTrainerIdLower(void);

#define GAME_CODE_LENGTH 4
extern const char RomHeaderGameCode[GAME_CODE_LENGTH];
extern const char RomHeaderSoftwareVersion;

extern u8 gLinkTransferringData;
extern u16 gKeyRepeatStartDelay;
