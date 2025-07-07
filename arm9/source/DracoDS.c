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

#include <nds.h>
#include <nds/fifomessages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fat.h>
#include <maxmod9.h>

#include "DracoDS.h"
#include "DracoUtils.h"
#include "dragon_kbd.h"
#include "coco_kbd.h"
#include "cassette.h"
#include "top_dragon.h"
#include "top_coco.h"
#include "mainmenu.h"
#include "soundbank.h"
#include "soundbank_bin.h"
#include "screenshot.h"
#include "cpu.h"
#include "pia.h"
#include "sam.h"

#include "printf.h"

// -----------------------------------------------------------------
// Most handy for development of the emulator is a set of 16 R/W
// registers and a couple of index vars... we show this when
// global settings is set to show the 'Debugger'. It's amazing
// how handy these general registers are for emulation development.
// -----------------------------------------------------------------
u32 debug[0x10]={0};
u32 DX = 0;
u32 DY = 0;

u8 DragonBASIC[0x4000]        = {0};  // We keep the 16k Dragon 32 BASIC/BIOS here
u8 CoCoBASIC[0x4000]          = {0};  // We keep the 16k Tandy CoCo BASIC/BIOS here (two 8K roms)
u8 DiskROM[0x4000]            = {0};  // We keep the 8/16k Disk ROM here

u8 TapeCartDiskBuffer[MAX_FILE_SIZE];     // This is where we keep the raw untouched file as read from the SD card

// ----------------------------------------------------------------------------
// We track the most recent directory and file loaded... both the initial one
// (for the CRC32) and subsequent additional tape loads (Side 2, Side B, etc)
// ----------------------------------------------------------------------------
static char cmd_line_file[256];
char initial_file[MAX_FILENAME_LEN] = "";
char initial_path[MAX_FILENAME_LEN] = "";
char last_path[MAX_FILENAME_LEN]    = "";
char last_file[MAX_FILENAME_LEN]    = "";

// --------------------------------------------------
// A few housekeeping vars to help with emulation...
// --------------------------------------------------
u8 bFirstTime        = 1;
u8 bottom_screen     = 0;

// ---------------------------------------------------------------------------
// Some timing and frame rate comutations to keep the emulation on pace...
// ---------------------------------------------------------------------------
u16 emuFps          __attribute__((section(".dtcm"))) = 0;
u16 emuActFrames    __attribute__((section(".dtcm"))) = 0;
u16 timingFrames    __attribute__((section(".dtcm"))) = 0;

u16 joy_x           __attribute__((section(".dtcm"))) = 0;
u16 joy_y           __attribute__((section(".dtcm"))) = 0;
u16 joy_dampen      __attribute__((section(".dtcm"))) = 0;

#define JOY_CENTER  33

// ----------------------------------------------------------------------------------
// For the various BIOS files - we must have the CoCo and Dragon BASIC files to run
// ----------------------------------------------------------------------------------
u8 bBIOS_found      = false;
u8 bDISKBIOS_found  = false;

u8 soundEmuPause     __attribute__((section(".dtcm"))) = 1;       // Set to 1 to pause (mute) sound, 0 is sound unmuted (sound channels active)

// -----------------------------------------------------------------------------------------------
// This set of critical vars is what determines the machine type -
// -----------------------------------------------------------------------------------------------
u8 draco_mode       __attribute__((section(".dtcm"))) = 0;       // See defines for the various modes...
u8 kbd_key           __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the ASCII key (e.g. 'A', 'B', '3', etc)
u16 nds_key          __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the NDS keys from keysCurrent() or similar
u8 last_mapped_key   __attribute__((section(".dtcm"))) = 0;       // The last mapped key which has been pressed - used for key click feedback
u8 kbd_keys_pressed  __attribute__((section(".dtcm"))) = 0;       // Each frame we check for keys pressed - since we can map keyboard keys to the NDS, there may be several pressed at once
u8 kbd_keys[12]      __attribute__((section(".dtcm")));           // Up to 12 possible keys pressed at the same time (we have 12 NDS physical buttons though it's unlikely that more than 2 or maybe 3 would be pressed)

u8 bStartSoundEngine = 0;      // Set to true to unmute sound after 1 frame of rendering...
int bg0, bg1, bg0b, bg1b;      // Some vars for NDS background screen handling
u16 vusCptVBL = 0;             // We use this as a basic timer for the Mario sprite... could be removed if another timer can be utilized
u8 touch_debounce = 0;         // A bit of touch-screen debounce
u8 key_debounce = 0;           // A bit of key debounce to ensure the key is held pressed for a minimum amount of time

// The DS/DSi has 12 keys that can be mapped
u16 NDS_keyMap[12] __attribute__((section(".dtcm"))) = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_X, KEY_Y, KEY_R, KEY_L, KEY_START, KEY_SELECT};

// ----------------------------------------------------------------------
// The key map for the Dragon and Tandy... mapped into the NDS controller
// We allow mapping of the 5 joystick 'presses' (up, down, left, right
// and fire) along with all 53 of the possible Dragon/CoCo keyboard keys.
// ----------------------------------------------------------------------
u16 keyCoresp[MAX_KEY_OPTIONS] __attribute__((section(".dtcm"))) = {
    JST_UP,     //0
    JST_DOWN,
    JST_LEFT,
    JST_RIGHT,
    JST_FIRE,
};

static char tmp[64];    // For various sprintf() calls

// ------------------------------------------------------------
// Utility function to pause the sound...
// ------------------------------------------------------------
void SoundPause(void)
{
    soundEmuPause = 1;
}

// ------------------------------------------------------------
// Utility function to un pause the sound...
// ------------------------------------------------------------
void SoundUnPause(void)
{
    soundEmuPause = 0;
}

// --------------------------------------------------------------------------------------------
// MAXMOD streaming setup and handling...
// We were using the normal ARM7 sound core but it sounded "scratchy" and so with the help
// of FluBBa, we've swiched over to the maxmod sound core which performs much better.
// --------------------------------------------------------------------------------------------
#define sample_rate         (31400)    // To roughly match how many samples (2x per scanline x 262 scanlines x 60 frames... or 2x312x50 = 31200)
#define buffer_size         (512+16)   // Enough buffer that we don't have to fill it too often. Must be multiple of 16.

mm_ds_system sys   __attribute__((section(".dtcm")));
mm_stream myStream __attribute__((section(".dtcm")));

#define WAVE_DIRECT_BUF_SIZE 4095
u16 mixer_read      __attribute__((section(".dtcm"))) = 0;
u16 mixer_write     __attribute__((section(".dtcm"))) = 0;
s16 mixer[WAVE_DIRECT_BUF_SIZE+1];


// The games normally run at the proper 100% speed, but user can override from 80% to 120%
u16 GAME_SPEED_PAL[]  __attribute__((section(".dtcm"))) = {655, 596, 547, 728, 818 };
u16 GAME_SPEED_NTSC[] __attribute__((section(".dtcm"))) = {546, 497, 455, 420, 607 };

