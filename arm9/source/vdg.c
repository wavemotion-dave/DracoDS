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
 * vdg.c
 *
 *  Module that implements the MC6847
 *  Video Display Generator (VDG) functionality.
 *
 *  https://en.wikipedia.org/wiki/Motorola_6847
 *  https://www.wikiwand.com/en/Semigraphics
 *
 *  July 2024
 *
 *******************************************************************/
#include    <nds.h>
#include    <stdint.h>

#include    "cpu.h"
#include    "mem.h"
#include    "vdg.h"

#include    "dragon/font.h"
#include    "dragon/semigraph.h"

#include    "DracoUtils.h"

/* -----------------------------------------
   Local definitions
----------------------------------------- */
#define     SCREEN_WIDTH_PIX        256
#define     SCREEN_HEIGHT_PIX       192

#define     SCREEN_WIDTH_CHAR       32
#define     SCREEN_HEIGHT_CHAR      16

#define     FB_BLACK                0
#define     FB_BLUE                 1
#define     FB_GREEN                2
#define     FB_CYAN                 3
#define     FB_RED                  4
#define     FB_MAGENTA              5
#define     FB_BROWN                6
#define     FB_GRAY                 7
#define     FB_DARK_GRAY            8
#define     FB_LIGHT_BLUE           9
#define     FB_LIGHT_GREEN          10
#define     FB_LIGHT_CYAN           11
#define     FB_LIGHT_RED            12
#define     FB_LIGHT_MAGENTA        13
#define     FB_YELLOW               14
#define     FB_WHITE                15
#define     ARTIFACT_BLUE           16 // For High-Res alternating white-black
#define     ARTIFACT_ORANGE         17 // For High-Res alternating black-white


#define     CHAR_SEMI_GRAPHICS      0x80
#define     CHAR_INVERSE            0x40

#define     SEMI_GRAPH4_MASK        0x0f
#define     SEMI_GRAPH6_MASK        0x3f

#define     SEMIG8_SEG_HEIGHT       4
#define     SEMIG12_SEG_HEIGHT      6
#define     SEMIG24_SEG_HEIGHT      12

#define     PIA_COLOR_SET           0x01

#define     DEF_COLOR_CSS_0         0
#define     DEF_COLOR_CSS_1         4

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
   Module static functions
----------------------------------------- */
static void vdg_render_alpha_semi4(int vdg_mem_base);
static void vdg_render_semi6(int vdg_mem_base);
static void vdg_render_semi_ext(video_mode_t mode, int vdg_mem_base);
static void vdg_render_resl_graph(video_mode_t mode, int vdg_mem_base);
static void vdg_render_color_graph(video_mode_t mode, int vdg_mem_base);

static video_mode_t vdg_get_mode(void);

/* -----------------------------------------
   Module globals
----------------------------------------- */
static uint8_t      video_ram_offset;
static int          sam_video_mode;
uint8_t             pia_video_mode;
static video_mode_t current_mode;
static uint8_t     *fbp;
static uint8_t      sam_2x_rez = 0;

/* The following table lists the pixel ratio of columns and rows
 * relative to a 768x384 frame buffer resolution.
 */
static const int resolution[][3] __attribute__((section(".dtcm"))) = {
    { 1, 1,  512 },  // ALPHA_INTERNAL, 2 color 32x16 512B Default
    { 1, 1,  512 },  // ALPHA_EXTERNAL, 4 color 32x16 512B
    { 1, 1,  512 },  // SEMI_GRAPHICS_4, 8 color 64x32 512B
    { 1, 1,  512 },  // SEMI_GRAPHICS_6, 8 color 64x48 512B
    { 1, 1, 2048 },  // SEMI_GRAPHICS_8, 8 color 64x64 2048B
    { 1, 1, 3072 },  // SEMI_GRAPHICS_12, 8 color 64x96 3072B
    { 1, 1, 6144 },  // SEMI_GRAPHICS_24, 8 color 64x192 6144B
    { 4, 3, 1024 },  // GRAPHICS_1C, 4 color 64x64 1024B
    { 2, 3, 1024 },  // GRAPHICS_1R, 2 color 128x64 1024B
    { 2, 3, 2048 },  // GRAPHICS_2C, 4 color 128x64 2048B
    { 2, 2, 1536 },  // GRAPHICS_2R, 2 color 128x96 1536B PMODE 0
    { 2, 2, 3072 },  // GRAPHICS_3C, 4 color 128x96 3072B PMODE 1
    { 2, 1, 3072 },  // GRAPHICS_3R, 2 color 128x192 3072B PMODE 2
    { 2, 1, 6144 },  // GRAPHICS_6C, 4 color 128x192 6144B PMODE 3
    { 1, 1, 6144 },  // GRAPHICS_6R, 2 color 256x192 6144B PMODE 4
    { 1, 1, 6144 },  // DMA, 2 color 256x192 6144B
};

