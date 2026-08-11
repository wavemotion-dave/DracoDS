// =====================================================================================
// Copyright (c) 2025-2026 Dave Bernazzani (wavemotion-dave)
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
   Module static functions
----------------------------------------- */
static uint8_t do_nothing_io_handler(uint16_t address, uint8_t data, mem_operation_t op);

/* -----------------------------------------
   Module globals
----------------------------------------- */
io_handler_callback callback_io[MEMORY_SIZE];  // IO Handler - we only really use the back-end 256 entries here but space is not an issue
uint8_t  memory_RAM[MEMORY_SIZE];              // 64K of RAM - the last 256 bytes here served as IO space
uint8_t  memory_ROM[MEMORY_SIZE];              // 64K of ROM but only the upper 32K is ever mapped/used

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
        callback_io[i] = do_nothing_io_handler;
    }
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
 *  return: Last address byte to mimic floating IO hardware
 */
static uint8_t do_nothing_io_handler(uint16_t address, uint8_t data, mem_operation_t op)
{
    return address & 0xFF;  // Generally non-connected IO will return the last data bus byte fetched... in this case the low byte of the address
}

/*------------------------------------------------
 * io_read16()
 *
 *  I'm not sure this is even possible... but we don't inline
 *  it as I'm never really expecting it to be called...
 *
 *  param:  address to read
 *  return: 16-bit I/O value
 */
__attribute__((noinline)) uint16_t io_read16(uint16_t address)
{
    /* An attempt to read an IO address will trigger
     * the callback that may return an alternative value.
     */
    u8 hi = callback_io[address]((uint16_t) address, memory_RAM[address], MEM_READ);
    address++;
    u8 lo = callback_io[address]((uint16_t) address, memory_RAM[address], MEM_READ);
    return (hi << 8) | lo;
}
