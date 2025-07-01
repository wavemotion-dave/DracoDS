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

#include    "mem.h"

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
memory_t memory_i[MEMORY];
uint8_t  memory[MEMORY];

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
        memory[i] = 0;
        memory_i[i].memory_type = MEM_TYPE_RAM;
        memory_i[i].io_handler = do_nothing_io_handler;
    }
}

/*------------------------------------------------
 * mem_read()
 *
 *  Read memory address
 *
 *  param:  Memory address
 *  return: Memory content at address
 */
int mem_read(int address)
{
    if ((address & 0xFF00) == 0xFF00)
    {
        if ( memory_i[address].memory_type == MEM_TYPE_IO)
        {
            /* An attempt to read an IO address will trigger
             * the callback that may return an alternative value.
             */
            memory[address] = memory_i[address].io_handler((uint16_t) address, memory[address], MEM_READ);
        }
    }

    return (int)(memory[address]);
}

/*------------------------------------------------
 * mem_write()
 *
 *  Write to memory address
 *
 *  param:  Memory address and data to write
 *  return: ' 0' - write ok,
 *          '-1' - memory location is out of range
 *          '-2' - memory location is ROM
 */
void mem_write(int address, int data)
{
    if (address & 0x8000)
    {
        if ( memory_i[address].memory_type == MEM_TYPE_ROM )
            return;
            
        memory[address] = (uint8_t) data;

        if ( memory_i[address].memory_type == MEM_TYPE_IO )
        {
            memory_i[address].io_handler((uint16_t) address, (uint8_t)data, MEM_WRITE);
        }
    }
    else memory[address] = (uint8_t) data;
}

/*------------------------------------------------
 * mem_define_rom()
 *
 *  Define address range as ROM.
 *  Function clears IO flag and callback index.
 *
 *  param:  Memory address range start and end, inclusive
 *  return: ' 0' - write ok,
 *          '-1' - memory location is out of range
 */
int  mem_define_rom(int addr_start, int addr_end)
{
    int i;

    if ( addr_start < 0 || addr_start > (MEMORY-1) ||
         addr_end < 0   || addr_end > (MEMORY-1)   ||
         addr_start > addr_end )
        return MEM_ADD_RANGE;

    for (i = addr_start; i <= addr_end; i++)
    {
        memory_i[i].memory_type = MEM_TYPE_ROM;
    }

    return MEM_OK;
}


/*------------------------------------------------
 * mem_define_ram()
 *
 *  Define address range as RAM.
 *  Function clears IO flag and callback index.
 *
 *  param:  Memory address range start and end, inclusive
 *  return: ' 0' - write ok,
 *          '-1' - memory location is out of range
 */
int  mem_define_ram(int addr_start, int addr_end)
{
    int i;

    if ( addr_start < 0 || addr_start > (MEMORY-1) ||
         addr_end < 0   || addr_end > (MEMORY-1)   ||
         addr_start > addr_end )
        return MEM_ADD_RANGE;

    for (i = addr_start; i <= addr_end; i++)
    {
        memory_i[i].memory_type = MEM_TYPE_RAM;
    }

    return MEM_OK;
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
 *          '-1' - memory location is out of range
 *          '-3' - Cannot hook IO handler
 */
int  mem_define_io(int addr_start, int addr_end, io_handler_callback io_handler)
{
    int i;

    if ( addr_start < 0 || addr_start > (MEMORY-1) ||
         addr_end < 0   || addr_end > (MEMORY-1)   ||
         addr_start > addr_end )
        return MEM_ADD_RANGE;

    for (i = addr_start; i <= addr_end; i++)
    {
        memory_i[i].memory_type = MEM_TYPE_IO;
        if ( io_handler != 0L )
            memory_i[i].io_handler = io_handler;
    }

    return MEM_OK;
}

/*------------------------------------------------
 * mem_load()
 *
 *  Load a memory range from a data buffer.
 *  Function clears IO, ROM, and callback index fields.
 *
 *  param:  Memory address start, source data buffer and
 *          number of data elements to load
 *  return: ' 0' - write ok,
 *          '-1' - memory location is out of range
 */
int mem_load(int addr_start, const uint8_t *buffer, int length)
{
    int i;

    if ( addr_start < 0 || addr_start > (MEMORY-1) ||
         (addr_start + length) > MEMORY )
        return MEM_ADD_RANGE;

    for (i = 0; i < length; i++)
    {
        memory[(i+addr_start)] = buffer[i];
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
    /* do nothing */
    /* TODO generate an exception? */
    return 0;
}