// -------------------------------------------------------------------------------------------
// maxmod will call this routine when the buffer is half-empty and requests that
// we fill the sound buffer with more samples. They will request 'len' samples and
// we will fill exactly that many. If the sound is paused, we fill with 'mute' samples.
// -------------------------------------------------------------------------------------------
s16 last_sample __attribute__((section(".dtcm"))) = 0;
int breather    __attribute__((section(".dtcm"))) = 0;
extern u16 dac_output;
ITCM_CODE mm_word OurSoundMixer(mm_word len, mm_addr dest, mm_stream_formats format)
{
    if (soundEmuPause)  // If paused, just "mix" in mute sound chip... all channels are OFF
    {
        s16 *p = (s16*)dest;
        for (int i=0; i<len; i++)
        {
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
        }
    }
    else
    {
        s16 *p = (s16*)dest;
        for (int i=0; i<len*2; i++)
        {
            if (mixer_read == mixer_write)
            {
                processDirectAudio();
            }
            last_sample = mixer[mixer_read];
            *p++ = last_sample;
            mixer_read = (mixer_read + 1) & WAVE_DIRECT_BUF_SIZE;
        }
        if (breather) {breather -= (len*2); if (breather < 0) breather = 0;}
    }

    return  len;
}

// --------------------------------------------------------------------------------------------
// This is called when we want to sample the audio directly - we grab 4x sound samples.
// --------------------------------------------------------------------------------------------
s16 mixbufAY[4]  __attribute__((section(".dtcm")));
s16 beeper_vol   __attribute__((section(".dtcm"))) = 0x0000;
s16 last_dac     __attribute__((section(".dtcm"))) = 0;
ITCM_CODE void processDirectAudio(void)
{
    for (u8 i=0; i<4; i++)
    {
        if (breather) {return;}
        if (pia_is_audio_dac_enabled())
        {
            last_dac = dac_output*256;
        }

        mixer[mixer_write] = beeper_vol + last_dac;
        mixer_write++; mixer_write &= WAVE_DIRECT_BUF_SIZE;
        if (((mixer_write+1)&WAVE_DIRECT_BUF_SIZE) == mixer_read) {breather = 2048;}
    }
}

// -----------------------------------------------------------------------------------------------
// The user can override the core emulation speed from 80% to 120% to make games play faster/slow
// than normal. We must adjust the MaxMode sample frequency to match or else we will not have the
// proper number of samples in our sound buffer... this isn't perfect but it's reasonably good!
// -----------------------------------------------------------------------------------------------
static u8 last_game_speed = 0;
static u32 sample_rate_adjust[] = {100, 110, 120, 90, 80};
void newStreamSampleRate(void)
{
    if (last_game_speed != myConfig.gameSpeed)
    {
        last_game_speed = myConfig.gameSpeed;
        mmStreamClose();

        // Adjust the sample rate to match the core emulation speed... user can override from 80% to 120%
        int new_sample_rate     = (sample_rate * sample_rate_adjust[myConfig.gameSpeed]) / 100;
        myStream.sampling_rate  = new_sample_rate;        // sample_rate to match the Dragon/Tandy scanline emulation
        myStream.buffer_length  = buffer_size;            // buffer length = (512+16)
        myStream.callback       = OurSoundMixer;          // set callback function
        myStream.format         = MM_STREAM_16BIT_STEREO; // format = stereo 16-bit
        myStream.timer          = MM_TIMER0;              // use hardware timer 0
        myStream.manual         = false;                  // use automatic filling
        mmStreamOpen(&myStream);
    }
}

// -------------------------------------------------------------------------------------------
// Setup the maxmod audio stream - we use this to render the 6-bit DAC + 1-bit Beeper audio
// -------------------------------------------------------------------------------------------
void setupStream(void)
{
  //----------------------------------------------------------------
  //  initialize maxmod with our small 3-effect soundbank
  //----------------------------------------------------------------
  mmInitDefaultMem((mm_addr)soundbank_bin);

  mmLoadEffect(SFX_CLICKNOQUIT);
  mmLoadEffect(SFX_KEYCLICK);
  mmLoadEffect(SFX_MUS_INTRO);
  mmLoadEffect(SFX_FLOPPY);

  //----------------------------------------------------------------
  //  open stream
  //----------------------------------------------------------------
  myStream.sampling_rate  = sample_rate;            // sample_rate for the Dragon/Tandy emulation
  myStream.buffer_length  = buffer_size;            // buffer length = (512+16)
  myStream.callback       = OurSoundMixer;          // set callback function
  myStream.format         = MM_STREAM_16BIT_STEREO; // format = stereo 16-bit
  myStream.timer          = MM_TIMER0;              // use hardware timer 0
  myStream.manual         = false;                  // use automatic filling
  mmStreamOpen(&myStream);

  //----------------------------------------------------------------
  //  when using 'automatic' filling, your callback will be triggered
  //  every time half of the wave buffer is processed.
  //
  //  so:
  //  25000 (rate)
  //  ----- = ~21 Hz for a full pass, and ~42hz for half pass
  //  1200  (length)
  //----------------------------------------------------------------
  //  with 'manual' filling, you must call mmStreamUpdate
  //  periodically (and often enough to avoid buffer underruns)
  //----------------------------------------------------------------
}

void sound_chip_reset()
{
  memset(mixer,   0x00, sizeof(mixer));
  mixer_read=0;
  mixer_write=0;
}

// -----------------------------------------------------------------------
// We setup the sound chips - disabling all volumes to start.
// -----------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void)
{
  SoundPause();             // Pause any sound output
  sound_chip_reset();       // Reset the sound generator
  setupStream();            // Setup maxmod stream...
  bStartSoundEngine = 5;    // Volume will 'unpause' after 5 frames in the main loop.
}

//*****************************************************************************
// Reset the Dragon/Tandy - mostly CPU and Memory plus some joystick/IO
//*****************************************************************************

// --------------------------------------------------------------
// When we first load a ROM/CASSETTE or when the user presses
// the RESET button on the touch-screen...
// --------------------------------------------------------------
void ResetDragonTandy(void)
{
  JoyState = 0x00000000;                // Nothing pressed to start

  sound_chip_reset();                   // Reset the sound generator
  dragon_reset();                       // Reset the Dragon/Tandy emulation

  // -----------------------------------------------------------
  // Timer 1 is used to time frame-to-frame of actual emulation
  // -----------------------------------------------------------
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;

  // -----------------------------------------------------------
  // Timer 2 is used to time once per second events
  // -----------------------------------------------------------
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;

  bFirstTime = 1;
  bottom_screen = 0;

  joy_x = joy_y = JOY_CENTER;
  joy_dampen = 0;
}


