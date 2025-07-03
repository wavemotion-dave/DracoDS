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
 * pia.h
 *
 *  Header for module that implements the MC6821 PIA functionality.
 *
 *  February 6, 2021
 *
 *******************************************************************/

#ifndef __PIA_H__
#define __PIA_H__

void pia_init(void);

void pia_vsync_irq(void);
void pia_cart_firq(void);
void pia_hsync_firq(void);
int  pia_function_key(void);
uint8_t pia_is_audio_dac_enabled(void);

#endif  /* __PIA_H__ */
