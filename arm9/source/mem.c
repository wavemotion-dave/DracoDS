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
 * mem.c
 *
 *  Memory module interface
 *
 *  July 2, 2020
 *
 *******************************************************************/
#include    <nds.h>

#include    "mem.h"
#include    "sam.h"

/* -----------------------------------------
   Local definitions
----------------------------------------- */
/* -----------------------------------------
   Module static functions
----------------------------------------- */
static uint8_t do_nothing_io_handler(uint16_t address, uint8_t data, mem_operation_t op);

/* -----------------------------------------
   Module globals
----------------------------------------- */
io_handler_callback callback_io[MEMORY_SIZE];  // IO Handler 
uint8_t  memory_RAM[MEMORY_SIZE];            // 64K of RAM 
uint8_t  memory_ROM[MEMORY_SIZE];            // 64K of ROM but only the upper 32K is ever mapped/used
uint8_t  memory_IO[MEMORY_SIZE];             // 256 bytes of IO Space

/*------------------------------------------------
 * mem_init()
 *
 *  Initialize the memory module
 *
 *  param:  Nothing
 *  return: Nothing
 */
void mem_init(void)
{
    for (int i = 0; i < MEMORY_SIZE; i++ )
    {
        memory_RAM[i] = 0x00;
        memory_ROM[i] = 0xFF;
        memory_IO[i]  = 0x00;
        callback_io[i] = do_nothing_io_handler;
    }
}


/*------------------------------------------------
 * mem_write()
 *
 *  Write to memory address
 *
 *  param:  Memory address and data to write
 *  return: Nothing
 */
ITCM_CODE void mem_write(int address, int data)
{
    if ((address & 0xFF00) == 0xFF00)
    {
        memory_IO[address] = callback_io[address]((uint16_t) address, (uint8_t)data, MEM_WRITE);
        return;
    }
    else
    {
        if ( sam_registers.memory_map_type & address ) return; // Check for ROMs... else fall through and write below (in 64K mode)
    }        
    
    memory_RAM[sam_registers.map_upper_to_lower | address] = (uint8_t) data;
}


/*------------------------------------------------
 * mem_define_io()
 *
 *  Define IO device address range and optional callback handler
 *  Function clears ROM flag.
 *
 *  param:  Memory address range start to end, inclusive
 *          IO handler callback for the range or NULL
 */
void mem_define_io(int addr_start, int addr_end, io_handler_callback io_handler)
{
    for (int i = addr_start; i <= addr_end; i++)
    {
        if ( io_handler != 0L )
        {
            callback_io[i] = io_handler;
        }
    }
}

/*------------------------------------------------
 * mem_load_rom()
 *
 *  Load a memory range from a data buffer.
 *
 *  param:  Memory address start, source data buffer and
 *          number of data elements to load
 */
void mem_load_rom(int addr_start, const uint8_t *buffer, int length)
{
    for (int i = 0; i < length; i++)
    {
        memory_ROM[(i+addr_start)] = buffer[i];
    }
}

/*------------------------------------------------
 * do_nothing_io_handler()
 *
 *  A default do-nothing IO handler
 *
 *  param:  Nothing
 *  return: Nothing
 */
extern unsigned int debug[];
static uint8_t do_nothing_io_handler(uint16_t address, uint8_t data, mem_operation_t op)
{
    return data;
}

