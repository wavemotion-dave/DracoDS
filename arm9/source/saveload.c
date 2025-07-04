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
#include "CRC32.h"
#include "cpu.h"
#include "DracoUtils.h"
#include "printf.h"

#include "lzav.h"

#define DRACO_SAVE_VER   0x0000       // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

u8 CompressBuffer[128*1024];

void DracoSaveState()
{

}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void DracoLoadState()
{

}

// End of file
