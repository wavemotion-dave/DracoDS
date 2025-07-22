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
   Module functions
----------------------------------------- */
void vdg_render_alpha_semi4(int vdg_mem_base);
void vdg_render_semi6(int vdg_mem_base);
void vdg_render_semi_ext(video_mode_t mode, int vdg_mem_base);
void vdg_render_resl_graph(video_mode_t mode, int vdg_mem_base);
void vdg_render_color_graph(video_mode_t mode, int vdg_mem_base);
void vdg_render_artifacting(video_mode_t mode, int vdg_mem_base);
void vdg_render_artifacting_mono(video_mode_t mode, int vdg_mem_base);
void vdg_render_artifacting_green(video_mode_t mode, int vdg_mem_base);

video_mode_t vdg_get_mode(void);

/* -----------------------------------------
   Module globals
----------------------------------------- */
int          video_ram_offset   __attribute__((section(".dtcm")));
int          sam_video_mode     __attribute__((section(".dtcm")));
int          sam_2x_rez         __attribute__((section(".dtcm"))) = 1;

uint8_t      pia_video_mode     __attribute__((section(".dtcm")));
video_mode_t current_mode       __attribute__((section(".dtcm")));

/* The following table lists the pixel ratio of columns and rows
 * relative to a 768x384 frame buffer resolution.
 */
