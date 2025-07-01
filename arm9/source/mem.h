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



typedef enum
{
    MEM_TYPE_RAM,
    MEM_TYPE_ROM,
    MEM_TYPE_IO,
} memory_flag_t;

typedef struct
{
    uint8_t data_byte;
    memory_flag_t memory_type;
    io_handler_callback io_handler;
} memory_t;

extern memory_t memory_i[MEMORY];
extern uint8_t  memory[MEMORY];

/********************************************************************
 *  Memory module API
 */

void mem_init(void);

int  mem_read(int address);
void mem_write(int address, int data);
int  mem_define_rom(int addr_start, int addr_end);
int  mem_define_ram(int addr_start, int addr_end);
int  mem_define_io(int addr_start, int addr_end, io_handler_callback io_handler);
int  mem_load(int addr_start, const uint8_t *buffer, int length);

#endif  /* __MEM_H__ */