static uint8_t const colors[] __attribute__((section(".dtcm"))) = {
        FB_LIGHT_GREEN,
        FB_YELLOW,
        FB_LIGHT_BLUE,
        FB_LIGHT_RED,

        FB_WHITE,
        FB_CYAN,
        FB_LIGHT_MAGENTA,
        FB_BROWN,
};

/*------------------------------------------------
 * vdg_init()
 *
 *  Initialize the VDG device
 *
 *  param:  Nothing
 *  return: Nothing
 */
void vdg_init(void)
{
    video_ram_offset = 0x02;    // For offset 0x400 text screen
    sam_video_mode = 0;         // Alphanumeric

    // This is our framebuffer - the DS/DSi LCD
    fbp = (uint8_t *) (0x06000000);

    /* Default startup mode of Dragon 32
     */
    current_mode = ALPHA_INTERNAL;
    
    sam_2x_rez = 0;
}

/*------------------------------------------------
 * vdg_render()
 *
 *  Render video display.
 *  A full screen rendering is performed at every invocation on the function.
 *  The function should be called periodically and will execute a screen refresh only
 *  if 20 milliseconds of more have elapsed since the last refresh (50Hz).
 *
 *  param:  Nothing
 *  return: Nothing
 */
int reduce_framerate_for_tape = 0; 
ITCM_CODE void vdg_render(void)
{
    int     vdg_mem_base;
    
    if (tape_motor)
    {
        if (++reduce_framerate_for_tape < 10) return;
        reduce_framerate_for_tape = 0;
    }

    /* VDG/SAM mode settings
     */
    current_mode = vdg_get_mode();

    /* Render screen content to frame buffer
     */
    vdg_mem_base = video_ram_offset << 9;

    switch ( current_mode )
    {
        case ALPHA_INTERNAL:
        case SEMI_GRAPHICS_4:
            vdg_render_alpha_semi4(vdg_mem_base);
            break;

        case SEMI_GRAPHICS_6:
        case ALPHA_EXTERNAL:
            vdg_render_semi6(vdg_mem_base);
            break;

        case GRAPHICS_1C:
        case GRAPHICS_2C:
        case GRAPHICS_3C:
        case GRAPHICS_6C:
            vdg_render_color_graph(current_mode, vdg_mem_base);
            break;

        case GRAPHICS_1R:
        case GRAPHICS_2R:
        case GRAPHICS_3R:
        case GRAPHICS_6R:
            vdg_render_resl_graph(current_mode, vdg_mem_base);
            break;

        case SEMI_GRAPHICS_8:
        case SEMI_GRAPHICS_12:
        case SEMI_GRAPHICS_24:
            vdg_render_semi_ext(current_mode, vdg_mem_base);
            break;

        case DMA:
            // Not supported... trap?
            break;

        default:
            break;
    }
}

/*------------------------------------------------
 * vdg_set_video_offset()
 *
 *  Set the video display start offset in RAM.
 *  Most significant six bits of a 15 bit RAM address.
 *  Value is set by SAM device.
 *
 *  param:  Offset value.
 *  return: Nothing
 */
void vdg_set_video_offset(uint8_t offset)
{
    video_ram_offset = offset;
}

/*------------------------------------------------
 * vdg_set_mode_sam()
 *
 *  Set the video display mode from SAM device.
 *
 *  0   Alpha, S4, S6
 *  1   G1C, G1R
 *  2   G2C
 *  3   G2R
 *  4   G3C
 *  5   G3R
 *  6   G6R, G6C
 *  7   DMA
 *
 *  param:  Mode value.
 *  return: Nothing
 */
void vdg_set_mode_sam(int sam_mode)
{
    sam_video_mode = sam_mode;
}

