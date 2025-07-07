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
#include "disk.h"
#include "fdc.h"
#include "vdg.h"
#include "printf.h"

#include "lzav.h"

#define DRACO_SAVE_VER   0x0002       // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

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
    if (retVal) retVal = fwrite(&last_path, sizeof(last_path), 1, handle);
    if (retVal) retVal = fwrite(&last_file, sizeof(last_file), 1, handle);

    // Write Motorola 6809 CPU
    if (retVal) retVal = fwrite(&cpu, sizeof(cpu), 1, handle);

    // Write SAM registers
    if (retVal) retVal = fwrite(&sam_registers, sizeof(sam_registers), 1, handle);
    
    // Write DISK vars
    if (retVal) retVal = fwrite(&nmi_enable, sizeof(nmi_enable), 1, handle);
    
    // Write FDC vars
    if (retVal) retVal = fwrite(&FDC,                   sizeof(FDC),                1, handle);
    if (retVal) retVal = fwrite(&Geom,                  sizeof(Geom),               1, handle);
    if (retVal) retVal = fwrite(&io_show_status,        sizeof(io_show_status),     1, handle);
    if (retVal) retVal = fwrite(disk_unsaved_data,      sizeof(disk_unsaved_data),  1, handle);
    
    // Write PIA vars
    if (retVal) retVal = fwrite(&pia0_ca1_int_enabled,  sizeof(pia0_ca1_int_enabled),   1, handle);
    if (retVal) retVal = fwrite(&pia0_cb1_int_enabled,  sizeof(pia0_cb1_int_enabled),   1, handle);
    if (retVal) retVal = fwrite(&pia1_cb1_int_enabled,  sizeof(pia1_cb1_int_enabled),   1, handle);
    if (retVal) retVal = fwrite(&mux_select,            sizeof(mux_select),             1, handle);
    if (retVal) retVal = fwrite(&dac_output,            sizeof(dac_output),             1, handle);
    if (retVal) retVal = fwrite(&sound_enable,          sizeof(sound_enable),           1, handle);
    if (retVal) retVal = fwrite(&cas_eof,               sizeof(cas_eof),                1, handle);
    if (retVal) retVal = fwrite(&tape_pos,              sizeof(tape_pos),               1, handle);
    if (retVal) retVal = fwrite(&tape_motor,            sizeof(tape_motor),             1, handle);
    if (retVal) retVal = fwrite(keyboard_rows,          sizeof(keyboard_rows),          1, handle);    
    if (retVal) retVal = fwrite(&pia0_ddr_a,            sizeof(pia0_ddr_a),             1, handle);
    if (retVal) retVal = fwrite(&pia0_ddr_b,            sizeof(pia0_ddr_b),             1, handle);
    if (retVal) retVal = fwrite(&pia1_ddr_a,            sizeof(pia1_ddr_a),             1, handle);
    if (retVal) retVal = fwrite(&pia1_ddr_b,            sizeof(pia1_ddr_b),             1, handle);
    
    // Write VDG vars
    if (retVal) retVal = fwrite(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
    if (retVal) retVal = fwrite(&sam_video_mode,        sizeof(sam_video_mode),         1, handle);
    if (retVal) retVal = fwrite(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
    if (retVal) retVal = fwrite(&sam_2x_rez,            sizeof(sam_2x_rez),             1, handle);
    if (retVal) retVal = fwrite(&pia_video_mode,        sizeof(pia_video_mode),         1, handle);
    if (retVal) retVal = fwrite(&current_mode,          sizeof(current_mode),           1, handle);
    
    // And some DracoDS handling memory
    if (retVal) retVal = fwrite(&draco_line,              sizeof(draco_line),               1, handle);
    if (retVal) retVal = fwrite(&draco_special_key,       sizeof(draco_special_key),        1, handle);
    if (retVal) retVal = fwrite(&last_file_size,          sizeof(last_file_size),           1, handle);
    if (retVal) retVal = fwrite(&tape_play_skip_frame,    sizeof(tape_play_skip_frame),     1, handle);
    if (retVal) retVal = fwrite(&draco_scanline_counter,  sizeof(draco_scanline_counter),   1, handle);
    if (retVal) retVal = fwrite(&joy_x,                   sizeof(joy_x),                    1, handle);
    if (retVal) retVal = fwrite(&joy_x,                   sizeof(joy_x),                    1, handle);
    if (retVal) retVal = fwrite(&emuFps,                  sizeof(emuFps),                   1, handle);
    if (retVal) retVal = fwrite(&emuActFrames,            sizeof(emuActFrames),             1, handle);
    if (retVal) retVal = fwrite(&timingFrames,            sizeof(timingFrames),             1, handle);
    
    // IO Memory Space
    if (retVal) retVal = fwrite(memory_IO+0xFF00,         0x100,                            1, handle);
    
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
  else
  {
      strcpy(tmpStr,"Error opening SAV file ...");
      DSPrint(2,0,0,tmpStr);
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void DracoLoadState()
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

  FILE *handle = fopen(szLoadFile, "rb");
  if (handle != NULL)
  {
     strcpy(tmpStr,"LOADING...");
     DSPrint(4,0,0,tmpStr);

    // Read Version
    u16 save_ver = 0xBEEF;
    retVal = fread(&save_ver, sizeof(u16), 1, handle);

    if (save_ver == DRACO_SAVE_VER)
    {
        // Restore Last Directory Path / Tape File
        if (retVal) retVal = fread(&last_path, sizeof(last_path), 1, handle);
        if (retVal) retVal = fread(&last_file, sizeof(last_file), 1, handle);

        // Restore Motorola 6809 CPU
        if (retVal) retVal = fread(&cpu, sizeof(cpu), 1, handle);

        // Restore SAM registers
        if (retVal) retVal = fread(&sam_registers, sizeof(sam_registers), 1, handle);
        
        // Restore DISK vars
        if (retVal) retVal = fread(&nmi_enable, sizeof(nmi_enable), 1, handle);
        
        // Restore FDC vars
        if (retVal) retVal = fread(&FDC,                    sizeof(FDC),                1, handle);
        if (retVal) retVal = fread(&Geom,                   sizeof(Geom),               1, handle);
        if (retVal) retVal = fread(&io_show_status,         sizeof(io_show_status),     1, handle);
        if (retVal) retVal = fread(disk_unsaved_data,       sizeof(disk_unsaved_data),  1, handle);
        
        Geom.disk0 = TapeCartDiskBuffer;    // Always... in case memory shifted
        
        // Restore PIA vars
        if (retVal) retVal = fread(&pia0_ca1_int_enabled,  sizeof(pia0_ca1_int_enabled),   1, handle);
        if (retVal) retVal = fread(&pia0_cb1_int_enabled,  sizeof(pia0_cb1_int_enabled),   1, handle);
        if (retVal) retVal = fread(&pia1_cb1_int_enabled,  sizeof(pia1_cb1_int_enabled),   1, handle);
        if (retVal) retVal = fread(&mux_select,            sizeof(mux_select),             1, handle);
        if (retVal) retVal = fread(&dac_output,            sizeof(dac_output),             1, handle);
        if (retVal) retVal = fread(&sound_enable,          sizeof(sound_enable),           1, handle);
        if (retVal) retVal = fread(&cas_eof,               sizeof(cas_eof),                1, handle);
        if (retVal) retVal = fread(&tape_pos,              sizeof(tape_pos),               1, handle);
        if (retVal) retVal = fread(&tape_motor,            sizeof(tape_motor),             1, handle);
        if (retVal) retVal = fread(keyboard_rows,          sizeof(keyboard_rows),          1, handle);    
        if (retVal) retVal = fread(&pia0_ddr_a,            sizeof(pia0_ddr_a),             1, handle);
        if (retVal) retVal = fread(&pia0_ddr_b,            sizeof(pia0_ddr_b),             1, handle);
        if (retVal) retVal = fread(&pia1_ddr_a,            sizeof(pia1_ddr_a),             1, handle);
        if (retVal) retVal = fread(&pia1_ddr_b,            sizeof(pia1_ddr_b),             1, handle);

        // Restore VDG vars
        if (retVal) retVal = fread(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
        if (retVal) retVal = fread(&sam_video_mode,        sizeof(sam_video_mode),         1, handle);
        if (retVal) retVal = fread(&video_ram_offset,      sizeof(video_ram_offset),       1, handle);
        if (retVal) retVal = fread(&sam_2x_rez,            sizeof(sam_2x_rez),             1, handle);
        if (retVal) retVal = fread(&pia_video_mode,        sizeof(pia_video_mode),         1, handle);
        if (retVal) retVal = fread(&current_mode,          sizeof(current_mode),           1, handle);
        
        // Restore some DracoDS handling memory
        if (retVal) retVal = fread(&draco_line,              sizeof(draco_line),               1, handle);
        if (retVal) retVal = fread(&draco_special_key,       sizeof(draco_special_key),        1, handle);
        if (retVal) retVal = fread(&last_file_size,          sizeof(last_file_size),           1, handle);
        if (retVal) retVal = fread(&tape_play_skip_frame,    sizeof(tape_play_skip_frame),     1, handle);
        if (retVal) retVal = fread(&draco_scanline_counter,  sizeof(draco_scanline_counter),   1, handle);
        if (retVal) retVal = fread(&joy_x,                   sizeof(joy_x),                    1, handle);
        if (retVal) retVal = fread(&joy_x,                   sizeof(joy_x),                    1, handle);
        if (retVal) retVal = fread(&emuFps,                  sizeof(emuFps),                   1, handle);
        if (retVal) retVal = fread(&emuActFrames,            sizeof(emuActFrames),             1, handle);
        if (retVal) retVal = fread(&timingFrames,            sizeof(timingFrames),             1, handle);

        // IO Memory Space
        if (retVal) retVal = fread(memory_IO+0xFF00,         0x100,                            1, handle);

        // Restore Main RAM memory
        int comp_len = 0;
        if (retVal) retVal = fread(&comp_len,          sizeof(comp_len), 1, handle);
        if (retVal) retVal = fread(&CompressBuffer,    comp_len,         1, handle);

        // ------------------------------------------------------------------
        // Decompress the previously compressed RAM and put it back into the
        // right memory location... this is quite fast all things considered.
        // ------------------------------------------------------------------
        (void)lzav_decompress( CompressBuffer, memory_RAM, comp_len, 0x10000 );
        
        strcpy(tmpStr, (retVal ? "OK ":"ERR"));
        DSPrint(13,0,0,tmpStr);

        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,0,0,"             ");
      }
  }
  else
  {
      DSPrint(4,0,0,"NO SAVED GAME");
      WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
      DSPrint(4,0,0,"             ");
  }

    fclose(handle);
}

// End of file
