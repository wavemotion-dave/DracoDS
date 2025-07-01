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


/********************************************************************
 * vdg.h
 *
 *  Header for module that implements the MC6847
 *  Video Display Generator (VDG) functionality.
 *
 *  February 6, 2021
 *
 *******************************************************************/

#ifndef __VDG_H__
#define __VDG_H__

#define     VDG_REFRESH_RATE        50      // in Hz

void vdg_init(void);
void vdg_render(void);

void vdg_set_video_offset(uint8_t offset);
void vdg_set_mode_sam(int sam_mode);
void vdg_set_mode_pia(uint8_t pia_mode);

#endif  /* __VDG_H__ */