/*------------------------------------------------
 * vdg_set_mode_pia()
 *
 *  Set the video display mode from PIA device.
 *  Mode bit are as-is for PIA shifted 3 to the right:
 *  Bit 4   O   Screen Mode G / ^A
 *  Bit 3   O   Screen Mode GM2
 *  Bit 2   O   Screen Mode GM1
 *  Bit 1   O   Screen Mode GM0 / ^INT
 *  Bit 0   O   Screen Mode CSS
 *
 *  param:  Mode value.
 *  return: Nothing
 */
void vdg_set_mode_pia(uint8_t pia_mode)
{
    pia_video_mode = pia_mode;
    if (myConfig.forceCSS)
    {
        if (myConfig.forceCSS == 1) pia_video_mode &= 0xFE;
        else  pia_video_mode |= 1;
    }
}

/*------------------------------------------------
 * vdg_render_alpha_semi4()
 *
 *  Render aplphanumeric internal and Semi-graphics 4.
 *
 * param:  VDG memory base address
 * return: None
 *
 */
ITCM_CODE void vdg_render_alpha_semi4(int vdg_mem_base)
{
    int         c, row, col, font_row, font_col;
    int         char_index, row_address;
    uint8_t     bit_pattern, pix_pos;
    uint8_t     color_set, fg_color, bg_color, tmp;

    uint8_t    *screen_buffer;

    screen_buffer = fbp;

    if ( pia_video_mode & PIA_COLOR_SET )
        color_set = colors[DEF_COLOR_CSS_1];
    else
        color_set = colors[DEF_COLOR_CSS_0];

    for ( row = 0; row < SCREEN_HEIGHT_CHAR; row++ )
    {
        row_address = row * SCREEN_WIDTH_CHAR + vdg_mem_base;

        for ( font_row = 0; font_row < FONT_HEIGHT; font_row++ )
        {
            for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
            {
                c = memory[col + row_address];

                bg_color = FB_BLACK;

                /* Mode dependent initializations
                 * for text or semigraphics 4:
                 * - Determine foreground and background colors
                 * - Character pattern array
                 * - Character code index to bit pattern array
                 *
                 */
                if ( (uint8_t)c & CHAR_SEMI_GRAPHICS )
                {
                    fg_color = colors[(((uint8_t)c & 0b01110000) >> 4)];
                    char_index = (int)(((uint8_t) c) & SEMI_GRAPH4_MASK);
                    bit_pattern = semi_graph_4[char_index][font_row];
                }
                else
                {
                    fg_color = color_set;

                    if ( (uint8_t)c & CHAR_INVERSE )
                    {
                        tmp = fg_color;
                        fg_color = bg_color;
                        bg_color = tmp;
                    }
                    char_index = (int)(((uint8_t) c) & ~(CHAR_SEMI_GRAPHICS | CHAR_INVERSE));
                    bit_pattern = font_img5x7[char_index][font_row];
                }

                /* Render a row of pixels in a temporary buffer
                 */
                pix_pos = 0x80;

                if (!bit_pattern)
                {
                    memset(screen_buffer, bg_color, 8);
                    screen_buffer+=8;
                }
                else
                for ( font_col = 0; font_col < FONT_WIDTH; font_col++ )
                {
                    /* Bit is set in Font, print pixel(s) in text color
                    */
                    if ( (bit_pattern & pix_pos) )
                    {
                        *screen_buffer++ = fg_color;
                    }
                    /* Bit is cleared in Font
                    */
                    else
                    {
                        *screen_buffer++ = bg_color;
                    }

                    /* Move to the next pixel position
                    */
                    pix_pos = pix_pos >> 1;
                }
            }
        }
    }
}

/*------------------------------------------------
 * vdg_render_semi6()
 *
 *  Render Semi-graphics 6.
 *
 * param:  VDG memory base address
 * return: None
 *
 */
