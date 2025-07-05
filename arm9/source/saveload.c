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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <dirent.h>

#include "DracoDS.h"
#include "DracoUtils.h"
#include "CRC32.h"
#include "cpu.h"
#include "sam.h"
#include "pia.h"
#include "mem.h"
#include "printf.h"

#include "lzav.h"

#define DRACO_SAVE_VER   0x0001       // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

u8 CompressBuffer[128*1024];

char szLoadFile[MAX_FILENAME_LEN+1];
char tmpStr[34];

void DracoSaveState()
{
  size_t retVal;

  // Return to the original path
  chdir(initial_path);

  // Init filename = romname and SAV in place of ROM
  DIR* dir = opendir("sav");
  if (dir) closedir(dir);    // Directory exists... close it out and move on.
  else mkdir("sav", 0777);   // Otherwise create the directory...
  sprintf(szLoadFile,"sav/%s", initial_file);

  int len = strlen(szLoadFile);
  szLoadFile[len-3] = 's';
  szLoadFile[len-2] = 'a';
  szLoadFile[len-1] = 'v';

  strcpy(tmpStr,"SAVING...");
  DSPrint(3,0,0,tmpStr);

  FILE *handle = fopen(szLoadFile, "wb+");
  if (handle != NULL)
  {
    // Write Version
    u16 save_ver = DRACO_SAVE_VER;
    retVal = fwrite(&save_ver, sizeof(u16), 1, handle);

    // Write Last Directory Path / Tape File
    retVal = fwrite(&last_path, sizeof(last_path), 1, handle);
    retVal = fwrite(&last_file, sizeof(last_file), 1, handle);

    // Write Motorola 6809 CPU
    retVal = fwrite(&cpu, sizeof(cpu), 1, handle);

    // Write SAM registers
    retVal = fwrite(&sam_registers, sizeof(sam_registers), 1, handle);
    
    // Write PIA vars
    retVal = fwrite(&pia0_ca1_int_enabled,  sizeof(pia0_ca1_int_enabled),   1, handle);
    retVal = fwrite(&pia0_cb1_int_enabled,  sizeof(pia0_cb1_int_enabled),   1, handle);
    retVal = fwrite(&pia1_cb1_int_enabled,  sizeof(pia1_cb1_int_enabled),   1, handle);
    retVal = fwrite(&mux_select,            sizeof(mux_select),             1, handle);
    retVal = fwrite(&dac_output,            sizeof(dac_output),             1, handle);
    retVal = fwrite(&sound_enable,          sizeof(sound_enable),           1, handle);
    retVal = fwrite(&cas_eof,               sizeof(cas_eof),                1, handle);
    retVal = fwrite(&tape_pos,              sizeof(tape_pos),               1, handle);
    retVal = fwrite(&tape_motor,            sizeof(tape_motor),             1, handle);
    retVal = fwrite(keyboard_rows,          sizeof(keyboard_rows),          1, handle);    

    // -----------------------------------------------------------------------
    // Compress the 64K RAM data using 'high' compression ratio... it's
    // still quite fast for such small memory buffers and gets us under 32K
    // -----------------------------------------------------------------------
    int max_len = lzav_compress_bound_hi( 0x10000 );
    int comp_len = lzav_compress_hi( memory_RAM, CompressBuffer, 0x10000, max_len );

    if (retVal) retVal = fwrite(&comp_len,          sizeof(comp_len), 1, handle);
    if (retVal) retVal = fwrite(&CompressBuffer,    comp_len,         1, handle);

    strcpy(tmpStr, (retVal ? "OK ":"ERR"));
    DSPrint(12,0,0,tmpStr);
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(3,0,0,"             ");
  }
  else {
    strcpy(tmpStr,"Error opening SAV file ...");
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void DracoLoadState()
{

}

// End of file
