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

#define     KBD_ROWS            7

extern uint8_t   pia0_ca1_int_enabled;
extern uint8_t   pia0_cb1_int_enabled;
extern uint8_t   pia1_cb1_int_enabled;
extern uint8_t   mux_select;
extern uint16_t  dac_output;
extern uint8_t   sound_enable;
extern uint8_t   last_comparator;
extern uint8_t   cas_eof;
extern uint32_t  tape_pos;
extern uint16_t  tape_motor;
extern uint8_t   keyboard_rows[KBD_ROWS];

void pia_init(void);

void pia_vsync_irq(void);
void pia_cart_firq(void);
void pia_hsync_firq(void);
int  pia_function_key(void);
uint8_t pia_is_audio_dac_enabled(void);

#endif  /* __PIA_H__ */