ITCM_CODE static void vdg_render_semi6(int vdg_mem_base)
{
    int         c, row, col, font_row, font_col, color_set;
    int         char_index, row_address;
    uint8_t     bit_pattern, pix_pos;
    uint8_t     fg_color, bg_color;

    uint8_t    *screen_buffer;

    screen_buffer = fbp;
    color_set = (int)(4 * (pia_video_mode & PIA_COLOR_SET));

    for ( row = 0; row < SCREEN_HEIGHT_CHAR; row++ )
    {
        row_address = row * SCREEN_WIDTH_CHAR + vdg_mem_base;

        for ( font_row = 0; font_row < FONT_HEIGHT; font_row++ )
        {
            for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
            {
                c = memory[col + row_address];

                bg_color = FB_BLACK;
                fg_color = colors[(int)(((c & 0b11000000) >> 6) + color_set)];

                char_index = (int)(((uint8_t) c) & SEMI_GRAPH6_MASK);
                bit_pattern = semi_graph_6[char_index][font_row];

                /* Render a row of pixels in a temporary buffer
                 */
                pix_pos = 0x80;

                for ( font_col = 0; font_col < FONT_WIDTH; font_col++ )
                {
                    /* Bit is set in Font, print pixel(s) in text color
                    */
                    if ( (bit_pattern & pix_pos) )
                    {
                        *screen_buffer++ = fg_color;
                    }
                    /* Bit is cleared in Font
                    */
                    else
                    {
                        *screen_buffer++ = bg_color;
                    }

                    /* Move to the next pixel position
                    */
                    pix_pos = pix_pos >> 1;
                }
            }
        }
    }
}

/*------------------------------------------------
 * vdg_render_semi_ext()
 *
 * Render semigraphics-8 -12 or -24.
 * Mode can only be SEMI_GRAPHICS_8, SEMI_GRAPHICS_12, and SEMI_GRAPHICS_24 as
 * this is not checked for validity.
 *
 * param:  Mode, base address of video memory buffer.
 * return: none
 *
 */
ITCM_CODE static void vdg_render_semi_ext(video_mode_t mode, int vdg_mem_base)
{
    int         row, seg_row, scan_line, col, font_col, font_row;
    int         segments, seg_scan_lines;
    int         c, char_index, row_address;
    uint8_t     bit_pattern, pix_pos;
    uint8_t     color_set, fg_color, bg_color, tmp;
    uint8_t    *screen_buffer;
    
    screen_buffer = fbp;
    font_row = 0;

    if ( pia_video_mode & PIA_COLOR_SET )
        color_set = colors[DEF_COLOR_CSS_1];
    else
        color_set = colors[DEF_COLOR_CSS_0];

    if ( mode == SEMI_GRAPHICS_8 )
    {
        segments = SEMIG8_SEG_HEIGHT;
        seg_scan_lines = FONT_HEIGHT / SEMIG8_SEG_HEIGHT;
    }
    else if ( mode == SEMI_GRAPHICS_12 )
    {
        segments = SEMIG12_SEG_HEIGHT;
        seg_scan_lines = FONT_HEIGHT / SEMIG12_SEG_HEIGHT;
    }
    else if ( mode == SEMI_GRAPHICS_24 )
    {
        segments = SEMIG24_SEG_HEIGHT;
        seg_scan_lines = FONT_HEIGHT / SEMIG24_SEG_HEIGHT;
    }
    else
    {
        return;
    }

    for ( row = 0; row < SCREEN_HEIGHT_CHAR; row++ )
    {
        for ( seg_row = 0; seg_row < segments; seg_row++ )
        {
            row_address = (row * segments + seg_row) * SCREEN_WIDTH_CHAR + vdg_mem_base;

            for ( scan_line = 0; scan_line < seg_scan_lines; scan_line++ )
            {
                for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
                {
                    c = memory[col + row_address];

                    /* Mode dependent initializations
                    * for text or semigraphics 4:
                    * - Determine foreground and background colors
                    * - Character pattern array
                    * - Character code index to bit pattern array
                    *
                    */
                    bg_color = FB_BLACK;

                    if ( (uint8_t)c & CHAR_SEMI_GRAPHICS )
                    {
                        fg_color = colors[(((uint8_t)c & 0b01110000) >> 4)];
                        char_index = (int)(((uint8_t) c) & SEMI_GRAPH4_MASK);
                        bit_pattern = semi_graph_4[char_index][font_row];
                    }
                    else
                    {
                        fg_color = color_set;

                        if ( (uint8_t)c & CHAR_INVERSE )
                        {
                            tmp = fg_color;
                            fg_color = bg_color;
                            bg_color = tmp;
                        }
                        char_index = (int)(((uint8_t) c) & ~(CHAR_SEMI_GRAPHICS | CHAR_INVERSE));
                        bit_pattern = font_img5x7[char_index][font_row];
                    }

                    /* Render a row of pixels in a temporary buffer
                    */
                    pix_pos = 0x80;

                    if (!bit_pattern)
                    {
                        memset(screen_buffer, bg_color, 8);
                        screen_buffer+=8;
                    }
                    else
                    for ( font_col = 0; font_col < FONT_WIDTH; font_col++ )
                    {
                        /* Bit is set in Font, print pixel(s) in text color
                        */
                        if ( (bit_pattern & pix_pos) )
                        {
                            *screen_buffer++ = fg_color;
                        }
                        /* Bit is cleared in Font
                        */
                        else
                        {
                            *screen_buffer++ = bg_color;
                        }

                        /* Move to the next pixel position
                        */
                        pix_pos = pix_pos >> 1;
                    }
                }

                font_row++;
                if ( font_row == FONT_HEIGHT )
                    font_row = 0;
            }
        }
    }
}

