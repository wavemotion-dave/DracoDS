// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave and eyalabraham
// (Dragon 32 emu core) are thanked profusely.
//
// The Draco-DS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================

#ifndef _DRACODS_H_
#define _DRACODS_H_

#include <nds.h>
#include <string.h>

extern u32 debug[0x10];
extern u32 DX, DY;

// These are the various special icons/menu operations
#define MENU_CHOICE_NONE        0x00
#define MENU_CHOICE_RESET_GAME  0x01
#define MENU_CHOICE_END_GAME    0x02
#define MENU_CHOICE_SAVE_GAME   0x03
#define MENU_CHOICE_LOAD_GAME   0x04
#define MENU_CHOICE_SWAP_DISK   0x05
#define MENU_CHOICE_DEFINE_KEYS 0x06
#define MENU_CHOICE_GAME_OPTION 0x07
#define MENU_CHOICE_MENU        0xFF        // Special brings up a mini-menu of choices

// ------------------------------------------------------------------------------
// Joystick UP, RIGHT, LEFT, DOWN and the FIRE button for the Kempston Joystick.
// Designed specifically so each has its own bit so we can press more than one
// direction/fire at the same time.  Keyboard keys are grafted onto this below.
// ------------------------------------------------------------------------------
#define JST_UP              0x0001
#define JST_RIGHT           0x0002
#define JST_DOWN            0x0004
#define JST_LEFT            0x0008
#define JST_FIRE            0x0010
#define JST_FIRE2           0x0020

#define MAX_KEY_OPTIONS     68

// What format is the input file?
#define MODE_CART           1
#define MODE_CAS            2
#define MODE_DSK            3

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

extern u8 draco_mode;
extern u8 kbd_keys_pressed;
extern u8 kbd_keys[12];
extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;
extern char initial_file[];
extern char initial_path[];
extern u16 nds_key;
extern u8  kbd_key;
extern u16 vusCptVBL;
extern u16 *pVidFlipBuf;
extern u16 keyCoresp[MAX_KEY_OPTIONS];
extern u16 NDS_keyMap[];
extern u8 soundEmuPause;
extern int bg0, bg1, bg0b, bg1b;
extern u32 last_file_size;
extern u8 bottom_screen;

extern void BottomScreenOptions(void);
extern void BottomScreenKeyboard(void);
extern void PauseSound(void);
extern void UnPauseSound(void);
extern void ResetStatusFlags(void);
extern void ReadFileCRCAndConfig(void);
extern void DisplayStatusLine(void);
extern void ResetDragonTandy(void);
extern void processDirectAudio(void);
extern void debug_init();
extern void debug_save();
extern void debug_printf(const char * str, ...);

#endif // _DRACODS_H_
