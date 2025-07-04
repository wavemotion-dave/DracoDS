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

#ifndef _DRACO_UTILS_H_
#define _DRACO_UTILS_H_
#include <nds.h>
#include "DracoDS.h"
#include "cpu.h"

#define MAX_FILES                   1048
#define MAX_FILENAME_LEN            160
#define MAX_FILE_SIZE               (256*1024) // 256K is big enough for any .CAS or .CCC file or standard (160K) .DSK file

#define MAX_CONFIGS                 1000
#define CONFIG_VERSION              0x0001

#define DRACO_FILE                  0x01
#define DIRECTORY                   0x02

#define ID_SHM_CANCEL               0x00
#define ID_SHM_YES                  0x01
#define ID_SHM_NO                   0x02

#define DPAD_NORMAL                 0
#define DPAD_SLIDE_N_GLIDE          1

extern unsigned char DragonBASIC[0x4000];
extern unsigned char CoCoBASIC[0x4000];

extern char last_path[MAX_FILENAME_LEN];
extern char last_file[MAX_FILENAME_LEN];

typedef struct {
  char szName[MAX_FILENAME_LEN+1];
  u8 uType;
  u32 uCrc;
} FIDraco;


extern u32 file_size;

extern uint32_t tape_pos;
extern uint16_t tape_motor;

typedef u16 word;

struct __attribute__((__packed__)) GlobalConfig_t
{
    u16 config_ver;
    u32 bios_checksums;
    char szLastFile[MAX_FILENAME_LEN+1];
    char szLastPath[MAX_FILENAME_LEN+1];
    char reserved1[MAX_FILENAME_LEN+1];
    char reserved2[MAX_FILENAME_LEN+1];
    u8  showFPS;
    u8  lastDir;
    u8  defMachine;
    u8  global_02;
    u8  global_03;
    u8  global_04;
    u8  global_05;
    u8  global_06;
    u8  global_07;
    u8  global_08;
    u8  global_09;
    u8  global_10;
    u8  global_11;
    u8  global_12;
    u8  debugger;
    u32 config_checksum;
};

struct __attribute__((__packed__)) Config_t
{
    u32 game_crc;
    u8  keymap[12];
    u8  machine;
    u8  autoLoad;
    u8  gameSpeed;
    u8  joystick;
    u8  autoFire;
    u8  joyType;
    u8  dpad;
    u8  forceCSS;
    u8  graphicsMode;
    u8  reserved4;
    u8  reserved5;
    u8  reserved6;
    u8  reserved7;
    u8  reserved8;
    u8  reserved9;
    u8  reserved10;
};

extern struct Config_t       myConfig;
extern struct GlobalConfig_t myGlobalConfig;

extern uint16_t joy_x;
extern uint16_t joy_y;

extern u8 last_special_key;
extern u8 last_special_key_dampen;

extern u16 JoyState;                    // Joystick / Paddle management

extern u32 file_crc;
extern u8 bFirstTime;
extern u8 show_tape_counter;

extern u8 BufferedKeys[32];
extern u8 BufferedKeysWriteIdx;
extern u8 BufferedKeysReadIdx;
extern u16 keyboard_interrupt;
extern u16 joystick_interrupt;
extern u8 bFlash;
extern u32 flash_timer;

extern u8 TapeCartDiskBuffer[MAX_FILE_SIZE];

extern FIDraco gpFic[MAX_FILES];
extern int uNbRoms;
extern int ucGameAct;
extern int ucGameChoice;

extern void LoadConfig(void);
extern u8   showMessage(char *szCh1, char *szCh2);
extern void DracoDSFindFiles(u8 bTapeOnly);
extern void DracoDSChangeOptions(void);
extern void DracoDSGameOptions(bool bIsGlobal);
extern void DSPrint(int iX,int iY,int iScr,char *szMessage);
extern u32  crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern u8   DracoDSLoadFile(u8 bTapeOnly);
extern void DisplayFileName(void);
extern void DisplayFileNameCassette(void);
extern u32  ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset);
extern u8   loadgame(const char *path);
extern u8   DragonTandyInit(char *szGame);
extern void DragonTandySetPalette(void);
extern void DragonTandyRun(void);
extern void tape_search_for_loader(void);
extern void tape_detect_loading(void);
extern void dragon_reset(void);
extern u32  dragon_run(void);
extern void getfile_crc(const char *path);
extern void DracoLoadState();
extern void DracoSaveState();
extern void intro_logo(void);
extern void BufferKey(u8 key);
extern void ProcessBufferedKeys(void);
extern void DracoDSChangeKeymap(void);

#endif // _DRACO_UTILS_H_