/*------------------------------------------------
 * vdg_render_resl_graph()
 *
 *  Render high resolution graphics modes:
 *  GRAPHICS_1R, GRAPHICS_2R, GRAPHICS_3R, and GRAPHICS_6R.
 *
 * param:  Mode, base address of video memory buffer.
 * return: none
 *
 */
ITCM_CODE static void vdg_render_resl_graph(video_mode_t mode, int vdg_mem_base)
{
    int         i, vdg_mem_offset, element, buffer_index;
    int         video_mem, row_rep;
    uint8_t     pixels_byte, fg_color, pixel;
    uint8_t    *screen_buffer;
    uint8_t     last_pixel = 99;
    uint8_t     pixel_row[SCREEN_WIDTH_PIX+16];

    screen_buffer = fbp;

    video_mem = resolution[mode][RES_MEM];
    row_rep = resolution[mode][RES_ROW_REP];
    buffer_index = 0;

    if ( pia_video_mode & PIA_COLOR_SET )
    {
        fg_color = colors[DEF_COLOR_CSS_1];
    }
    else
    {
        fg_color = colors[DEF_COLOR_CSS_0];
    }

    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem / (1+sam_2x_rez); vdg_mem_offset++)
    {
        pixels_byte = memory[vdg_mem_offset + vdg_mem_base];

        if (pixels_byte == 0x00)
        {
            if ( mode != GRAPHICS_6R ) // Expand 2x
            {
                memset(pixel_row+buffer_index, FB_BLACK, 16);
                buffer_index += 16;
            }
            else
            {
                memset(pixel_row+buffer_index, FB_BLACK, 8);
                buffer_index += 8;
            }
            last_pixel = pixel = FB_BLACK;
        }
        else
        for ( element = 7; element >= 0; element--)
        {
            if ( (pixels_byte >> element) & 0x01 )
            {
                pixel = fg_color;
            }
            else
            {
                pixel = FB_BLACK;
            }

            if ( mode != GRAPHICS_6R ) // Expand 2x
            {
                pixel_row[buffer_index++] = pixel;
                pixel_row[buffer_index++] = pixel;
            }
            else // Possible artifact colors
            {
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    if (buffer_index & 1)
                    {
                        if (pixel == FB_BLACK) pixel = ARTIFACT_ORANGE; else pixel=ARTIFACT_BLUE;
                    }
                    else
                    {
                        if (pixel == FB_BLACK) pixel = ARTIFACT_BLUE; else pixel=ARTIFACT_ORANGE;
                    }
                }
                pixel_row[buffer_index++] = pixel;
            }
        }

        if ( buffer_index >= SCREEN_WIDTH_PIX )
        {
            // A few games like Monster Maze and Temple of ROM are 256x192 VDG mode but the
            // SAM bits indicate a 256x96 mode... In essence, they are doing 256x192 but only
            // using 3K of memory and the vertical resolution is somehow doubled. We handle
            // that here - at least until I can figure out what's really going on!
            for ( i = 0; i < row_rep * (sam_2x_rez+1); i++ )
            {
                memcpy(screen_buffer, pixel_row, SCREEN_WIDTH_PIX);
                screen_buffer += SCREEN_WIDTH_PIX;
            }

            buffer_index = 0;
        }
    }
}

/*------------------------------------------------
 * vdg_render_color_graph()
 *
 *  Render color graphics modes:
 *  GRAPHICS_1C, GRAPHICS_2C, GRAPHICS_3C, and GRAPHICS_6C.
 *
 * param:  Mode, base address of video memory buffer.
 * return: none
 *
 */
