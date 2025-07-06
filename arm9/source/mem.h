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
 * mem.h
 *
 *  Header file that defines the memory module interface
 *
 *  July 2, 2020
 *
 *******************************************************************/

#ifndef __MEM_H__
#define __MEM_H__

#include    <stdint.h>
#include    "sam.h"

#define     MEMORY_SIZE    65536       // 64K Byte for the full M6809 memory map

typedef enum
{
    MEM_READ,
    MEM_WRITE,
} mem_operation_t;

typedef uint8_t (*io_handler_callback)(uint16_t, uint8_t, mem_operation_t);

extern io_handler_callback callback_io[MEMORY_SIZE];

extern uint8_t  memory_RAM[MEMORY_SIZE];
extern uint8_t  memory_ROM[MEMORY_SIZE];
extern uint8_t  memory_IO[0x100];

/********************************************************************
 *  Memory module API
 */

void mem_init(void);

void mem_write(int address, int data);
void mem_define_io(int addr_start, int addr_end, io_handler_callback io_handler);
void mem_load_rom(int addr_start, const uint8_t *buffer, int length);


/*------------------------------------------------
 * mem_read()
 *
 *  Read memory address
 *
 *  param:  Memory address
 *  return: Memory content at address
 */
inline __attribute__((always_inline)) uint8_t mem_read(int address)
{
    if ((address & 0xFF00) == 0xFF00)
    {
        /* An attempt to read an IO address will trigger
         * the callback that may return an alternative value.
         */
        memory_RAM[address] = callback_io[address]((uint16_t) address, memory_RAM[address], MEM_READ);
    }
    else if (sam_registers.memory_map_type & address)
    {
        return memory_ROM[address];
    }
    
    // This handles Page #1 where upper RAM is mapped to lower address space
    return memory_RAM[sam_registers.map_upper_to_lower | address];
}

inline __attribute__((always_inline)) uint8_t mem_read_pc(int address)
{
    // See if this is a ROM address
    if (sam_registers.memory_map_type & address)
    {
        return memory_ROM[address];
    }
    
    // This handles Page #1 where upper RAM is mapped to lower address space
    return memory_RAM[sam_registers.map_upper_to_lower | address];
}

#endif  /* __MEM_H__ */
