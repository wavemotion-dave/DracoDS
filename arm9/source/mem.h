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

#define     MEMORY                  65536       // 64K Byte

#define     MEM_OK                  0           // Operation ok
#define     MEM_ADD_RANGE          -1           // Address out of range
#define     MEM_ROM                -2           // Location is ROM
#define     MEM_HANDLER_ERR        -3           // Cannot hook IO handler

typedef enum
{
    MEM_READ,
    MEM_WRITE,
} mem_operation_t;

typedef uint8_t (*io_handler_callback)(uint16_t, uint8_t, mem_operation_t);


extern io_handler_callback memory_io[MEMORY];

extern uint8_t  memory_RAM[MEMORY];
extern uint8_t  memory_ROM[MEMORY];

/********************************************************************
 *  Memory module API
 */

void mem_init(void);

void mem_write(int address, int data);
int  mem_define_io(int addr_start, int addr_end, io_handler_callback io_handler);
int  mem_load_rom(int addr_start, const uint8_t *buffer, int length);

extern uint8_t sam_rom_in;

inline __attribute__((always_inline)) uint8_t mem_read_pc(int address)
{
    if (sam_rom_in)
    {
        if (address & 0x8000) return memory_ROM[address];
    }
    
    return memory_RAM[address];
}

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
    if (address & 0x8000)
    {
        if ((address & 0xFF00) == 0xFF00)
        {
            /* An attempt to read an IO address will trigger
             * the callback that may return an alternative value.
             */
            memory_RAM[address] = memory_io[address]((uint16_t) address, memory_RAM[address], MEM_READ);
        }
        else
        if (sam_rom_in)
        {
            if (address & 0x8000) return memory_ROM[address];
        }
    }
    
    return memory_RAM[address];
}


#endif  /* __MEM_H__ */