ITCM_CODE static void vdg_render_color_graph(video_mode_t mode, int vdg_mem_base)
{
    int         i, vdg_mem_offset, element, buffer_index;
    int         video_mem, row_rep, color_set, color;
    uint8_t     pixels_byte, pixel;
    uint8_t    *screen_buffer;
    uint8_t     pixel_row[SCREEN_WIDTH_PIX+16];

    screen_buffer = fbp;

    video_mem = resolution[mode][RES_MEM];
    row_rep = resolution[mode][RES_ROW_REP];
    color_set = 4 * (pia_video_mode & PIA_COLOR_SET);
    buffer_index = 0;

    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem; vdg_mem_offset++)
    {
        pixels_byte = memory[vdg_mem_offset + vdg_mem_base];

        for ( element = 6; element >= 0; element -= 2)
        {
            color = (int)((pixels_byte >> element) & 0x03) + color_set;
            pixel = colors[color];

            pixel_row[buffer_index++] = pixel;
            pixel_row[buffer_index++] = pixel;
            if ( mode == GRAPHICS_1C )
            {
                pixel_row[buffer_index++] = pixel;
                pixel_row[buffer_index++] = pixel;
            }
        }

        if ( buffer_index >= SCREEN_WIDTH_PIX )
        {
            for ( i = 0; i < row_rep; i++ )
            {
                memcpy(screen_buffer, pixel_row, SCREEN_WIDTH_PIX);
                screen_buffer += SCREEN_WIDTH_PIX;
            }

            buffer_index = 0;
        }
    }
}

/*------------------------------------------------
 * vdg_get_mode()
 *
 * Parse 'sam_video_mode' and 'pia_video_mode' and return video mode type.
 *
 * param:  None
 * return: Video mode
 *
 */
static video_mode_t vdg_get_mode(void)
{
    video_mode_t mode = UNDEFINED;

    if ( sam_video_mode == 7 )
    {
        mode = DMA;
    }
    else if ( (pia_video_mode & 0x10) ) // Graphics Modes...
    {
        // ------------------------------------------------------------------------------
        // Something strange happens with VDG bit settings vs SAM bit settings for
        // the graphic mode selection. Even though the VDG might say 'Graphics 2-C'
        // the SAM registers might say 'Graphics 3-C'. In practice, the SAM registers
        // seem to take precedence... so that gets the priority in this logic. Otherwise
        // we end up with games like Micro Chess that draws too tall or Monster Maze
        // that only draws half a screen.
        // ------------------------------------------------------------------------------
        
        sam_2x_rez = 0;
        
        switch ( pia_video_mode & 0x0e  )
        {
            case 0x00:
                debug[0]++;
                mode = GRAPHICS_1C;
                break;
            case 0x02:
                debug[1]++;
                mode = GRAPHICS_1R;
                break;
            case 0x04:
                debug[2]++;
                mode = GRAPHICS_2C;
                if (sam_video_mode == 0x04) mode = GRAPHICS_3C; // Bump up to 3K higher-rez mode
                break;
            case 0x06:
                debug[3]++;
                mode = GRAPHICS_2R;
                break;
            case 0x08:
                debug[4]++;
                mode = GRAPHICS_3C;
                break;
            case 0x0a:
                debug[5]++;
                mode = GRAPHICS_3R;
                break;
            case 0x0c:
                debug[6]++;
                mode = GRAPHICS_6C;
                break;
            case 0x0e:
                debug[7]++;
                mode = GRAPHICS_6R;
                if (sam_video_mode == 0x04) sam_2x_rez = 1;     // Essentially 256x96 using 3K
                break;
        }
    }
    else // Text Modes...
    {
        if ( sam_video_mode == 0 &&
             (pia_video_mode & 0x02) == 0 )
        {
            mode = ALPHA_INTERNAL;
            // Character bit.7 selects SEMI_GRAPHICS_4;
        }
        else if ( sam_video_mode == 0 &&
                (pia_video_mode & 0x02) )
        {
            mode = SEMI_GRAPHICS_6;
            // Character bit.7=0 selects ALPHA_EXTERNAL;
            // Character bit.7=1 selects SEMI_GRAPHICS_6;
        }
        else if ( sam_video_mode == 2 &&
                (pia_video_mode & 0x02) == 0 )
        {
            mode = SEMI_GRAPHICS_8;
        }
        else if ( sam_video_mode == 4 &&
                (pia_video_mode & 0x02) == 0 )
        {
            mode = SEMI_GRAPHICS_12;
        }
        else if ( sam_video_mode == 4 &&
                (pia_video_mode & 0x02) == 0 )
        {
            mode = SEMI_GRAPHICS_24;
        }
    }

    return mode;
}
