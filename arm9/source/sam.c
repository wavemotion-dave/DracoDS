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
 * sam.c
 *
 *  Module that implements the Synchronous Address Multiplexer
 *  MC6883 / SN74LS785, as used in the Dragon 32 computer
 *
 *  February 6, 2021
 *
 *******************************************************************/
#include    <nds.h>
#include    <stdint.h>

#include    "mem.h"
#include    "sam.h"
#include    "vdg.h"

/* -----------------------------------------
   Local definitions
----------------------------------------- */

/* -----------------------------------------
   Module static functions
----------------------------------------- */
static uint8_t io_handler_vector_redirect(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_handler_sam_write(uint16_t address, uint8_t data, mem_operation_t op);

static uint8_t io_rom_mode(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_ram_mode(uint16_t address, uint8_t data, mem_operation_t op);

static uint8_t io_page_zero(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_page_one(uint16_t address, uint8_t data, mem_operation_t op);

/* -----------------------------------------
   Module vars
----------------------------------------- */
struct sam_reg_t sam_registers  __attribute__((section(".dtcm")));

/*------------------------------------------------
 * sam_init()
 *
 *  Initialize the SAM device
 *
 *  param:  Nothing
 *  return: Nothing
 */
void sam_init(void)
{
    mem_define_io(0xfff2, 0xffff, io_handler_vector_redirect);
    mem_define_io(0xffc0, 0xffdf, io_handler_sam_write);
    
    mem_define_io(0xffde, 0xffde, io_rom_mode);     // RAM/ROM (type 0 map)
    mem_define_io(0xffdf, 0xffdf, io_ram_mode);     // ALL-RAM (type 1 map)
    
    mem_define_io(0xffd4, 0xffd4, io_page_zero);    // Normal mapping
    mem_define_io(0xffd5, 0xffd5, io_page_one);     // Mapping upper RAM into lower 32K
    
    sam_registers.vdg_mode = 0;                     // Alphanumeric mode
    sam_registers.vdg_display_offset = 2;           // Dragon computer text page 0x0400
    sam_registers.page = 0;                         // 0=Normal, 1=Map upper 32K RAM to lower address space
    sam_registers.mpu_rate = 0;                     // For compatibility, not used. Fixed to 0.89MHz
    sam_registers.memory_size = 2;                  // For compatibility, not used. Fixed to 64K
    sam_registers.memory_map_type = 0x8000;         // 1=ROMs in place, 0=ALL RAM mode 64K
    sam_registers.map_upper_to_lower = 0x0000;      // No SAM offset to memory
}

/*------------------------------------------------
 * io_handler_vector_redirect()
 *
 *  IO call-back handler that will redirect CPU memory access from
 *  the normal vector area 0xfff2 through 0xffff to 0xbff2 through 0xbffff
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
static uint8_t io_handler_vector_redirect(uint16_t address, uint8_t data, mem_operation_t op)
{
    uint8_t response = 0;

    if ( op == MEM_READ )
    {
        response = (uint8_t) mem_read((int)(address & 0xbfff));
    }

    return response;
}

/*------------------------------------------------
 * io_handler_sam_write()
 *
 *  IO call-back handler to emulate writing/modifying SAM registers.
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
ITCM_CODE uint8_t io_handler_sam_write(uint16_t address, uint8_t data, mem_operation_t op)
{
    uint16_t    register_addr;

    if ( op == MEM_WRITE )
    {
        register_addr = address & 0x001f;
        switch ( register_addr )
        {
            /* VDG mode
             */
            case 0x00:
                sam_registers.vdg_mode &= 0xfe;
                break;

            case 0x01:
                sam_registers.vdg_mode |= 0x01;
                break;

            case 0x02:
                sam_registers.vdg_mode &= 0xfd;
                break;

            case 0x03:
                sam_registers.vdg_mode |= 0x02;
                break;

            case 0x04:
                sam_registers.vdg_mode &= 0xfb;
                break;

            case 0x05:
                sam_registers.vdg_mode |= 0x04;
                break;

            /* Display offset
             */
            case 0x06:
                sam_registers.vdg_display_offset &= 0xfe;
                break;

            case 0x07:
                sam_registers.vdg_display_offset |= 0x01;
                break;

            case 0x08:
                sam_registers.vdg_display_offset &= 0xfd;
                break;

            case 0x09:
                sam_registers.vdg_display_offset |= 0x02;
                break;

            case 0x0a:
                sam_registers.vdg_display_offset &= 0xfb;
                break;

            case 0x0b:
                sam_registers.vdg_display_offset |= 0x04;
                break;

            case 0x0c:
                sam_registers.vdg_display_offset &= 0xf7;
                break;

            case 0x0d:
                sam_registers.vdg_display_offset |= 0x08;
                break;

            case 0x0e:
                sam_registers.vdg_display_offset &= 0xef;
                break;

            case 0x0f:
                sam_registers.vdg_display_offset |= 0x10;
                break;

            case 0x10:
                sam_registers.vdg_display_offset &= 0xdf;
                break;

            case 0x11:
                sam_registers.vdg_display_offset |= 0x20;
                break;

            case 0x12:
                sam_registers.vdg_display_offset &= 0xbf;
                break;

            case 0x13:
                sam_registers.vdg_display_offset |= 0x40;
                break;
        }
    }

    /* Send VDG mode to VDG emulation module
     * and display offset address to VDG emulation module
     */
    vdg_set_mode_sam((int) sam_registers.vdg_mode);
    vdg_set_video_offset(sam_registers.vdg_display_offset);

    return 0;
}

// ----------------------------------------------------------------------
// 64K Emulation - mainly to allow swap of RAM/ROM mode for ALL-RAM mode
// ----------------------------------------------------------------------

static uint8_t io_rom_mode(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        sam_registers.memory_map_type = 0x8000;
        sam_registers.map_upper_to_lower = (sam_registers.page ? 0x8000:0x0000);
    }
    
    return 0;
}

static uint8_t io_ram_mode(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        sam_registers.memory_map_type = 0;
        sam_registers.map_upper_to_lower = 0x0000;
    }
    
    return 0;
}

static uint8_t io_page_zero(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        sam_registers.map_upper_to_lower = 0x0000;
        sam_registers.page = 0;
    }
    
    return 0;
}

static uint8_t io_page_one(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        sam_registers.map_upper_to_lower = (sam_registers.memory_map_type ? 0x8000 : 0x0000);
        sam_registers.page = 1;
    }
    
    return 0;
}