int resolution[][3] __attribute__((section(".dtcm"))) = {
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

uint8_t colors[] __attribute__((section(".dtcm"))) = {
        FB_LIGHT_GREEN,
        FB_YELLOW,
        FB_LIGHT_BLUE,
        FB_LIGHT_RED,

        FB_WHITE,
        FB_CYAN,
        FB_LIGHT_MAGENTA,
        FB_BROWN,
};

uint16_t colors16[] __attribute__((section(".dtcm"))) = {
        (FB_LIGHT_GREEN<<8)   | FB_LIGHT_GREEN,
        (FB_YELLOW<<8)        | FB_YELLOW,
        (FB_LIGHT_BLUE<<8)    | FB_LIGHT_BLUE,
        (FB_LIGHT_RED<<8)     | FB_LIGHT_RED,

        (FB_WHITE<<8)         | FB_WHITE,
        (FB_CYAN<<8)          | FB_CYAN,
        (FB_LIGHT_MAGENTA<<8) | FB_LIGHT_MAGENTA,
        (FB_BROWN<<8)         | FB_BROWN,
};

uint32_t color_translation_32[8][16] __attribute__((section(".dtcm"))) = {0};

uint32_t color_artifact_0[16]       __attribute__((section(".dtcm"))) = {0};
uint32_t color_artifact_1[16]       __attribute__((section(".dtcm"))) = {0};
uint32_t color_artifact_0r[16]      __attribute__((section(".dtcm"))) = {0};
uint32_t color_artifact_1r[16]      __attribute__((section(".dtcm"))) = {0};

uint32_t color_artifact_mono_0[16]  __attribute__((section(".dtcm"))) = {0};
uint32_t color_artifact_mono_1[16]  __attribute__((section(".dtcm"))) = {0};

uint32_t color_artifact_green0[16]  __attribute__((section(".dtcm"))) = {0};
uint32_t color_artifact_green1[16]  __attribute__((section(".dtcm"))) = {0};


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
    uint8_t buf[4];
    int buffer_index;
    
    video_ram_offset = 0x02;    // For offset 0x400 text screen
    sam_video_mode = 0;         // Alphanumeric

    /* Default startup mode of Dragon 32
     */
    current_mode = ALPHA_INTERNAL;

    sam_2x_rez = 1;

    // --------------------------------------------------------------------------
    // Pre-render the 2-color modes for fast look-up and 32-bit writes for speed
    // --------------------------------------------------------------------------
    for (int color = 0; color < 8; color++)
    {
        for (int i=0; i<16; i++)
        {
            switch (i)
            {
                case 0x0: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0x1: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0x2: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0x3: color_translation_32[color][i] = (FB_BLACK<<0)       | (FB_BLACK<<8)      | (colors[color]<<16) | (colors[color]<<24);    break;
                case 0x4: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0x5: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0x6: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0x7: color_translation_32[color][i] = (FB_BLACK<<0)       | (colors[color]<<8) | (colors[color]<<16) | (colors[color]<<24);    break;
                case 0x8: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0x9: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0xA: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0xB: color_translation_32[color][i] = (colors[color]<<0)  | (FB_BLACK<<8)      | (colors[color]<<16) | (colors[color]<<24);    break;
                case 0xC: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (FB_BLACK<<16)      | (FB_BLACK<<24);         break;
                case 0xD: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (FB_BLACK<<16)      | (colors[color]<<24);    break;
                case 0xE: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (colors[color]<<16) | (FB_BLACK<<24);         break;
                case 0xF: color_translation_32[color][i] = (colors[color]<<0)  | (colors[color]<<8) | (colors[color]<<16) | (colors[color]<<24);    break;
            }
        }
    }

    // ---------------------------------------------------------------------------------
    // Pre-render the artifact color modes for fast look-up and 32-bit writes for speed
    // ---------------------------------------------------------------------------------
    for (int pixels_byte=0; pixels_byte<16; pixels_byte++)
    {
        uint8_t buf2[4];
        uint8_t pixel = 0;
        uint8_t pixel2 = 0;
        uint8_t last_pixel;
        
        buffer_index = 0;
        last_pixel = FB_BLACK;
        for ( uint8_t element = 0x08; element != 0; element >>= 1)
        {
            if (pixels_byte & element)
            {
                pixel = FB_WHITE;
                pixel2 = FB_WHITE;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = (buffer_index & 1) ? ARTIFACT_BLUE : ARTIFACT_ORANGE;
                    pixel2 = (buffer_index & 1) ? ARTIFACT_ORANGE : ARTIFACT_BLUE;
                }
            }
            else
            {
                pixel = FB_BLACK;
                pixel2 = FB_BLACK;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = (buffer_index & 1) ? ARTIFACT_ORANGE : ARTIFACT_BLUE;
                    pixel2 = (buffer_index & 1) ? ARTIFACT_BLUE : ARTIFACT_ORANGE;
                }
            }

            buf[buffer_index] = pixel;
            buf2[buffer_index++] = pixel2;
        }

        color_artifact_0[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);
        color_artifact_0r[pixels_byte] = (buf2[3] << 24) | (buf2[2] << 16) | (buf2[1] << 8) | (buf2[0] << 0);


        buffer_index = 0;
        last_pixel = FB_WHITE;
        for (uint8_t element = 0x08; element != 0; element >>= 1)
        {
            if (pixels_byte & element)
            {
                pixel = FB_WHITE;
                pixel2 = FB_WHITE;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = (buffer_index & 1) ? ARTIFACT_BLUE : ARTIFACT_ORANGE;
                    pixel2 = (buffer_index & 1) ? ARTIFACT_ORANGE : ARTIFACT_BLUE;
                }
            }
            else
            {
                pixel = FB_BLACK;
                pixel2 = FB_BLACK;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = (buffer_index & 1) ? ARTIFACT_ORANGE : ARTIFACT_BLUE;
                    pixel2 = (buffer_index & 1) ? ARTIFACT_BLUE : ARTIFACT_ORANGE;
                }
            }

            buf[buffer_index] = pixel;
            buf2[buffer_index++] = pixel2;
        }

        color_artifact_1[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);
        color_artifact_1r[pixels_byte] = (buf2[3] << 24) | (buf2[2] << 16) | (buf2[1] << 8) | (buf2[0] << 0);
    }

    for (int pixels_byte=0; pixels_byte<16; pixels_byte++)
    {
        buffer_index = 0;
        for (uint8_t element = 0x08; element != 0; element >>= 1)
        {
            buf[buffer_index++] = (pixels_byte & element) ? FB_WHITE : FB_BLACK;
        }
        color_artifact_mono_0[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);
        
        buffer_index = 0;
        for (uint8_t element = 0x08; element != 0; element >>= 1)
        {
            buf[buffer_index++] = (pixels_byte & element) ? FB_LIGHT_GREEN : FB_BLACK;
        }
        color_artifact_mono_1[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);
    }
    
    
    // ---------------------------------------------------------------------------------
    // Pre-render the artifact color modes for fast look-up and 32-bit writes for speed
    // This one is for the "muddied green artifacting" when using the green color set.
    // ---------------------------------------------------------------------------------
    for (int pixels_byte=0; pixels_byte<16; pixels_byte++)
    {
        uint8_t pixel = 0;
        uint8_t last_pixel;
        
        buffer_index = 0;
        last_pixel = FB_BLACK;
        for ( uint8_t element = 0x08; element != 0; element >>= 1)
        {
            if (pixels_byte & element)
            {
                pixel = FB_LIGHT_GREEN;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = FB_GREEN;
                }
            }
            else
            {
                pixel = FB_BLACK;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = FB_GREEN;
                }
            }

            buf[buffer_index++] = pixel;
        }

        color_artifact_green0[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);


        buffer_index = 0;
        last_pixel = FB_LIGHT_GREEN;
        for (uint8_t element = 0x08; element != 0; element >>= 1)
        {
            if (pixels_byte & element)
            {
                pixel = FB_LIGHT_GREEN;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = FB_GREEN;
                }
            }
            else
            {
                pixel = FB_BLACK;
                if (pixel != last_pixel)
                {
                    last_pixel = pixel;
                    pixel  = FB_GREEN;
                }
            }

            buf[buffer_index++] = pixel;
        }

        color_artifact_green1[pixels_byte] = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0] << 0);
    }    
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
            vdg_render_resl_graph(current_mode, vdg_mem_base);
            break;

        case GRAPHICS_6R:
            if (myConfig.artifacts == 2)
            {
                vdg_render_artifacting_mono(current_mode, vdg_mem_base);
            }
            else
                vdg_render_artifacting(current_mode, vdg_mem_base);
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
    int         c, row, col, font_row;
    int         char_index, row_address;
    uint8_t     bit_pattern;
    uint8_t     color_set, fg_color;

    uint32_t    *screen_buffer = (uint32_t *)0x06000000;

    if ( pia_video_mode & PIA_COLOR_SET )
        color_set = DEF_COLOR_CSS_1;
    else
        color_set = DEF_COLOR_CSS_0;

    for ( row = 0; row < SCREEN_HEIGHT_CHAR; row++ )
    {
        row_address = row * SCREEN_WIDTH_CHAR + vdg_mem_base;

        for ( font_row = 0; font_row < FONT_HEIGHT; font_row++ )
        {
            for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
            {
                c = memory_RAM[col + row_address];

                /* Mode dependent initialization
                 * for text or semigraphics 4:
                 * - Determine foreground and background colors
                 * - Character pattern array
                 * - Character code index to bit pattern array
                 *
                 */
                if ( (uint8_t)c & CHAR_SEMI_GRAPHICS )
                {
                    fg_color = (((uint8_t)c & 0b01110000) >> 4);
                    char_index = (int)(((uint8_t) c) & SEMI_GRAPH4_MASK);
                    bit_pattern = semi_graph_4[char_index][font_row];
                }
                else
                {
                    fg_color = color_set;

                    char_index = (int)(((uint8_t) c) & ~(CHAR_SEMI_GRAPHICS | CHAR_INVERSE));
                    bit_pattern = font_img5x7[char_index][font_row];
                    if ( (uint8_t)c & CHAR_INVERSE )
                    {
                        bit_pattern = ~bit_pattern;
                    }
                }

                /* Render a row of pixels directly to the screen buffer - 32-bit speed!
                 */
                *screen_buffer++ = color_translation_32[fg_color][bit_pattern >> 4];
                *screen_buffer++ = color_translation_32[fg_color][bit_pattern & 0xF];
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
ITCM_CODE void vdg_render_semi6(int vdg_mem_base)
{
    int         c, row, col, font_row, font_col, color_set;
    int         char_index, row_address;
    uint8_t     bit_pattern, pix_pos;
    uint8_t     fg_color, bg_color;

    uint32_t    *screen_buffer;

    screen_buffer = (uint32_t *)0x06000000;
    color_set = (int)(4 * (pia_video_mode & PIA_COLOR_SET));

    for ( row = 0; row < SCREEN_HEIGHT_CHAR; row++ )
    {
        row_address = row * SCREEN_WIDTH_CHAR + vdg_mem_base;

        for ( font_row = 0; font_row < FONT_HEIGHT; font_row++ )
        {
            for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
            {
                c = memory_RAM[col + row_address];

                bg_color = FB_BLACK;
                fg_color = colors[(int)(((c & 0b11000000) >> 6) + color_set)];

                char_index = (int)(((uint8_t) c) & SEMI_GRAPH6_MASK);
                bit_pattern = semi_graph_6[char_index][font_row];

                /* Render a row of pixels in a temporary buffer
                 */
                pix_pos = 0x80;

                uint8_t buf[8];
                for ( font_col = 0; font_col < FONT_WIDTH; font_col++ )
                {
                    /* Bit is set in Font, print pixel(s) in text color
                    */
                    if ( (bit_pattern & pix_pos) )
                    {
                        buf[font_col] = fg_color;
                    }
                    /* Bit is cleared in Font
                    */
                    else
                    {
                        buf[font_col] = bg_color;
                    }

                    /* Move to the next pixel position
                    */
                    pix_pos = pix_pos >> 1;
                }

                uint32_t *ptr32 = (uint32_t *)&buf;
                *screen_buffer++ = *ptr32++;
                *screen_buffer++ = *ptr32++;
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
ITCM_CODE void vdg_render_semi_ext(video_mode_t mode, int vdg_mem_base)
{
    int         row, seg_row, scan_line, col, font_col, font_row;
    int         segments, seg_scan_lines;
    int         c, char_index, row_address;
    uint8_t     bit_pattern, pix_pos;
    uint8_t     color_set, fg_color, bg_color;
    uint32_t   *screen_buffer;

    screen_buffer = (uint32_t *)0x06000000;
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
        uint8_t buf[8];

        for ( seg_row = 0; seg_row < segments; seg_row++ )
        {
            row_address = (row * segments + seg_row) * SCREEN_WIDTH_CHAR + vdg_mem_base;

            for ( scan_line = 0; scan_line < seg_scan_lines; scan_line++ )
            {
                for ( col = 0; col < SCREEN_WIDTH_CHAR; col++ )
                {
                    c = memory_RAM[col + row_address];

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

                        char_index = (int)(((uint8_t) c) & ~(CHAR_SEMI_GRAPHICS | CHAR_INVERSE));
                        bit_pattern = font_img5x7[char_index][font_row];

                        if ( (uint8_t)c & CHAR_INVERSE )
                        {
                            bit_pattern = ~bit_pattern;
                        }
                    }

                    if (!bit_pattern) // Background - always black
                    {
                        *screen_buffer++ = 0x00000000;
                        *screen_buffer++ = 0x00000000;
                    }
                    else
                    {
                        /* Render a row of pixels in a temporary buffer
                        */
                        pix_pos = 0x80;

                        for ( font_col = 0; font_col < FONT_WIDTH; font_col++ )
                        {
                            /* Bit is set in Font, print pixel(s) in text color
                            */
                            if ( (bit_pattern & pix_pos) )
                            {
                                buf[font_col] = fg_color;
                            }
                            /* Bit is cleared in Font
                            */
                            else
                            {
                                buf[font_col] = bg_color;
                            }

                            /* Move to the next pixel position
                            */
                            pix_pos = pix_pos >> 1;
                        }

                        uint32_t *ptr32 = (uint32_t *)&buf;
                        *screen_buffer++ = *ptr32++;
                        *screen_buffer++ = *ptr32++;
                    }
                }

                if (++font_row == FONT_HEIGHT) font_row = 0;
            }
        }
    }
}

/*------------------------------------------------
 * vdg_render_resl_graph()
 *
 *  Render high resolution graphics modes:
 *  GRAPHICS_1R, GRAPHICS_2R, GRAPHICS_3R.
 *
 * param:  Mode, base address of video memory buffer.
 * return: none
 *
 */
ITCM_CODE void vdg_render_resl_graph(video_mode_t mode, int vdg_mem_base)
{
    int         i, vdg_mem_offset, element, buffer_index;
    int         video_mem, row_rep;
    uint8_t     pixels_byte, fg_color, pixel;
    uint8_t    *screen_buffer;
    uint8_t     pixel_row[SCREEN_WIDTH_PIX+16];

    screen_buffer = (uint8_t *) (0x06000000);

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

    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem / sam_2x_rez; vdg_mem_offset++)
    {
        pixels_byte = memory_RAM[vdg_mem_offset + vdg_mem_base];

        if (pixels_byte == 0x00)
        {
            memset(pixel_row+buffer_index, FB_BLACK, 16);
            buffer_index += 16;
        }
        else if (pixels_byte == 0xFF)
        {
            memset(pixel_row+buffer_index, fg_color, 16);
            buffer_index += 16;
        }
        else
        for ( element = 0x80; element != 0; element = element >> 1)
        {
            if ( pixels_byte & element )
            {
                pixel = fg_color;
            }
            else
            {
                pixel = FB_BLACK;
            }

            // Expand 2x
            pixel_row[buffer_index++] = pixel;
            pixel_row[buffer_index++] = pixel;
        }

        if ( buffer_index >= SCREEN_WIDTH_PIX )
        {
            for ( i = 0; i < row_rep * sam_2x_rez; i++ )
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
ITCM_CODE void vdg_render_color_graph(video_mode_t mode, int vdg_mem_base)
{
    int         i, vdg_mem_offset;
    int         video_mem, row_rep, color_set, color;
    uint8_t     pixels_byte;
    uint8_t    *screen_buffer;
    uint8_t     pixel_row[SCREEN_WIDTH_PIX+16];

    screen_buffer = (uint8_t *) (0x06000000);

    video_mem = resolution[mode][RES_MEM];
    row_rep = resolution[mode][RES_ROW_REP];
    color_set = 4 * (pia_video_mode & PIA_COLOR_SET);

    uint16_t *pixRowPtr = (uint16_t *)pixel_row;
    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem; vdg_mem_offset++)
    {
        pixels_byte = memory_RAM[vdg_mem_offset + vdg_mem_base];

        color = (int)((pixels_byte >> 6)) + color_set;
        *pixRowPtr++ = colors16[color];
        if ( mode == GRAPHICS_1C ) *pixRowPtr++ = colors16[color];

        color = (int)((pixels_byte >> 4) & 0x03) + color_set;
        *pixRowPtr++ = colors16[color];
        if ( mode == GRAPHICS_1C ) *pixRowPtr++ = colors16[color];

        color = (int)((pixels_byte >> 2) & 0x03) + color_set;
        *pixRowPtr++ = colors16[color];
        if ( mode == GRAPHICS_1C ) *pixRowPtr++ = colors16[color];

        color = (int)((pixels_byte) & 0x03) + color_set;
        *pixRowPtr++ = colors16[color];
        if ( mode == GRAPHICS_1C ) *pixRowPtr++ = colors16[color];

        if ( pixRowPtr >= (uint16_t *)(pixel_row+SCREEN_WIDTH_PIX) )
        {
            for ( i = 0; i < row_rep; i++ )
            {
                memcpy(screen_buffer, pixel_row, SCREEN_WIDTH_PIX);
                screen_buffer += SCREEN_WIDTH_PIX;
            }

            pixRowPtr = (uint16_t *)pixel_row;
        }
    }
}


// --------------------------------------------------------------------
// Handler for GRAPHICS_6R - this one is high-rez with artifacting...
// It's the most complicated but also the hallmark of the NTSC Coco.
// --------------------------------------------------------------------
ITCM_CODE void vdg_render_artifacting(video_mode_t mode, int vdg_mem_base)
{
    int         vdg_mem_offset;
    int         video_mem;
    uint8_t     pixels_byte, fg_color;
    uint32_t   *screen_buffer;
    uint8_t     last_pixel = FB_BLACK;
    int         pix_char = 0;

    if ( pia_video_mode & PIA_COLOR_SET )
    {
        // This is the NTSC Black/White artifacting mode...
        fg_color = colors[DEF_COLOR_CSS_1];
    }
    else // Mono... just greens in this case
    {
        vdg_render_artifacting_green(mode, vdg_mem_base);
        return;
    }
    
    screen_buffer = (uint32_t *) (0x06000000);

    video_mem = resolution[mode][RES_MEM];
    uint8_t bDoubleRez = ((resolution[mode][RES_ROW_REP] * sam_2x_rez) > 1) ? 1:0;

    uint32_t fg32 = (fg_color << 24)        | (fg_color << 16)        | (fg_color << 8)        | (fg_color << 0);
    uint32_t or32 = (ARTIFACT_ORANGE << 24) | (ARTIFACT_ORANGE << 16) | (ARTIFACT_ORANGE << 8) | (ARTIFACT_ORANGE << 0);
    uint32_t bl32 = (ARTIFACT_BLUE << 24)   | (ARTIFACT_BLUE << 16)   | (ARTIFACT_BLUE << 8)   | (ARTIFACT_BLUE << 0);
    
    if (myConfig.artifacts) // Reverse normal artifacting
    {
        uint32_t temp = or32;
        or32 = bl32;  bl32=temp; // Swap Orange/Blue
    }

    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem / sam_2x_rez; vdg_mem_offset++)
    {
        pixels_byte = memory_RAM[vdg_mem_offset + vdg_mem_base];

        if (pixels_byte == 0x00) // All background color
        {
            *screen_buffer++ = 0;
            *screen_buffer++ = 0;
            last_pixel = FB_BLACK;
        }
        else if (pixels_byte == 0xFF) // All foreground color
        {
            *screen_buffer++ = fg32;
            *screen_buffer++ = fg32;
            last_pixel = FB_WHITE;
        }
        else if (pixels_byte == 0xAA) // All orange color
        {
            *screen_buffer++ = or32;
            *screen_buffer++ = or32;
            last_pixel = FB_BLACK;
        }
        else if (pixels_byte == 0x55) // All blue color
        {
            *screen_buffer++ = bl32;
            *screen_buffer++ = bl32;
            last_pixel = FB_WHITE;
        }
        else // Need to do this set of 8 pixels the hard way...
        {
            if (myConfig.artifacts) // Reverse normal artifacting
            {
                if (last_pixel)
                {
                    *screen_buffer++ = color_artifact_1r[(pixels_byte>>4) & 0x0F];
                }
                else
                {
                    *screen_buffer++ = color_artifact_0r[(pixels_byte>>4) & 0x0F];
                }
                
                if (pixels_byte & 0x10)
                {
                    *screen_buffer++ = color_artifact_1r[pixels_byte & 0x0F];
                }
                else
                {
                    *screen_buffer++ = color_artifact_0r[pixels_byte & 0x0F];
                }            
            }
            else
            {
                if (last_pixel)
                {
                    *screen_buffer++ = color_artifact_1[(pixels_byte>>4) & 0x0F];
                }
                else
                {
                    *screen_buffer++ = color_artifact_0[(pixels_byte>>4) & 0x0F];
                }
                
                if (pixels_byte & 0x10)
                {
                    *screen_buffer++ = color_artifact_1[pixels_byte & 0x0F];
                }
                else
                {
                    *screen_buffer++ = color_artifact_0[pixels_byte & 0x0F];
                }
            }

            last_pixel = (pixels_byte & 1) ? FB_WHITE : FB_BLACK;
        }

        // Check if full line rendered... 32 chars (256 pixels)
        if (++pix_char & 0x20)
        {
            pix_char = 0;
            if (bDoubleRez)
            {                
                memcpy(screen_buffer, screen_buffer-64, SCREEN_WIDTH_PIX);
                screen_buffer += 64;
            }
            last_pixel = ((memory_RAM[vdg_mem_offset + vdg_mem_base + 1] & 0xC0) == 0xC0) ? FB_WHITE : FB_BLACK;
        }
    }
}

ITCM_CODE void vdg_render_artifacting_green(video_mode_t mode, int vdg_mem_base)
{
    int         vdg_mem_offset;
    int         video_mem;
    uint8_t     pixels_byte, fg_color;
    uint32_t   *screen_buffer;
    uint8_t     last_pixel = FB_BLACK;
    int         pix_char = 0;

    fg_color = FB_LIGHT_GREEN;
    
    screen_buffer = (uint32_t *) (0x06000000);

    video_mem = resolution[mode][RES_MEM];
    uint8_t bDoubleRez = ((resolution[mode][RES_ROW_REP] * sam_2x_rez) > 1) ? 1:0;

    uint32_t fg32 = (fg_color << 24)        | (fg_color << 16)        | (fg_color << 8)        | (fg_color << 0);
    
    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem / sam_2x_rez; vdg_mem_offset++)
    {
        pixels_byte = memory_RAM[vdg_mem_offset + vdg_mem_base];

        if (pixels_byte == 0x00) // All background color
        {
            *screen_buffer++ = 0;
            *screen_buffer++ = 0;
            last_pixel = FB_BLACK;
        }
        else if (pixels_byte == 0xFF) // All foreground color
        {
            *screen_buffer++ = fg32;
            *screen_buffer++ = fg32;
            last_pixel = FB_LIGHT_GREEN;
        }
        else // Need to do this set of 8 pixels the hard way...
        {
            if (last_pixel)
            {
                *screen_buffer++ = color_artifact_green1[(pixels_byte>>4) & 0x0F];
            }
            else
            {
                *screen_buffer++ = color_artifact_green0[(pixels_byte>>4) & 0x0F];
            }
            
            if (pixels_byte & 0x10)
            {
                *screen_buffer++ = color_artifact_green1[pixels_byte & 0x0F];
            }
            else
            {
                *screen_buffer++ = color_artifact_green0[pixels_byte & 0x0F];
            }

            last_pixel = (pixels_byte & 1) ? FB_LIGHT_GREEN : FB_BLACK;
        }

        // Check if full line rendered... 32 chars (256 pixels)
        if (++pix_char & 0x20)
        {
            pix_char = 0;
            if (bDoubleRez)
            {                
                memcpy(screen_buffer, screen_buffer-64, SCREEN_WIDTH_PIX);
                screen_buffer += 64;
            }
            last_pixel = ((memory_RAM[vdg_mem_offset + vdg_mem_base + 1] & 0xC0) == 0xC0) ? FB_LIGHT_GREEN : FB_BLACK;
        }
    }
}

// ---------------------------------------------------------------------------------------------------
// For when we are not NTSC Arifacting - or if the user has selected no artifacting in configuration.
// This will render either a Black/White or Black/Green monochrome high-rez image at 256x192.
// ---------------------------------------------------------------------------------------------------
ITCM_CODE void vdg_render_artifacting_mono(video_mode_t mode, int vdg_mem_base)
{
    int         vdg_mem_offset;
    int         video_mem;
    uint8_t     pixels_byte, fg_color;
    uint32_t   *screen_buffer;
    uint8_t     pix_char = 0;

    if ( pia_video_mode & PIA_COLOR_SET )
    {
        fg_color = colors[DEF_COLOR_CSS_1];
    }
    else
    {
        fg_color = colors[DEF_COLOR_CSS_0];
    }    

    screen_buffer = (uint32_t *) (0x06000000);

    video_mem = resolution[mode][RES_MEM];
    uint8_t bDoubleRez = ((resolution[mode][RES_ROW_REP] * sam_2x_rez) > 1) ? 1:0;

    uint32_t fg32 = (fg_color << 24)        | (fg_color << 16)        | (fg_color << 8)        | (fg_color << 0);

    for ( vdg_mem_offset = 0; vdg_mem_offset < video_mem / sam_2x_rez; vdg_mem_offset++)
    {
        pixels_byte = memory_RAM[vdg_mem_offset + vdg_mem_base];

        if (pixels_byte == 0x00)
        {
            *screen_buffer++ = 0;
            *screen_buffer++ = 0;
        }
        else if (pixels_byte == 0xFF)
        {
            *screen_buffer++ = fg32;
            *screen_buffer++ = fg32;
        }
        else
        {
            if (fg_color)
            {
                *screen_buffer++ = color_artifact_mono_1[(pixels_byte>>4) & 0x0F];
                *screen_buffer++ = color_artifact_mono_1[pixels_byte & 0x0F];
            }
            else
            {
                *screen_buffer++ = color_artifact_mono_0[(pixels_byte>>4) & 0x0F];
                *screen_buffer++ = color_artifact_mono_0[pixels_byte & 0x0F];
            }
        }

        // Check if full line rendered... 32 chars (256 pixels)
        if (++pix_char & 0x20)
        {
            pix_char = 0;
            if (bDoubleRez)
            {                
                memcpy(screen_buffer, screen_buffer-64, SCREEN_WIDTH_PIX);
                screen_buffer += 64;
            }
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
video_mode_t vdg_get_mode(void)
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
        sam_2x_rez = 1;
        switch ( pia_video_mode & 0x0e  )
        {
            case 0x00:
                mode = GRAPHICS_1C;
                break;
            case 0x02:
                mode = GRAPHICS_1R;
                break;
            case 0x04:
                mode = GRAPHICS_2C;
                if (sam_video_mode == 0x04) mode = GRAPHICS_3C; // Bump up to 3K higher-rez mode
                break;
            case 0x06:
                mode = GRAPHICS_2R;
                break;
            case 0x08:
                mode = GRAPHICS_3C;
                if (sam_video_mode == 0x06) mode = GRAPHICS_6C; // Bump up to 6K higher-rez mode
                break;
            case 0x0a:
                mode = GRAPHICS_3R;
                break;
            case 0x0c:
                mode = GRAPHICS_6C;
                break;
            case 0x0e:
                mode = GRAPHICS_6R;
                if (sam_video_mode == 0x04) sam_2x_rez = 2;     // Essentially 256x96 using 3K
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
        else if ( sam_video_mode == 6 &&
                (pia_video_mode & 0x02) == 0 )
        {
            mode = SEMI_GRAPHICS_24;
        }
    }

    return mode;
}