// Show debug registers...
void ShowDebugger(void)
{
    u8 idx = 1;
    for (u8 i=0; i<4; i++)
    {
        sprintf(tmp, "D%d %-7ld %04lX  D%d %-7ld %04lX", i, (s32)debug[i], (debug[i] < 0xFFFF ? debug[i]:0xFFFF), 4+i, (s32)debug[4+i], (debug[4+i] < 0xFFFF ? debug[4+i]:0xFFFF));
        DSPrint(0, idx++, 0, tmp);
    }
}


// ------------------------------------------------------------
// The status line shows the status of the Emulation System
// on the top line of the bottom DS display.
// ------------------------------------------------------------
void DisplayStatusLine(void)
{
    extern u8 io_show_status, shift_key;
    
    DSPrint(29,0,2, (sam_registers.memory_map_type ? "32K": "64K"));

    if (draco_mode == MODE_DSK)
    {
        if (io_show_status)
        {
            if (io_show_status) --io_show_status;
            // Show disk in green (read/write)
            DSPrint(27, 21, 2, ",-.");
            DSPrint(27, 22, 2, "LMN");
            DSPrint(27, 23, 2, "OPQ");
            if (io_show_status >= 3) mmEffect(SFX_FLOPPY);
        }
        else
        {
            // Show disk in white (no activity)
            DSPrint(27, 21, 2, "'()");
            DSPrint(27, 22, 2, "GHI");
            DSPrint(27, 23, 2, "OPQ");
        }
    }
    else
    {
        if (tape_motor)
        {
            char tmp[5];
            // Show cassette in green (playing)
            DSPrint(27, 21, 2, "$%&");
            DSPrint(27, 22, 2, "DEF");
            
            sprintf(tmp, "%03d", tape_pos/1024); // Tape Counter in 1K increments
            DSPrint(27, 23, 6, tmp);
        }
        else
        {
            // Show cassette in white (stopped)
            DSPrint(27, 21, 2, "!\"#");
            DSPrint(27, 22, 2, "ABC");
            DSPrint(27, 23, 6, "   ");
        }
    }
    
    if (shift_key)
    {
        DSPrint(1, 19, 2, "@");
    }
    else
    {
        DSPrint(1, 19, 6, " ");
    }
}


// ------------------------------------------------------------------------
// Show the Mini Menu - highlight the selected row. This can be called
// up directly from the Keyboard Graphic - allows the user to quit
// the current game, set high scores, save/load game state, etc.
// ------------------------------------------------------------------------
u8 mini_menu_items = 0;
void MiniMenuShow(bool bClearScreen, u8 sel)
{
    mini_menu_items = 0;
    if (bClearScreen)
    {
      // ---------------------------------------------------
      // Put up a generic background for this mini-menu...
      // ---------------------------------------------------
      BottomScreenOptions();
    }

    DSPrint(8,7,6,                                           " DS MINI MENU  ");
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " RESET  GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " QUIT   GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " SAVE   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " LOAD   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " GAME   OPTIONS");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " DEFINE KEYS   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " EXIT   MENU   ");  mini_menu_items++;
}

// ------------------------------------------------------------------------
// Handle mini-menu interface...
// ------------------------------------------------------------------------
u8 MiniMenu(void)
{
  u8 retVal = MENU_CHOICE_NONE;
  u8 menuSelection = 0;

  SoundPause();
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  MiniMenuShow(true, menuSelection);

  while (true)
  {
    nds_key = keysCurrent();
    if (nds_key)
    {
        if (nds_key & KEY_UP)
        {
            menuSelection = (menuSelection > 0) ? (menuSelection-1):(mini_menu_items-1);
            MiniMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_DOWN)
        {
            menuSelection = (menuSelection+1) % mini_menu_items;
            MiniMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_A)
        {
            if      (menuSelection == 0) retVal = MENU_CHOICE_RESET_GAME;
            else if (menuSelection == 1) retVal = MENU_CHOICE_END_GAME;
            else if (menuSelection == 2) retVal = MENU_CHOICE_SAVE_GAME;
            else if (menuSelection == 3) retVal = MENU_CHOICE_LOAD_GAME;
            else if (menuSelection == 4) retVal = MENU_CHOICE_GAME_OPTION;
            else if (menuSelection == 5) retVal = MENU_CHOICE_DEFINE_KEYS;
            else if (menuSelection == 6) retVal = MENU_CHOICE_NONE;
            else retVal = MENU_CHOICE_NONE;
            break;
        }
        if (nds_key & KEY_B)
        {
            retVal = MENU_CHOICE_NONE;
            break;
        }

        while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
        WAITVBL;WAITVBL;
    }
  }

  while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
  WAITVBL;WAITVBL;

  if (retVal == MENU_CHOICE_NONE)
  {
    BottomScreenKeyboard();  // Could be generic or overlay...
  }

  SoundUnPause();

  return retVal;
}


// -------------------------------------------------------------------------
// Keyboard handler - mapping DS touch screen virtual keys to keyboard keys
// that we can feed into the key processing handler in spectrum.c when the
// IO port is read.
// -------------------------------------------------------------------------

u8 shift_key = 0;
u8 last_kbd_key = 0;

