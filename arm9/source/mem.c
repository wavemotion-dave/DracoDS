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
io_handler_callback memory_io[MEMORY];  // IO Handler 
uint8_t  memory_RAM[MEMORY];            // 64K of RAM 
uint8_t  memory_ROM[MEMORY];            // 64K of ROM but only the upper 32K is ever mapped/used

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
    int i;

    for ( i = 0; i < MEMORY; i++ )
    {
        memory_RAM[i] = 0;
        memory_ROM[i] = 0xFF;
        memory_io[i] = do_nothing_io_handler;
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
    if (address & 0x8000)
    {
        if ((address & 0xFF00) == 0xFF00)
        {
            memory_io[address]((uint16_t) address, (uint8_t)data, MEM_WRITE);
        }
        else
        {
            if ( sam_rom_in ) return; // Else fall through and write below ... 64K mode
        }        
    }
    
    memory_RAM[address] = (uint8_t) data;
}


/*------------------------------------------------
 * mem_define_io()
 *
 *  Define IO device address range and optional callback handler
 *  Function clears ROM flag.
 *
 *  param:  Memory address range start to end, inclusive
 *          IO handler callback for the range or NULL
 *  return: ' 0' - write ok,
 */
int mem_define_io(int addr_start, int addr_end, io_handler_callback io_handler)
{
    int i;

    for (i = addr_start; i <= addr_end; i++)
    {
        if ( io_handler != 0L )
        {
            memory_io[i] = io_handler;
        }
    }

    return MEM_OK;
}

/*------------------------------------------------
 * mem_load_rom()
 *
 *  Load a memory range from a data buffer.
 *
 *  param:  Memory address start, source data buffer and
 *          number of data elements to load
 *  return: ' 0' - write ok,
 */
int mem_load_rom(int addr_start, const uint8_t *buffer, int length)
{
    for (int i = 0; i < length; i++)
    {
        memory_ROM[(i+addr_start)] = buffer[i];
    }

    return MEM_OK;
}

/*------------------------------------------------
 * do_nothing_io_handler()
 *
 *  A default do-nothing IO handler
 *
 *  param:  Nothing
 *  return: Nothing
 */
static uint8_t do_nothing_io_handler(uint16_t address, uint8_t data, mem_operation_t op)
{
    return data;
}

