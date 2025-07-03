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
#include "mem.h"
#include "pia.h"
#include "vdg.h"
#include "sam.h"
#include "printf.h"

#define     DRAGON_ROM_START        0x8000
#define     DRAGON_ROM_END          0xfeff
#define     CARTRIDGE_ROM_BASE      0xc000

#define     EXEC_VECTOR_HI          0x9d
#define     EXEC_VECTOR_LO          0x9e



u32 draco_line           __attribute__((section(".dtcm"))) = 0;
u8  draco_special_key    __attribute__((section(".dtcm"))) = 0;
u32 last_file_size       __attribute__((section(".dtcm"))) = 0;
u8  isCompressed         __attribute__((section(".dtcm"))) = 1;
u8  tape_play_skip_frame __attribute__((section(".dtcm"))) = 0;

u8 CompressBuffer[128*1024];

// ----------------------------------------------------------------------
// Reset the emulation. Freshly decompress the contents of RAM memory
// and setup the CPU registers exactly as the snapshot indicates. Then
// we can start the emulation at exactly the point it left off... This
// works fine so long as the game does not need to go back out to the
// tape to load another segment of the game.
// ----------------------------------------------------------------------
void dragon_reset(void)
{
    draco_special_key = 0;

    draco_line = 0;

    // Initialize all of the peripherals
    mem_init();
    sam_init();
    pia_init();
    vdg_init();

    mem_define_rom(DRAGON_ROM_START, DRAGON_ROM_END);
    
    if (myConfig.machine)
    {
        mem_load(DRAGON_ROM_START, CoCoBASIC, sizeof(CoCoBASIC));
    }
    else
    {
        mem_load(DRAGON_ROM_START, DragonBASIC, sizeof(DragonBASIC));
    }

    mem_define_rom(CARTRIDGE_ROM_BASE, 0x4000-256);
    
    if (draco_mode == MODE_CART)
    {
        mem_load(CARTRIDGE_ROM_BASE, ROM_Memory, 0x4000-256);
        mem_write(EXEC_VECTOR_HI, 0xc0);
        mem_write(EXEC_VECTOR_LO, 0x00);
    }

    cpu_init(DRAGON_ROM_START);
    cpu_reset(1);
    cpu_check_reset();
}


// -----------------------------------------------------------------------------
// Run the emulation for exactly 1 scanline and handle the VDP interrupt if
// the emulation has executed the last line of the frame.  This also handles
// direct beeper and possibly AY sound emulation as well. Crude but effective.
// -----------------------------------------------------------------------------
ITCM_CODE u32 dragon_run(void)
{
    // --------------------------------------
    // Process 1 scanline worth of DAC Audio
    // --------------------------------------
    processDirectAudio();

    // ----------------------------------------
    // Execute one scanline of CPU (57 cycles)
    // ----------------------------------------
    cpu_run();

    // -------------------------------------------------
    // Each scanline generates a Fast IRQ for the HSync
    // -------------------------------------------------
    pia_hsync_firq();
    
    // --------------------------------------------
    // Are we at the end of the frame? VSync time!
    // --------------------------------------------
    if (++draco_line == (myConfig.machine ? 262:312))
    {
        vdg_render();
        pia_vsync_irq();
        draco_line = 0;
        extern int cycles_this_scanline;
        cycles_this_scanline = 0;
        return 1; // End of frame
    }

    return 0; // Not end of frame
}

// End of file