u8 handle_keyboard_press(u16 iTx, u16 iTy)  // Dragon/Tandy keyboard
{
    if ((iTy >= 40) && (iTy < 72))   // Row 1 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = 31;
        else if ((iTx >= 23)  && (iTx < 44))   kbd_key = 32;
        else if ((iTx >= 44)  && (iTx < 65))   kbd_key = 33;
        else if ((iTx >= 65)  && (iTx < 86))   kbd_key = 34;
        else if ((iTx >= 88)  && (iTx < 107))  kbd_key = 35;
        else if ((iTx >= 107) && (iTx < 128))  kbd_key = 36;
        else if ((iTx >= 128) && (iTx < 149))  kbd_key = 37;
        else if ((iTx >= 149) && (iTx < 170))  kbd_key = 38;
        else if ((iTx >= 170) && (iTx < 191))  kbd_key = 39;
        else if ((iTx >= 191) && (iTx < 212))  kbd_key = 40;
        else if ((iTx >= 212) && (iTx < 233))  kbd_key = 41; // Dash
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = 50; // Up Arrow
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 2 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = 21;
        else if ((iTx >= 23)  && (iTx < 44))   kbd_key = 27;
        else if ((iTx >= 44)  && (iTx < 65))   kbd_key = 9;
        else if ((iTx >= 65)  && (iTx < 86))   kbd_key = 22;
        else if ((iTx >= 88)  && (iTx < 107))  kbd_key = 24;
        else if ((iTx >= 107) && (iTx < 128))  kbd_key = 29;

        else if ((iTx >= 128) && (iTx < 149))  kbd_key = 25;
        else if ((iTx >= 149) && (iTx < 170))  kbd_key = 13;
        else if ((iTx >= 170) && (iTx < 191))  kbd_key = 19;
        else if ((iTx >= 191) && (iTx < 212))  kbd_key = 20;
        else if ((iTx >= 212) && (iTx < 233))  kbd_key = 47; // AT Sign
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = 53; // Down Arrow
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 3 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = 5;
        else if ((iTx >= 23)  && (iTx < 44))   kbd_key = 23;
        else if ((iTx >= 44)  && (iTx < 65))   kbd_key = 8;
        else if ((iTx >= 65)  && (iTx < 86))   kbd_key = 10;
        else if ((iTx >= 88)  && (iTx < 107))  kbd_key = 11;
        else if ((iTx >= 107) && (iTx < 128))  kbd_key = 12;
        else if ((iTx >= 128) && (iTx < 149))  kbd_key = 14;
        else if ((iTx >= 149) && (iTx < 170))  kbd_key = 15;
        else if ((iTx >= 170) && (iTx < 191))  kbd_key = 16;
        else if ((iTx >= 191) && (iTx < 212))  kbd_key = 45;
        else if ((iTx >= 212) && (iTx < 233))  kbd_key = 48; // Enter
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = 48; // Enter
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 4 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   {kbd_key = 55; shift_key = 55;} // Shift
        else if ((iTx >= 23)  && (iTx < 44))   kbd_key = 30; // Z
        else if ((iTx >= 44)  && (iTx < 65))   kbd_key = 28;
        else if ((iTx >= 65)  && (iTx < 86))   kbd_key = 7;
        else if ((iTx >= 88)  && (iTx < 107))  kbd_key = 26;
        else if ((iTx >= 107) && (iTx < 128))  kbd_key = 6;
        else if ((iTx >= 128) && (iTx < 149))  kbd_key = 18;
        else if ((iTx >= 149) && (iTx < 170))  kbd_key = 17; // M
        else if ((iTx >= 170) && (iTx < 191))  kbd_key = 42; // Comma
        else if ((iTx >= 191) && (iTx < 212))  kbd_key = 43; // Period
        else if ((iTx >= 212) && (iTx < 233))  kbd_key = 46; // Slash
        else if ((iTx >= 233) && (iTx < 255))  kbd_key = 44; // Colon
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 5 (SPACE BAR and icons row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = 51; // Left Arrow
        else if ((iTx >= 23)  && (iTx < 43))   kbd_key = 52; // Right Arrow
        else if ((iTx >= 43)  && (iTx < 62))   kbd_key = 56; // Break
        else if ((iTx >= 62)  && (iTx < 82))   kbd_key = 54; // Clear
        else if ((iTx >= 82)  && (iTx < 201))  kbd_key = 49; // Space
        else if ((iTx >= 201) && (iTx < 255))  return MENU_CHOICE_MENU;
    }

    DisplayStatusLine();

    return MENU_CHOICE_NONE;
}


