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

/* -----------------------------------------
   Local definitions
----------------------------------------- */
#define     SCREEN_WIDTH_PIX        256
#define     SCREEN_HEIGHT_PIX       192

#define     SCREEN_WIDTH_CHAR       32
#define     SCREEN_HEIGHT_CHAR      16

#define     FB_BLACK                0

#define     FB_GREEN                1
#define     FB_YELLOW               2
#define     FB_BLUE                 3
#define     FB_RED                  4

#define     FB_BUFF                 5
#define     FB_CYAN                 6
#define     FB_MAGENTA              7
#define     FB_ORANGE               8

#define     ARTIFACT_BLUE           9  // For High-Res alternating white-black
#define     ARTIFACT_ORANGE         10 // For High-Res alternating black-white

#define     CHAR_SEMI_GRAPHICS      0x80
#define     CHAR_INVERSE            0x40

#define     SEMI_GRAPH4_MASK        0x0f
#define     SEMI_GRAPH6_MASK        0x3f

#define     SEMIG8_SEG_HEIGHT       4
#define     SEMIG12_SEG_HEIGHT      6
#define     SEMIG24_SEG_HEIGHT      12

#define     PIA_COLOR_SET           0x01

#define     DEF_COLOR_CSS_0         1  // GREEN, YELLOW, BLUE, RED
#define     DEF_COLOR_CSS_1         5  // BUFF, CYAN, MAGENTA, ORANGE

/* Index definitions for resolution[] matrix
 */
#define     RES_PIXEL_REP           0       // Count of uint8_t repeat per pixel
#define     RES_ROW_REP             1       // Row repeat count
#define     RES_MEM                 2       // Memory bytes per page

typedef enum
{                       // Colors   Res.     Bytes BASIC
    ALPHA_INTERNAL = 0, // 2 color  32x16    512   Default
    ALPHA_EXTERNAL,     // 4 color  32x16    512
    SEMI_GRAPHICS_4,    // 8 color  64x32    512
    SEMI_GRAPHICS_6,    // 8 color  64x48    512
    SEMI_GRAPHICS_8,    // 8 color  64x64   2048
    SEMI_GRAPHICS_12,   // 8 color  64x96   3072
    SEMI_GRAPHICS_24,   // 8 color  64x192  6144
    GRAPHICS_1C,        // 4 color  64x64   1024
    GRAPHICS_1R,        // 2 color  128x64  1024
    GRAPHICS_2C,        // 4 color  128x64  1536
    GRAPHICS_2R,        // 2 color  128x96  1536   PMODE0
    GRAPHICS_3C,        // 4 color  128x96  3072   PMODE1
    GRAPHICS_3R,        // 2 color  128x192 3072   PMODE2
    GRAPHICS_6C,        // 4 color  128x192 6144   PMODE3
    GRAPHICS_6R,        // 2 color  256x192 6144   PMODE4
    DMA,                // 2 color  256x192 6144
    UNDEFINED,          // Undefined
} video_mode_t;


/* -----------------------------------------
   Module globals
----------------------------------------- */
extern int          video_ram_offset;
extern int          sam_video_mode;
extern int          sam_2x_rez;
extern uint8_t      pia_video_mode;
extern video_mode_t current_mode;

void vdg_init(void);
void vdg_render(void);

void vdg_set_video_offset(uint8_t offset);
void vdg_set_mode_sam(int sam_mode);
void vdg_set_mode_pia(uint8_t pia_mode);

#endif  /* __VDG_H__ */