u8 __attribute__((noinline)) handle_meta_key(u8 meta_key)
{
    switch (meta_key)
    {
        case MENU_CHOICE_RESET_GAME:
            SoundPause();
            // Ask for verification
            if (showMessage("DO YOU REALLY WANT TO", "RESET THE CURRENT GAME ?") == ID_SHM_YES)
            {
                ResetDragonTandy();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_END_GAME:
              SoundPause();
              //  Ask for verification
              if  (showMessage("DO YOU REALLY WANT TO","QUIT THE CURRENT GAME ?") == ID_SHM_YES)
              {
                  memset((u8*)0x06000000, 0x00, 0x20000);    // Reset VRAM to 0x00 to clear any potential display garbage on way out
                  return 1;
              }
              BottomScreenKeyboard();
              DisplayStatusLine();
              SoundUnPause();
            break;

        case MENU_CHOICE_SAVE_GAME:
            SoundPause();
            if  (showMessage("DO YOU REALLY WANT TO","SAVE GAME STATE ?") == ID_SHM_YES)
            {
                DracoSaveState();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_LOAD_GAME:
            SoundPause();
            if (showMessage("DO YOU REALLY WANT TO","LOAD GAME STATE ?") == ID_SHM_YES)
            {
                DracoLoadState();
            }
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_DEFINE_KEYS:
            SoundPause();
            DracoDSChangeKeymap();
            BottomScreenKeyboard();
            SoundUnPause();
            break;

        case MENU_CHOICE_GAME_OPTION:
            SoundPause();
            DracoDSGameOptions(0);
            BottomScreenKeyboard();
            SoundUnPause();
            break;
            break;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// Slide-n-Glide D-pad keeps moving in the last known direction for a few more
// frames to help make those hairpin turns up and off ladders much easier...
// ----------------------------------------------------------------------------
u8 slide_n_glide_key_up = 0;
u8 slide_n_glide_key_down = 0;
u8 slide_n_glide_key_left = 0;
u8 slide_n_glide_key_right = 0;

// ------------------------------------------------------------------------
// The main emulation loop is here... call into the Z80 and render frame
// ------------------------------------------------------------------------
void DracoDS_main(void)
{
  u16 iTx,  iTy;
  u32 ucDEUX = 0x0000;
  static u8 dampenClick = 0;
  u8 meta_key = 0;

  // Setup the debug buffer for DSi use
  debug_init();

  // Get the Emulator ready
  DragonTandyInit(gpFic[ucGameAct].szName);

  DragonTandySetPalette();
  DragonTandyRun();

  // Frame-to-frame timing...
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;

  // Once/second timing...
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;

  newStreamSampleRate();

  // Force the sound engine to turn on when we start emulation
  bStartSoundEngine = 10;

  bFirstTime = 1;

  // -----------------------------------------------------------
  // Stay in this loop running the game until the user exits...
  // -----------------------------------------------------------
  while(1)
  {
    // Take a tour of the Z80 counter and display the screen if necessary
    if (dragon_run())
    {
        // If we've been asked to start the sound engine, rock-and-roll!
        if (bStartSoundEngine)
        {
              if (--bStartSoundEngine == 0) SoundUnPause();
        }

        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= (tape_motor ? 16364 : 32728))   //  1000MS (1 sec)
        {
            char szChai[4];

            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
            emuFps = emuActFrames;
            if (myGlobalConfig.showFPS)
            {
                if (emuFps/100) szChai[0] = '0' + emuFps/100;
                else szChai[0] = ' ';
                szChai[1] = '0' + (emuFps%100) / 10;
                szChai[2] = '0' + (emuFps%100) % 10;
                szChai[3] = 0;
                DSPrint(0,0,6,szChai);
            }
            DisplayStatusLine();
            emuActFrames = 0;
        }
        emuActFrames++;

        // ---------------------------------------------
        // We support Tandy at 60Hz and Dragon at 50Hz
        // ---------------------------------------------
        if (++timingFrames == (myConfig.machine ? 60:50))
        {
            TIMER2_CR=0;
            TIMER2_DATA=0;
            TIMER2_CR=TIMER_ENABLE | TIMER_DIV_1024;
            timingFrames = 0;
        }

        // ----------------------------------------------------------------------
        // 32,728.5 ticks of TIMER2 = 1 second
        // 1 frame = 1/50 or 655 ticks of TIMER2
        //
        // This is how we time frame-to frame to keep the game running at 50FPS
        // ----------------------------------------------------------------------
        while (TIMER2_DATA < (myConfig.machine ? GAME_SPEED_NTSC[myConfig.gameSpeed] : GAME_SPEED_PAL[myConfig.gameSpeed]) *(timingFrames+1))
        {
            if (myGlobalConfig.showFPS == 2) break;   // If Full Speed, break out...
            if (tape_motor) break; // If running TAPE go full speed
        }


        // If the Z80 Debugger is enabled, call it
        if (myGlobalConfig.debugger)
        {
            ShowDebugger();
        }

        uint16_t keys_current = keysCurrent();

        // ------------------------------------------------------------------------------------
        // The first time we press KEY_START, we might be loading up the Cassette or Cartridge
        // ------------------------------------------------------------------------------------
        if (bFirstTime && myConfig.autoLoad)
        {
            if (draco_mode == MODE_CART)
            {
                bFirstTime = 0;
                pia_cart_firq();
            }
            else if (draco_mode == MODE_CAS)
            {
                // START key is also special...
                if (keys_current & KEY_START)
                {
                    bFirstTime = 0;
                    BufferKey(7);     // C
                    BufferKey(16);    // L
                    BufferKey(19);    // O
                    BufferKey(5);     // A
                    BufferKey(8);     // D
                    if (myConfig.autoLoad == 1)
                    {
                        BufferKey(17);    // M
                    }
                    BufferKey(44);    // :
                    
                    BufferKey(9);     // E
                    BufferKey(28);    // X
                    BufferKey(9);     // E
                    BufferKey(7);     // C
                    
                    BufferKey(48);    // ENTER
                    BufferKey(255);   // END
                }
            }
            else if (draco_mode == MODE_DSK)
            {
                // START key is also special...
                if (keys_current & KEY_START)
                {
                    bFirstTime = 0;
                    BufferKey(8);     // D
                    BufferKey(13);    // I
                    BufferKey(22);    // R
                    BufferKey(48);    // ENTER
                    BufferKey(255);   // END
                }
            }
        }

      // --------------------------------------------------------------
      // Hold the key press for a brief instant... To allow the
      // emulated CPU to 'see' the key briefly... Good enough.
      // --------------------------------------------------------------
      if (BufferedKeysReadIdx == BufferedKeysWriteIdx)
      {
          if (key_debounce > 0) key_debounce--;
          else
          {
              // -----------------------------------------------------------
              // This is where we accumualte the keys pressed... up to 12!
              // -----------------------------------------------------------
              kbd_keys_pressed = 0;
              memset(kbd_keys, 0x00, sizeof(kbd_keys));
              kbd_key = 0;

              // ------------------------------------------
              // Handle any screen touch events
              // ------------------------------------------
              if  (keys_current & KEY_TOUCH)
              {
                  // ------------------------------------------------------------------------------------------------
                  // Just a tiny bit of touch debounce so ensure touch screen is pressed for a fraction of a second.
                  // ------------------------------------------------------------------------------------------------
                  if (++touch_debounce > 1)
                  {
                    touchPosition touch;
                    touchRead(&touch);
                    iTx = touch.px;
                    iTy = touch.py;

                    // ------------------------------------------------------------
                    // Test the touchscreen for various full keyboard handlers...
                    // ------------------------------------------------------------
                    meta_key = handle_keyboard_press(iTx, iTy);

                    // If the special menu key indicates we should show the choice menu, do so here...
                    if (meta_key == MENU_CHOICE_MENU)
                    {
                        meta_key = MiniMenu();
                    }

                    // -------------------------------------------------------------------
                    // If one of the special meta keys was picked, we handle that here...
                    // -------------------------------------------------------------------
                    if (handle_meta_key(meta_key)) return;

                    if (++dampenClick > 0)  // Make sure the key is pressed for an appreciable amount of time...
                    {
                        if (kbd_key != 0)
                        {
                            if (shift_key && (kbd_key != shift_key)) // First non-shift key pairs with shift
                            {
                                kbd_keys[kbd_keys_pressed++] = shift_key; // Shift
                                shift_key = 0;
                            }
                            kbd_keys[kbd_keys_pressed++] = kbd_key;
                            key_debounce = 5;
                            if (last_kbd_key == 0)
                            {
                                 mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
                            }
                            last_kbd_key = kbd_key;
                        }
                    }
                  }
              } //  SCR_TOUCH
              else
              {
                touch_debounce = 0;
                dampenClick = 0;
                last_kbd_key = 0;
              }

              // ---------------------------------------------------------------------------
              //  Test DS keypresses (ABXY, L/R) and map to corresponding Dragon/Tandy keys
              // ---------------------------------------------------------------------------
              ucDEUX  = 0;
              nds_key  = keys_current;     // Get any current keys pressed on the NDS

              // -----------------------------------------
              // Check various key combinations first...
              // -----------------------------------------
              if ((nds_key & KEY_L) && (nds_key & KEY_R) && (nds_key & KEY_X))
              {
                    lcdSwap();
                    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
              }
              else if ((nds_key & KEY_L) && (nds_key & KEY_R) && (nds_key & KEY_Y))
              {
                    DSPrint(5,0,0,"SNAPSHOT");
                    screenshot();
                    debug_save();
                    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
                    DSPrint(5,0,0,"        ");
              }
              else if  (nds_key & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_R | KEY_L | KEY_X | KEY_Y))
              {
                  if (myConfig.dpad == DPAD_SLIDE_N_GLIDE) // CHUCKIE-EGG Style... hold left/right or up/down for a few frames
                  {
                        if (nds_key & KEY_UP)
                        {
                            slide_n_glide_key_up    = 12;
                            slide_n_glide_key_down  = 0;
                        }
                        if (nds_key & KEY_DOWN)
                        {
                            slide_n_glide_key_down  = 12;
                            slide_n_glide_key_up    = 0;
                        }
                        if (nds_key & KEY_LEFT)
                        {
                            slide_n_glide_key_left  = 12;
                            slide_n_glide_key_right = 0;
                        }
                        if (nds_key & KEY_RIGHT)
                        {
                            slide_n_glide_key_right = 12;
                            slide_n_glide_key_left  = 0;
                        }

                        if (slide_n_glide_key_up)
                        {
                            slide_n_glide_key_up--;
                            nds_key |= KEY_UP;
                        }

                        if (slide_n_glide_key_down)
                        {
                            slide_n_glide_key_down--;
                            nds_key |= KEY_DOWN;
                        }

                        if (slide_n_glide_key_left)
                        {
                            slide_n_glide_key_left--;
                            nds_key |= KEY_LEFT;
                        }

                        if (slide_n_glide_key_right)
                        {
                            slide_n_glide_key_right--;
                            nds_key |= KEY_RIGHT;
                        }
                  }

                  // --------------------------------------------------------------------------------------------------
                  // There are 12 NDS buttons (D-Pad, XYAB, L/R and Start+Select) - we allow mapping of any of these.
                  // --------------------------------------------------------------------------------------------------
                  for (u8 i=0; i<12; i++)
                  {
                      if (nds_key & NDS_keyMap[i])
                      {
                          if (myConfig.keymap[i] < 5)   // Joystick key map
                          {
                              ucDEUX  |= keyCoresp[myConfig.keymap[i]];
                          }
                          else if (myConfig.keymap[i] >= 60)   // Macro mapping
                          {
                              switch (myConfig.keymap[i])
                              {
                                  case 60:
                                    BufferKey(5); BufferKey(49); BufferKey(16); BufferKey(48);  // ATTACK LEFT
                                    break;
                                  case 61:
                                    BufferKey(5); BufferKey(49); BufferKey(22); BufferKey(48);  // ATTACK RIGHT
                                    break;
                                  case 62:
                                    BufferKey(17); BufferKey(48);  // MOVE
                                    break;
                                  case 63:
                                    BufferKey(24); BufferKey(49); BufferKey(16); BufferKey(48);  // TURN LEFT
                                    break;
                                  case 64:
                                    BufferKey(24); BufferKey(49); BufferKey(22); BufferKey(48);  // TURN LEFT
                                    break;
                              }
                          }
                          else // This is a keyboard maping... handle that here... just set the appopriate kbd_key
                          {
                              kbd_key = myConfig.keymap[i];
                              kbd_keys[kbd_keys_pressed++] = kbd_key;
                          }
                      }
                  }
              }
              else // No NDS keys pressed...
              {
                  if (slide_n_glide_key_up)    slide_n_glide_key_up--;
                  if (slide_n_glide_key_down)  slide_n_glide_key_down--;
                  if (slide_n_glide_key_left)  slide_n_glide_key_left--;
                  if (slide_n_glide_key_right) slide_n_glide_key_right--;
                  last_mapped_key = 0;
              }
          }
      }
      else
      {
          // ------------------------------------------------------------------------------------------
          // Finally, check if there are any buffered keys that need to go into the keyboard handling.
          // ------------------------------------------------------------------------------------------
          ProcessBufferedKeys();
      }

      // ---------------------------------------------------------
      // Accumulate all bits above into the Joystick State var...
      // ---------------------------------------------------------
      JoyState = ucDEUX;

      switch (myConfig.joyType)
      {
          case 0:  // Digital
            joy_x = joy_y = JOY_CENTER; // Self-centering
            if ( JoyState & JST_UP )    joy_y = 0;
            if ( JoyState & JST_DOWN)   joy_y = 64;
            if ( JoyState & JST_LEFT )  joy_x = 0;
            if ( JoyState & JST_RIGHT ) joy_x = 64;
            break;

          case 1:  // Analog Slow - every other frame
            if (timingFrames & 1)
            {
                if ( JoyState & JST_UP )    {if (joy_y > 1)  joy_y -= 1; else joy_y = 0;}
                if ( JoyState & JST_DOWN)   {if (joy_y < 64) joy_y += 1; else joy_y = 64;}
                if ( JoyState & JST_LEFT )  {if (joy_x > 1)  joy_x -= 1; else joy_x = 0;}
                if ( JoyState & JST_RIGHT ) {if (joy_x < 64) joy_x += 1; else joy_x = 64;}
            }
            break;

          case 2:  // Analog Medium
            if ( JoyState & JST_UP )    {if (joy_y > 1)  joy_y -= 1; else joy_y = 0;}
            if ( JoyState & JST_DOWN)   {if (joy_y < 64) joy_y += 1; else joy_y = 64;}
            if ( JoyState & JST_LEFT )  {if (joy_x > 1)  joy_x -= 1; else joy_x = 0;}
            if ( JoyState & JST_RIGHT ) {if (joy_x < 64) joy_x += 1; else joy_x = 64;}
            break;

          case 3:  // Analog Fast
            if ( JoyState & JST_UP )    {if (joy_y > 2)  joy_y -= 2; else joy_y = 0;}
            if ( JoyState & JST_DOWN)   {if (joy_y < 63) joy_y += 2; else joy_y = 64;}
            if ( JoyState & JST_LEFT )  {if (joy_x > 2)  joy_x -= 2; else joy_x = 0;}
            if ( JoyState & JST_RIGHT ) {if (joy_x < 63) joy_x += 2; else joy_x = 64;}
            break;

          case 4:  // Analog Slow - Self Center
            if (timingFrames & 1)
            {
                if ((JoyState & (JST_UP | JST_DOWN | JST_LEFT | JST_RIGHT | JST_FIRE)) == 0)
                {
                    if (joy_dampen)
                    {
                        if (--joy_dampen == 0)
                        {
                            joy_x = joy_y = JOY_CENTER; // Self-centering
                        }
                    }
                }
                if (JoyState & JST_UP )    {joy_dampen = 10; if (joy_y > 1)  joy_y -= 1; else joy_y = 0;}
                if (JoyState & JST_DOWN)   {joy_dampen = 10; if (joy_y < 64) joy_y += 1; else joy_y = 64;}
                if (JoyState & JST_LEFT)   {joy_dampen = 10; if (joy_x > 1)  joy_x -= 1; else joy_x = 0;}
                if (JoyState & JST_RIGHT)  {joy_dampen = 10; if (joy_x < 64) joy_x += 1; else joy_x = 64;}
            }
            break;

          case 5:  // Analog Medium - Self Center
            if ((JoyState & (JST_UP | JST_DOWN | JST_LEFT | JST_RIGHT | JST_FIRE)) == 0)
            {
                if (joy_dampen)
                {
                    if (--joy_dampen == 0)
                    {
                        joy_x = joy_y = JOY_CENTER; // Self-centering
                    }
                }
            }
            if (JoyState & JST_UP )    {joy_dampen = 20; if (joy_y > 1)  joy_y -= 1; else joy_y = 0;}
            if (JoyState & JST_DOWN)   {joy_dampen = 20; if (joy_y < 64) joy_y += 1; else joy_y = 64;}
            if (JoyState & JST_LEFT)   {joy_dampen = 20; if (joy_x > 1)  joy_x -= 1; else joy_x = 0;}
            if (JoyState & JST_RIGHT)  {joy_dampen = 20; if (joy_x < 64) joy_x += 1; else joy_x = 64;}
            break;

          case 6:  // Analog Fast - Self Center
            if ((JoyState & (JST_UP | JST_DOWN | JST_LEFT | JST_RIGHT | JST_FIRE)) == 0)
            {
                if (joy_dampen)
                {
                    if (--joy_dampen == 0)
                    {
                        joy_x = joy_y = JOY_CENTER; // Self-centering
                    }
                }
            }
            if ( JoyState & JST_UP )    {joy_dampen = 20; if (joy_y > 2)  joy_y -= 2; else joy_y = 0;}
            if ( JoyState & JST_DOWN)   {joy_dampen = 20; if (joy_y < 63) joy_y += 2; else joy_y = 64;}
            if ( JoyState & JST_LEFT )  {joy_dampen = 20; if (joy_x > 2)  joy_x -= 2; else joy_x = 0;}
            if ( JoyState & JST_RIGHT ) {joy_dampen = 20; if (joy_x < 63) joy_x += 2; else joy_x = 64;}
            break;
      }

      // --------------------------------------------------
      // Handle Auto-Fire if enabled in configuration...
      // --------------------------------------------------
      static u8 autoFireTimer=0;
      if (myConfig.autoFire && (JoyState & JST_FIRE))  // Fire Button
      {
         if ((++autoFireTimer & 7) > 4)  JoyState &= ~JST_FIRE;
      }
    }
  }
}


// ----------------------------------------------------------------------------------------
// We steal 256K of the VRAM to hold a shadow copy of the ROM cart for fast swap...
// ----------------------------------------------------------------------------------------
void useVRAM(void)
{
  vramSetBankB(VRAM_B_LCD);        // 128K VRAM used for snapshot DCAP buffer - but could be repurposed during emulation ...
  vramSetBankD(VRAM_D_LCD);        // Not using this for video but 128K of faster RAM always useful!  Mapped at 0x06860000 -   Unused - reserved for future use
  vramSetBankE(VRAM_E_LCD);        // Not using this for video but 64K of faster RAM always useful!   Mapped at 0x06880000 -   Unused - reserved for future use
  vramSetBankF(VRAM_F_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06890000 -   Save RAM
  vramSetBankG(VRAM_G_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06894000 -   Save ROM
  vramSetBankH(VRAM_H_LCD);        // Not using this for video but 32K of faster RAM always useful!   Mapped at 0x06898000 -   Unused - reserved for future use
  vramSetBankI(VRAM_I_LCD);        // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x068A0000 -   Unused - reserved for future use
}

/*********************************************************************************
 * Init DS Emulator - setup VRAM banks and background screen rendering banks
 ********************************************************************************/
void DracoDSInit(void)
{
  //  Init graphic mode (bitmap mode)
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE);
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE  | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankC(VRAM_C_SUB_BG);

  //  Stop blending effect of intro
  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;

  //  Render the top screen
  bg0 = bgInit(0, BgType_Text8bpp,  BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp,  BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);

  if (myGlobalConfig.defMachine)
  {
      decompress(top_cocoTiles,  bgGetGfxPtr(bg0), LZ77Vram);
      decompress(top_cocoMap,  (void*) bgGetMapPtr(bg0), LZ77Vram);
      dmaCopy((void*) top_cocoPal,(void*)  BG_PALETTE,256*2);
  }
  else
  {
      decompress(top_dragonTiles,  bgGetGfxPtr(bg0), LZ77Vram);
      decompress(top_dragonMap,  (void*) bgGetMapPtr(bg0), LZ77Vram);
      dmaCopy((void*) top_dragonPal,(void*)  BG_PALETTE,256*2);
  }
  unsigned  short dmaVal =*(bgGetMapPtr(bg0)+51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1),32*24*2);

  // Put up the options screen
  BottomScreenOptions();

  //  Find the files
  DracoDSFindFiles(0);
}

void BottomScreenOptions(void)
{
    swiWaitForVBlank();

    if (bottom_screen != 1)
    {
        // ---------------------------------------------------
        // Put up the options select screen background...
        // ---------------------------------------------------
        bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
        bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 29,0);
        bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);

        decompress(mainmenuTiles, bgGetGfxPtr(bg0b), LZ77Vram);
        decompress(mainmenuMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
        dmaCopy((void*) mainmenuPal,(void*) BG_PALETTE_SUB,256*2);

        unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
        dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    }
    else // Just clear the screen
    {
        for (u8 i=0; i<23; i++)  DSPrint(0,i,0,"                                ");
    }

    bottom_screen = 1;
}

// ---------------------------------------------------------------------------
// Setup the bottom screen - mostly for menu, high scores, options, etc.
// ---------------------------------------------------------------------------
void BottomScreenKeyboard(void)
{
    swiWaitForVBlank();

    //  Init bottom screen for Dragon/Tandy Keyboard
    if (myConfig.machine)
    {
        decompress(coco_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
        decompress(coco_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
        dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
        dmaCopy((void*) coco_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else
    {
        decompress(dragon_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
        decompress(dragon_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
        dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
        dmaCopy((void*) dragon_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }

    unsigned  short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);

    bottom_screen = 2;

    DisplayStatusLine();
}



void BottomScreenCassette(void)
{
    swiWaitForVBlank();

    // ---------------------------------------------------
    // Put up the cassette screen background...
    // ---------------------------------------------------
    bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
    bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 29,0);
    bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);

    decompress(cassetteTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(cassetteMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void*) cassettePal,(void*) BG_PALETTE_SUB,256*2);

    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

    bottom_screen = 3;
}


/*********************************************************************************
 * Init CPU for the current game
 ********************************************************************************/
void DracoDSInitCPU(void)
{
    // -----------------------------------------------
    // Init bottom screen do display the Keyboard
    // -----------------------------------------------
    BottomScreenKeyboard();
}

// -------------------------------------------------------------
// Only used for basic timing of splash screen fade-out
// -------------------------------------------------------------
ITCM_CODE void irqVBlank(void)
{
    // Manage time
    vusCptVBL++;
}

// ------------------------------
// Look for the BIOS/BASIC files
// ------------------------------
void LoadBIOSFiles(void)
{
    int size = 0;

    // -----------------------------------------------------
    // We will look for the various needed BASIC/BIOS roms
    // -----------------------------------------------------
    bBIOS_found = false;

    memset(DragonBASIC, 0xFF, sizeof(DragonBASIC));
    memset(CoCoBASIC,   0xFF, sizeof(CoCoBASIC));
    memset(DiskROM,     0xFF, sizeof(DiskROM));

    // ----------------------------------------------------
    // Try to load the Dragon 32 BIOS/BASIC file
    // ----------------------------------------------------
               size = ReadFileCarefully("dragon.rom",               DragonBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/dragon.rom",    DragonBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/dragon.rom",    DragonBASIC, 0x4000, 0);

    if (!size) size = ReadFileCarefully("dragon32.rom",             DragonBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/dragon32.rom",  DragonBASIC, 0x4000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/dragon32.rom",  DragonBASIC, 0x4000, 0);

    if (size) bBIOS_found = true;

    // ----------------------------------------------------
    // Try to load the Dragon 32 BIOS/BASIC file
    // ----------------------------------------------------
    if (bBIOS_found)
    {
                   size = ReadFileCarefully("coco.rom",                 CoCoBASIC, 0x4000, 0);
        if (!size) size = ReadFileCarefully("/roms/bios/coco.rom",      CoCoBASIC, 0x4000, 0);
        if (!size) size = ReadFileCarefully("/data/bios/coco.rom",      CoCoBASIC, 0x4000, 0);

        if (!size) size = ReadFileCarefully("coco2.rom",                CoCoBASIC, 0x4000, 0);
        if (!size) size = ReadFileCarefully("/roms/bios/coco2.rom",     CoCoBASIC, 0x4000, 0);
        if (!size) size = ReadFileCarefully("/data/bios/coco2.rom",     CoCoBASIC, 0x4000, 0);

        if (!size)
        {
                       size = ReadFileCarefully("extbas11.rom",             CoCoBASIC+0x0000, 0x2000, 0);
            if (!size) size = ReadFileCarefully("/roms/bios/extbas11.rom",  CoCoBASIC+0x0000, 0x2000, 0);
            if (!size) size = ReadFileCarefully("/data/bios/extbas11.rom",  CoCoBASIC+0x0000, 0x2000, 0);

            if (size)
            {
                           size = ReadFileCarefully("bas12.rom",                CoCoBASIC+0x2000, 0x2000, 0);
                if (!size) size = ReadFileCarefully("/roms/bios/bas12.rom",     CoCoBASIC+0x2000, 0x2000, 0);
                if (!size) size = ReadFileCarefully("/data/bios/bas12.rom",     CoCoBASIC+0x2000, 0x2000, 0);
            }
        }
    }
    
    if (size) bBIOS_found = true;
    
    // Optional Disk ROM
               size = ReadFileCarefully("disk11.rom",                 DiskROM, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/roms/bios/disk11.rom",      DiskROM, 0x2000, 0);
    if (!size) size = ReadFileCarefully("/data/bios/disk11.rom",      DiskROM, 0x2000, 0);    
    if (size) bDISKBIOS_found = true;
}

/************************************************************************************
 * Program entry point - check if an argument has been passed in probably from TWL++
 ***********************************************************************************/
int main(int argc, char **argv)
{
  //  Init sound
  consoleDemoInit();

  if  (!fatInitDefault()) {
     iprintf("Unable to initialize libfat!\n");
     return -1;
  }

  lcdMainOnTop();

  //  Init timer for frame management
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE|TIMER_DIV_1024;
  dsInstallSoundEmuFIFO();

  // -----------------------------------------------------------------
  // And do an initial load of configuration... We'll match it up
  // with the game that was selected later...
  // -----------------------------------------------------------------
  LoadConfig();

  //  Show the fade-away intro logo...
  intro_logo();

  SetYtrigger(190); //trigger 2 lines before vblank

  irqSet(IRQ_VBLANK,  irqVBlank);
  irqEnable(IRQ_VBLANK);

  // -----------------------------------------------------------------
  // Grab the BIOS before we try to switch any directories around...
  // -----------------------------------------------------------------
  useVRAM();
  LoadBIOSFiles();

  //  Handle command line argument... mostly for TWL++
  if  (argc > 1)
  {
      //  We want to start in the directory where the file is being launched...
      if  (strchr(argv[1], '/') != NULL)
      {
          static char  path[128];
          strcpy(path,  argv[1]);
          char  *ptr = &path[strlen(path)-1];
          while (*ptr !=  '/') ptr--;
          ptr++;
          strcpy(cmd_line_file,  ptr);
          *ptr=0;
          chdir(path);
      }
      else
      {
          strcpy(cmd_line_file,  argv[1]);
      }
  }
  else
  {
      cmd_line_file[0]=0; // No file passed on command line...

      if ((myGlobalConfig.lastDir == 2) && (strlen(myGlobalConfig.szLastPath) > 2))
      {
          chdir(myGlobalConfig.szLastPath);  // Try to start back where we last were...
      }
      else
      {
          chdir("/roms");       // Try to start in roms area... doesn't matter if it fails
          if (myGlobalConfig.lastDir == 0)
            chdir("dragon");      // And try to start in the proper subdir. Doesn't really matter if it fails.
          else
            chdir("coco");        // And try to start in the proper subdir. Doesn't really matter if it fails.
      }
  }

  SoundPause();

  srand(time(NULL));

  //  ------------------------------------------------------------
  //  We run this loop forever until game exit is selected...
  //  ------------------------------------------------------------
  while(1)
  {
    DracoDSInit();

    // ---------------------------------------------------------------
    // Let the user know what BIOS files were found - the only BIOS
    // that must exist is 48.rom or else the show is off...
    // ---------------------------------------------------------------
    if (!bBIOS_found)
    {
        DSPrint(2,10,0," ERROR: DRAGON32.ROM OR     ");
        DSPrint(2,12,0," BAS12.ROM AND EXTBAS11.ROM ");
        DSPrint(2,14,0," NOT FOUND. PLACE THESE IN  ");
        DSPrint(2,15,0," /ROMS/BIOS OR WITH EMULATOR");
        while(1) ;  // We're done... Need a bios to run this emulator
    }

    while(1)
    {
      SoundPause();
      //  Choose option
      if  (cmd_line_file[0] != 0)
      {
          ucGameChoice=0;
          ucGameAct=0;
          strcpy(gpFic[ucGameAct].szName, cmd_line_file);
          cmd_line_file[0] = 0;    // No more initial file...
          ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
      }
      else
      {
          DracoDSChangeOptions();
      }

      //  Run Machine
      DracoDSInitCPU();
      DracoDS_main();
    }
  }
  return(0);
}


// -----------------------------------------------------------------------
// The code below is a handy set of debug tools that allows us to
// write printf() like strings out to a file. Basically we accumulate
// the strings into a large RAM buffer and then when the L+R shoulder
// buttons are pressed and held, we will snapshot out the debug.log file.
// The DS-Lite only gets a small 16K debug buffer but the DSi gets 4MB!
// -----------------------------------------------------------------------

#define MAX_DPRINTF_STR_SIZE  256
u32     MAX_DEBUG_BUF_SIZE  = 0;

char *debug_buffer = 0;
u32  debug_len = 0;
extern char szName[]; // Reuse buffer which has no other in-game use

void debug_init()
{
    if (!debug_buffer)
    {
        if (isDSiMode())
        {
            MAX_DEBUG_BUF_SIZE = (1024*1024*2); // 2MB!!
            debug_buffer = malloc(MAX_DEBUG_BUF_SIZE);
        }
        else
        {
            MAX_DEBUG_BUF_SIZE = (1024*16);     // 16K only
            debug_buffer = malloc(MAX_DEBUG_BUF_SIZE);
        }
    }
    memset(debug_buffer, 0x00, MAX_DEBUG_BUF_SIZE);
    debug_len = 0;
}

void debug_printf(const char * str, ...)
{
    va_list ap = {0};

    va_start(ap, str);
    vsnprintf(szName, MAX_DPRINTF_STR_SIZE, str, ap);
    va_end(ap);

    strcat(debug_buffer, szName);
    debug_len += strlen(szName);
}

void debug_save()
{
    if (debug_len > 0) // Only if we have debug data to write...
    {
        FILE *fp = fopen("debug.log", "w");
        if (fp)
        {
            fwrite(debug_buffer, 1, debug_len, fp);
            fclose(fp);
        }
    }
}

// End of file
