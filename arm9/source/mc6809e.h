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


/*
 * mc6809e.h
 *
 * This header file lists MC6809E assembly op-codes,
 * command mnemonic, command cycle count, byte count, and
 * addressing mode.
 *
 * This static data structure is based on CPU data sheet
 * Motorola INC. 1984 DS9846-R2.
 *
 *  July 4, 2020
 *
 */

#ifndef __MC6809E_H__
#define __MC6809E_H__
#include    <nds.h>
#include    <stdint.h>

#define     ADDR_DIRECT             1
#define     ADDR_INHERENT           2
#define     ADDR_RELATIVE           3           // 8-bit address offset
#define     ADDR_LRELATIVE          4           // 16-bit address offset
#define     ADDR_INDEXED            5
#define     ADDR_EXTENDED           6
#define     ADDR_IMMEDIATE          7           // 8-bit immediate
#define     ADDR_LIMMEDIATE         8           // 16-bit immediate
#define     DOUBLE_BYTE             9           // Double byte commands starting with 0x10 or 0x11
#define     ILLEGAL_OP              10

typedef struct
{
    uint8_t  mode;
    uint8_t  cycles;
} machine_code_t;

machine_code_t machine_code[] __attribute__((section(".dtcm"))) = 
{
    {ADDR_DIRECT    , 6}, // 0x00 - neg
    {ADDR_DIRECT    , 6}, // 0x01 - illegal (acts like neg)
    {ADDR_DIRECT    , 6}, // 0x02 - illegal (acts like com)
    {ADDR_DIRECT    , 6}, // 0x03 - com
    {ADDR_DIRECT    , 6}, // 0x04 - lsr
    {ADDR_DIRECT    , 6}, // 0x05 - illegal (acts like lsr)
    {ADDR_DIRECT    , 6}, // 0x06 - ror
    {ADDR_DIRECT    , 6}, // 0x07 - asr
    {ADDR_DIRECT    , 6}, // 0x08 - lsl / asl
    {ADDR_DIRECT    , 6}, // 0x09 - rol
    {ADDR_DIRECT    , 6}, // 0x0a - dec
    {ADDR_DIRECT    , 6}, // 0x0b - illegal (acts like dec)
    {ADDR_DIRECT    , 6}, // 0x0c - inc
    {ADDR_DIRECT    , 6}, // 0x0d - tst
    {ADDR_DIRECT    , 3}, // 0x0e - jmp
    {ADDR_DIRECT    , 6}, // 0x0f - clr

    {DOUBLE_BYTE    , 0}, // 0x10 - page2
    {DOUBLE_BYTE    , 0}, // 0x11 - page3
    {ADDR_INHERENT  , 2}, // 0x12 - nop
    {ADDR_INHERENT  , 4}, // 0x13 - sync
    {ILLEGAL_OP     , 0}, // 0x14 - illegal
    {ILLEGAL_OP     , 0}, // 0x15 - illegal
    {ADDR_LRELATIVE , 5}, // 0x16 - lbra
    {ADDR_LRELATIVE , 9}, // 0x17 - lbsr
    {ILLEGAL_OP     , 0}, // 0x18 - illegal
    {ADDR_INHERENT  , 2}, // 0x19 - daa
    {ADDR_IMMEDIATE , 3}, // 0x1a - orcc
    {ADDR_INHERENT  , 2}, // 0x1b - mul
    {ADDR_IMMEDIATE , 3}, // 0x1c - andcc
    {ADDR_INHERENT  , 2}, // 0x1d - sex
    {ADDR_IMMEDIATE , 8}, // 0x1e - exg
    {ADDR_IMMEDIATE , 6}, // 0x1f - tfr

    {ADDR_RELATIVE  , 3}, // 0x20 - bra
    {ADDR_RELATIVE  , 3}, // 0x21 - brn
    {ADDR_RELATIVE  , 3}, // 0x22 - bhi
    {ADDR_RELATIVE  , 3}, // 0x23 - bls
    {ADDR_RELATIVE  , 3}, // 0x24 - bhs / bcc
    {ADDR_RELATIVE  , 3}, // 0x25 - blo / bcs
    {ADDR_RELATIVE  , 3}, // 0x26 - bne
    {ADDR_RELATIVE  , 3}, // 0x27 - beq
    {ADDR_RELATIVE  , 3}, // 0x28 - bvc
    {ADDR_RELATIVE  , 3}, // 0x29 - bvs
    {ADDR_RELATIVE  , 3}, // 0x2a - bpl
    {ADDR_RELATIVE  , 3}, // 0x2b - bmi
    {ADDR_RELATIVE  , 3}, // 0x2c - bge
    {ADDR_RELATIVE  , 3}, // 0x2d - blt
    {ADDR_RELATIVE  , 3}, // 0x2e - bgt
    {ADDR_RELATIVE  , 3}, // 0x2f - ble

    {ADDR_INDEXED   , 4}, // 0x30 - leax
    {ADDR_INDEXED   , 4}, // 0x31 - leay
    {ADDR_INDEXED   , 4}, // 0x32 - leas
    {ADDR_INDEXED   , 4}, // 0x33 - leau
    {ADDR_IMMEDIATE , 5}, // 0x34 - pshs
    {ADDR_IMMEDIATE , 5}, // 0x35 - puls
    {ADDR_IMMEDIATE , 5}, // 0x36 - pshu
    {ADDR_IMMEDIATE , 5}, // 0x37 - pulu
    {ILLEGAL_OP     , 0}, // 0x38 - illegal
    {ADDR_INHERENT  , 5}, // 0x39 - rts
    {ADDR_INHERENT  , 3}, // 0x3a - abx
    {ADDR_INHERENT  , 6}, // 0x3b - rti
    {ADDR_IMMEDIATE , 20},// 0x3c - cwai
    {ADDR_INHERENT  , 11},// 0x3d - mul (or nul on 6809)
    {ILLEGAL_OP     , 0}, // 0x3e - illegal
    {ADDR_INHERENT  , 19},// 0x3f - swi

    {ADDR_INHERENT  , 2}, // 0x40 - nega
    {ILLEGAL_OP     , 0}, // 0x41 - illegal
    {ILLEGAL_OP     , 0}, // 0x42 - illegal
    {ADDR_INHERENT  , 2}, // 0x43 - coma
    {ADDR_INHERENT  , 2}, // 0x44 - lsra
    {ILLEGAL_OP     , 0}, // 0x45 - illegal
    {ADDR_INHERENT  , 2}, // 0x46 - rora
    {ADDR_INHERENT  , 2}, // 0x47 - asra
    {ADDR_INHERENT  , 2}, // 0x48 - lsla / asla
    {ADDR_INHERENT  , 2}, // 0x49 - rola
    {ADDR_INHERENT  , 2}, // 0x4a - deca
    {ILLEGAL_OP     , 0}, // 0x4b - illegal
    {ADDR_INHERENT  , 2}, // 0x4c - inca
    {ADDR_INHERENT  , 2}, // 0x4d - tsta
    {ILLEGAL_OP     , 0}, // 0x4e - illegal
    {ADDR_INHERENT  , 2}, // 0x4f - clra

    {ADDR_INHERENT  , 2}, // 0x50 - negb
    {ILLEGAL_OP     , 0}, // 0x51 - illegal
    {ILLEGAL_OP     , 0}, // 0x52 - illegal
    {ADDR_INHERENT  , 2}, // 0x53 - comb
    {ADDR_INHERENT  , 2}, // 0x54 - lsrb
    {ILLEGAL_OP     , 0}, // 0x55 - illegal
    {ADDR_INHERENT  , 2}, // 0x56 - rorb
    {ADDR_INHERENT  , 2}, // 0x57 - asrb
    {ADDR_INHERENT  , 2}, // 0x58 - lslb / aslb
    {ADDR_INHERENT  , 2}, // 0x59 - rolb
    {ADDR_INHERENT  , 2}, // 0x5a - decb
    {ILLEGAL_OP     , 0}, // 0x5b - illegal
    {ADDR_INHERENT  , 2}, // 0x5c - incb
    {ADDR_INHERENT  , 2}, // 0x5d - tstb
    {ILLEGAL_OP     , 0}, // 0x5e - illegal
    {ADDR_INHERENT  , 2}, // 0x5f - clrb

    {ADDR_INDEXED   , 6}, // 0x60 - neg
    {ADDR_INDEXED   , 6}, // 0x61 - illegal (acts like neg)
    {ADDR_INDEXED   , 6}, // 0x62 - illegal (acts like com)
    {ADDR_INDEXED   , 6}, // 0x63 - com
    {ADDR_INDEXED   , 6}, // 0x64 - lsr
    {ILLEGAL_OP     , 0}, // 0x65 - illegal (acts like lsr)
    {ADDR_INDEXED   , 6}, // 0x66 - ror
    {ADDR_INDEXED   , 6}, // 0x67 - asr
    {ADDR_INDEXED   , 6}, // 0x68 - lsl / asl
    {ADDR_INDEXED   , 6}, // 0x69 - rol
    {ADDR_INDEXED   , 6}, // 0x6a - dec
    {ILLEGAL_OP     , 0}, // 0x6b - illegal
    {ADDR_INDEXED   , 6}, // 0x6c - inc
    {ADDR_INDEXED   , 6}, // 0x6d - tst
    {ADDR_INDEXED   , 3}, // 0x6e - jmp
    {ADDR_INDEXED   , 6}, // 0x6f - clr

    {ADDR_EXTENDED  , 7}, // 0x70 - neg
    {ADDR_EXTENDED  , 7}, // 0x71 - illegal (acts like neg)
    {ADDR_EXTENDED  , 7}, // 0x72 - illegal (acts like com)
    {ADDR_EXTENDED  , 7}, // 0x73 - com
    {ADDR_EXTENDED  , 7}, // 0x74 - lsr
    {ILLEGAL_OP     , 0}, // 0x75 - illegal (acts like lsr)
    {ADDR_EXTENDED  , 7}, // 0x76 - ror
    {ADDR_EXTENDED  , 7}, // 0x77 - asr
    {ADDR_EXTENDED  , 7}, // 0x78 - lsl / asl
    {ADDR_EXTENDED  , 7}, // 0x79 - rol
    {ADDR_EXTENDED  , 7}, // 0x7a - dec
    {ILLEGAL_OP     , 0}, // 0x7b - illegal
    {ADDR_EXTENDED  , 7}, // 0x7c - inc
    {ADDR_EXTENDED  , 7}, // 0x7d - tst
    {ADDR_EXTENDED  , 4}, // 0x7e - jmp
    {ADDR_EXTENDED  , 7}, // 0x7f - clr

    {ADDR_IMMEDIATE , 2}, // 0x80 - suba
    {ADDR_IMMEDIATE , 2}, // 0x81 - cmpa
    {ADDR_IMMEDIATE , 2}, // 0x82 - sbca
    {ADDR_LIMMEDIATE, 4}, // 0x83 - subd
    {ADDR_IMMEDIATE , 2}, // 0x84 - anda
    {ADDR_IMMEDIATE , 2}, // 0x85 - bita
    {ADDR_IMMEDIATE , 2}, // 0x86 - lda
    {ADDR_IMMEDIATE , 2}, // 0x87 - illegal
    {ADDR_IMMEDIATE , 2}, // 0x88 - eora
    {ADDR_IMMEDIATE , 2}, // 0x89 - adca
    {ADDR_IMMEDIATE , 2}, // 0x8a - ora
    {ADDR_IMMEDIATE , 2}, // 0x8b - adda
    {ADDR_LIMMEDIATE, 4}, // 0x8c - cmpx
    {ADDR_RELATIVE  , 7}, // 0x8d - bsr
    {ADDR_LIMMEDIATE, 3}, // 0x8e - ldx
    {ILLEGAL_OP     , 0}, // 0x8f - illegal

    {ADDR_DIRECT    , 4}, // 0x90 - suba
    {ADDR_DIRECT    , 4}, // 0x91 - cmpa
    {ADDR_DIRECT    , 4}, // 0x92 - sbca
    {ADDR_DIRECT    , 6}, // 0x93 - subd
    {ADDR_DIRECT    , 4}, // 0x94 - anda
    {ADDR_DIRECT    , 4}, // 0x95 - bita
    {ADDR_DIRECT    , 4}, // 0x96 - lda
    {ADDR_DIRECT    , 4}, // 0x97 - sta
    {ADDR_DIRECT    , 4}, // 0x98 - eora
    {ADDR_DIRECT    , 4}, // 0x99 - adca
    {ADDR_DIRECT    , 4}, // 0x9a - ora
    {ADDR_DIRECT    , 4}, // 0x9b - adda
    {ADDR_DIRECT    , 6}, // 0x9c - cmpx
    {ADDR_DIRECT    , 7}, // 0x9d - jsr
    {ADDR_DIRECT    , 5}, // 0x9e - ldx
    {ADDR_DIRECT    , 5}, // 0x9f - stx

    {ADDR_INDEXED   , 4}, // 0xa0 - suba
    {ADDR_INDEXED   , 4}, // 0xa1 - cmpa
    {ADDR_INDEXED   , 4}, // 0xa2 - sbca
    {ADDR_INDEXED   , 6}, // 0xa3 - subd
    {ADDR_INDEXED   , 4}, // 0xa4 - anda
    {ADDR_INDEXED   , 4}, // 0xa5 - bita
    {ADDR_INDEXED   , 4}, // 0xa6 - lda
    {ADDR_INDEXED   , 4}, // 0xa7 - sta
    {ADDR_INDEXED   , 4}, // 0xa8 - eora
    {ADDR_INDEXED   , 4}, // 0xa9 - adca
    {ADDR_INDEXED   , 4}, // 0xaa - ora
    {ADDR_INDEXED   , 4}, // 0xab - adda
    {ADDR_INDEXED   , 6}, // 0xac - cmpx
    {ADDR_INDEXED   , 7}, // 0xad - jsr
    {ADDR_INDEXED   , 5}, // 0xae - ldx
    {ADDR_INDEXED   , 5}, // 0xaf - stx

    {ADDR_EXTENDED  , 5}, // 0xb0 - suba
    {ADDR_EXTENDED  , 5}, // 0xb1 - cmpa
    {ADDR_EXTENDED  , 5}, // 0xb2 - sbca
    {ADDR_EXTENDED  , 7}, // 0xb3 - subd
    {ADDR_EXTENDED  , 5}, // 0xb4 - anda
    {ADDR_EXTENDED  , 5}, // 0xb5 - bita
    {ADDR_EXTENDED  , 5}, // 0xb6 - lda
    {ADDR_EXTENDED  , 5}, // 0xb7 - sta
    {ADDR_EXTENDED  , 5}, // 0xb8 - eora
    {ADDR_EXTENDED  , 5}, // 0xb9 - adca
    {ADDR_EXTENDED  , 5}, // 0xba - ora
    {ADDR_EXTENDED  , 5}, // 0xbb - adda
    {ADDR_EXTENDED  , 7}, // 0xbc - cmpx
    {ADDR_EXTENDED  , 8}, // 0xbd - jsr
    {ADDR_EXTENDED  , 6}, // 0xbe - ldx
    {ADDR_EXTENDED  , 6}, // 0xbf - stx

    {ADDR_IMMEDIATE , 2}, // 0xc0 - subb
    {ADDR_IMMEDIATE , 2}, // 0xc1 - cmpb
    {ADDR_IMMEDIATE , 2}, // 0xc2 - sbcb
    {ADDR_LIMMEDIATE, 4}, // 0xc3 - addd
    {ADDR_IMMEDIATE , 2}, // 0xc4 - andb
    {ADDR_IMMEDIATE , 2}, // 0xc5 - bitb
    {ADDR_IMMEDIATE , 2}, // 0xc6 - ldb
    {ADDR_IMMEDIATE , 2}, // 0xc7 - illegal
    {ADDR_IMMEDIATE , 2}, // 0xc8 - eorb
    {ADDR_IMMEDIATE , 2}, // 0xc9 - adcb
    {ADDR_IMMEDIATE , 2}, // 0xca - orb
    {ADDR_IMMEDIATE , 2}, // 0xcb - addb
    {ADDR_LIMMEDIATE, 3}, // 0xcc - ldd
    {ILLEGAL_OP     , 0}, // 0xcd - illegal
    {ADDR_LIMMEDIATE, 3}, // 0xce - ldu
    {ILLEGAL_OP     , 0}, // 0xcf - illegal

    {ADDR_DIRECT    , 4}, // 0xd0 - subb
    {ADDR_DIRECT    , 4}, // 0xd1 - cmpb
    {ADDR_DIRECT    , 4}, // 0xd2 - sbcb
    {ADDR_DIRECT    , 6}, // 0xd3 - addd
    {ADDR_DIRECT    , 4}, // 0xd4 - andb
    {ADDR_DIRECT    , 4}, // 0xd5 - bitb
    {ADDR_DIRECT    , 4}, // 0xd6 - ldb
    {ADDR_DIRECT    , 4}, // 0xd7 - stb
    {ADDR_DIRECT    , 4}, // 0xd8 - eorb
    {ADDR_DIRECT    , 4}, // 0xd9 - adcb
    {ADDR_DIRECT    , 4}, // 0xda - orb
    {ADDR_DIRECT    , 4}, // 0xdb - addb
    {ADDR_DIRECT    , 5}, // 0xdc - ldd
    {ADDR_DIRECT    , 5}, // 0xdd - std
    {ADDR_DIRECT    , 5}, // 0xde - ldu
    {ADDR_DIRECT    , 5}, // 0xdf - stu

    {ADDR_INDEXED   , 4}, // 0xe0 - subb
    {ADDR_INDEXED   , 4}, // 0xe1 - cmpb
    {ADDR_INDEXED   , 4}, // 0xe2 - sbcb
    {ADDR_INDEXED   , 6}, // 0xe3 - addd
    {ADDR_INDEXED   , 4}, // 0xe4 - andb
    {ADDR_INDEXED   , 4}, // 0xe5 - bitb
    {ADDR_INDEXED   , 4}, // 0xe6 - ldb
    {ADDR_INDEXED   , 4}, // 0xe7 - stb
    {ADDR_INDEXED   , 4}, // 0xe8 - eorb
    {ADDR_INDEXED   , 4}, // 0xe9 - adcb
    {ADDR_INDEXED   , 4}, // 0xea - orb
    {ADDR_INDEXED   , 4}, // 0xeb - addb
    {ADDR_INDEXED   , 5}, // 0xec - ldd
    {ADDR_INDEXED   , 5}, // 0xed - std
    {ADDR_INDEXED   , 5}, // 0xee - ldu
    {ADDR_INDEXED   , 5}, // 0xef - stu

    {ADDR_EXTENDED  , 5}, // 0xf0 - subb
    {ADDR_EXTENDED  , 5}, // 0xf1 - cmpb
    {ADDR_EXTENDED  , 5}, // 0xf2 - sbcb
    {ADDR_EXTENDED  , 7}, // 0xf3 - addd
    {ADDR_EXTENDED  , 5}, // 0xf4 - andb
    {ADDR_EXTENDED  , 5}, // 0xf5 - bitb
    {ADDR_EXTENDED  , 5}, // 0xf6 - ldb
    {ADDR_EXTENDED  , 5}, // 0xf7 - stb
    {ADDR_EXTENDED  , 5}, // 0xf8 - eorb
    {ADDR_EXTENDED  , 5}, // 0xf9 - adcb
    {ADDR_EXTENDED  , 5}, // 0xfa - orb
    {ADDR_EXTENDED  , 5}, // 0xfb - addb
    {ADDR_EXTENDED  , 6}, // 0xfc - ldd
    {ADDR_EXTENDED  , 6}, // 0xfd - std
    {ADDR_EXTENDED  , 6}, // 0xfe - ldu
    {ADDR_EXTENDED  , 6}  // 0xff - stu
};

/* Double byte 0x10 op-codes */
machine_code_t machine_code_10[] __attribute__((section(".dtcm"))) = 
{
    {ILLEGAL_OP     , 0}, // 0x00
    {ILLEGAL_OP     , 0}, // 0x01
    {ILLEGAL_OP     , 0}, // 0x02
    {ILLEGAL_OP     , 0}, // 0x03
    {ILLEGAL_OP     , 0}, // 0x04
    {ILLEGAL_OP     , 0}, // 0x05
    {ILLEGAL_OP     , 0}, // 0x06
    {ILLEGAL_OP     , 0}, // 0x07
    {ILLEGAL_OP     , 0}, // 0x08
    {ILLEGAL_OP     , 0}, // 0x09
    {ILLEGAL_OP     , 0}, // 0x0a
    {ILLEGAL_OP     , 0}, // 0x0b
    {ILLEGAL_OP     , 0}, // 0x0c
    {ILLEGAL_OP     , 0}, // 0x0d
    {ILLEGAL_OP     , 0}, // 0x0e
    {ILLEGAL_OP     , 0}, // 0x0f

    {ILLEGAL_OP     , 0}, // 0x10
    {ILLEGAL_OP     , 0}, // 0x11
    {ILLEGAL_OP     , 0}, // 0x12
    {ILLEGAL_OP     , 0}, // 0x13
    {ILLEGAL_OP     , 0}, // 0x14
    {ILLEGAL_OP     , 0}, // 0x15
    {ILLEGAL_OP     , 0}, // 0x16
    {ILLEGAL_OP     , 0}, // 0x17
    {ILLEGAL_OP     , 0}, // 0x18
    {ILLEGAL_OP     , 0}, // 0x19
    {ILLEGAL_OP     , 0}, // 0x1a
    {ILLEGAL_OP     , 0}, // 0x1b
    {ILLEGAL_OP     , 0}, // 0x1c
    {ILLEGAL_OP     , 0}, // 0x1d
    {ILLEGAL_OP     , 0}, // 0x1e
    {ILLEGAL_OP     , 0}, // 0x1f
    
    {ILLEGAL_OP     , 0}, // 0x20
    {ADDR_LRELATIVE , 5}, // 0x21 - LBRN
    {ADDR_LRELATIVE , 5}, // 0x22 - LBHI
    {ADDR_LRELATIVE , 5}, // 0x23 - LBLS
    {ADDR_LRELATIVE , 5}, // 0x24 - LBCC
    {ADDR_LRELATIVE , 5}, // 0x25 - LBCS
    {ADDR_LRELATIVE , 5}, // 0x26 - LBNE
    {ADDR_LRELATIVE , 5}, // 0x27 - LBEQ
    {ADDR_LRELATIVE , 5}, // 0x28 - LBVC
    {ADDR_LRELATIVE , 5}, // 0x29 - LBVS
    {ADDR_LRELATIVE , 5}, // 0x2a - LBPL
    {ADDR_LRELATIVE , 5}, // 0x2b - LBMI
    {ADDR_LRELATIVE , 5}, // 0x2c - LBGE
    {ADDR_LRELATIVE , 5}, // 0x2d - LBLT
    {ADDR_LRELATIVE , 5}, // 0x2e - LBGT
    {ADDR_LRELATIVE , 5}, // 0x2f - LBLE
    
    {ILLEGAL_OP     , 0}, // 0x30
    {ILLEGAL_OP     , 0}, // 0x31
    {ILLEGAL_OP     , 0}, // 0x32
    {ILLEGAL_OP     , 0}, // 0x33
    {ILLEGAL_OP     , 0}, // 0x34
    {ILLEGAL_OP     , 0}, // 0x35
    {ILLEGAL_OP     , 0}, // 0x36
    {ILLEGAL_OP     , 0}, // 0x37
    {ILLEGAL_OP     , 0}, // 0x38
    {ILLEGAL_OP     , 0}, // 0x39
    {ILLEGAL_OP     , 0}, // 0x3a
    {ILLEGAL_OP     , 0}, // 0x3b
    {ILLEGAL_OP     , 0}, // 0x3c
    {ILLEGAL_OP     , 0}, // 0x3d
    {ILLEGAL_OP     , 0}, // 0x3e
    {ADDR_INHERENT  ,20}, // 0x3f - SWI2
    
    {ILLEGAL_OP     , 0}, // 0x40
    {ILLEGAL_OP     , 0}, // 0x41
    {ILLEGAL_OP     , 0}, // 0x42
    {ILLEGAL_OP     , 0}, // 0x43
    {ILLEGAL_OP     , 0}, // 0x44
    {ILLEGAL_OP     , 0}, // 0x45
    {ILLEGAL_OP     , 0}, // 0x46
    {ILLEGAL_OP     , 0}, // 0x47
    {ILLEGAL_OP     , 0}, // 0x48
    {ILLEGAL_OP     , 0}, // 0x49
    {ILLEGAL_OP     , 0}, // 0x4a
    {ILLEGAL_OP     , 0}, // 0x4b
    {ILLEGAL_OP     , 0}, // 0x4c
    {ILLEGAL_OP     , 0}, // 0x4d
    {ILLEGAL_OP     , 0}, // 0x4e
    {ILLEGAL_OP     , 0}, // 0x4f
    
    {ILLEGAL_OP     , 0}, // 0x50
    {ILLEGAL_OP     , 0}, // 0x51
    {ILLEGAL_OP     , 0}, // 0x52
    {ILLEGAL_OP     , 0}, // 0x53
    {ILLEGAL_OP     , 0}, // 0x54
    {ILLEGAL_OP     , 0}, // 0x55
    {ILLEGAL_OP     , 0}, // 0x56
    {ILLEGAL_OP     , 0}, // 0x57
    {ILLEGAL_OP     , 0}, // 0x58
    {ILLEGAL_OP     , 0}, // 0x59
    {ILLEGAL_OP     , 0}, // 0x5a
    {ILLEGAL_OP     , 0}, // 0x5b
    {ILLEGAL_OP     , 0}, // 0x5c
    {ILLEGAL_OP     , 0}, // 0x5d
    {ILLEGAL_OP     , 0}, // 0x5e
    {ILLEGAL_OP     , 0}, // 0x5f
    
    {ILLEGAL_OP     , 0}, // 0x60
    {ILLEGAL_OP     , 0}, // 0x61
    {ILLEGAL_OP     , 0}, // 0x62
    {ILLEGAL_OP     , 0}, // 0x63
    {ILLEGAL_OP     , 0}, // 0x64
    {ILLEGAL_OP     , 0}, // 0x65
    {ILLEGAL_OP     , 0}, // 0x66
    {ILLEGAL_OP     , 0}, // 0x67
    {ILLEGAL_OP     , 0}, // 0x68
    {ILLEGAL_OP     , 0}, // 0x69
    {ILLEGAL_OP     , 0}, // 0x6a
    {ILLEGAL_OP     , 0}, // 0x6b
    {ILLEGAL_OP     , 0}, // 0x6c
    {ILLEGAL_OP     , 0}, // 0x6d
    {ILLEGAL_OP     , 0}, // 0x6e
    {ILLEGAL_OP     , 0}, // 0x6f

    {ILLEGAL_OP     , 0}, // 0x70
    {ILLEGAL_OP     , 0}, // 0x71
    {ILLEGAL_OP     , 0}, // 0x72
    {ILLEGAL_OP     , 0}, // 0x73
    {ILLEGAL_OP     , 0}, // 0x74
    {ILLEGAL_OP     , 0}, // 0x75
    {ILLEGAL_OP     , 0}, // 0x76
    {ILLEGAL_OP     , 0}, // 0x77
    {ILLEGAL_OP     , 0}, // 0x78
    {ILLEGAL_OP     , 0}, // 0x79
    {ILLEGAL_OP     , 0}, // 0x7a
    {ILLEGAL_OP     , 0}, // 0x7b
    {ILLEGAL_OP     , 0}, // 0x7c
    {ILLEGAL_OP     , 0}, // 0x7d
    {ILLEGAL_OP     , 0}, // 0x7e
    {ILLEGAL_OP     , 0}, // 0x7f

    {ILLEGAL_OP     , 0}, // 0x80
    {ILLEGAL_OP     , 0}, // 0x81
    {ILLEGAL_OP     , 0}, // 0x82
    {ADDR_LIMMEDIATE, 5}, // 0x83 - CMPD
    {ILLEGAL_OP     , 0}, // 0x84
    {ILLEGAL_OP     , 0}, // 0x85
    {ILLEGAL_OP     , 0}, // 0x86
    {ILLEGAL_OP     , 0}, // 0x87
    {ILLEGAL_OP     , 0}, // 0x88
    {ILLEGAL_OP     , 0}, // 0x89
    {ILLEGAL_OP     , 0}, // 0x8a
    {ILLEGAL_OP     , 0}, // 0x8b
    {ADDR_LIMMEDIATE, 5}, // 0x8c - CMPY
    {ILLEGAL_OP     , 0}, // 0x8d
    {ADDR_LIMMEDIATE, 4}, // 0x8e - LDY
    {ILLEGAL_OP     , 0}, // 0x8f
    
    {ILLEGAL_OP     , 0}, // 0x90
    {ILLEGAL_OP     , 0}, // 0x91
    {ILLEGAL_OP     , 0}, // 0x92
    {ADDR_DIRECT    , 7}, // 0x93 - CMPD
    {ILLEGAL_OP     , 0}, // 0x94
    {ILLEGAL_OP     , 0}, // 0x95
    {ILLEGAL_OP     , 0}, // 0x96
    {ILLEGAL_OP     , 0}, // 0x97
    {ILLEGAL_OP     , 0}, // 0x98
    {ILLEGAL_OP     , 0}, // 0x99
    {ILLEGAL_OP     , 0}, // 0x9a
    {ILLEGAL_OP     , 0}, // 0x9b
    {ADDR_DIRECT    , 7}, // 0x9c - CMPY
    {ILLEGAL_OP     , 0}, // 0x9d
    {ADDR_DIRECT    , 6}, // 0x9e - LDY
    {ADDR_DIRECT    , 6}, // 0x9f - STY
    
    {ILLEGAL_OP     , 0}, // 0xa0
    {ILLEGAL_OP     , 0}, // 0xa1
    {ILLEGAL_OP     , 0}, // 0xa2
    {ADDR_INDEXED   , 7}, // 0xa3 - CMPD
    {ILLEGAL_OP     , 0}, // 0xa4
    {ILLEGAL_OP     , 0}, // 0xa5
    {ILLEGAL_OP     , 0}, // 0xa6
    {ILLEGAL_OP     , 0}, // 0xa7
    {ILLEGAL_OP     , 0}, // 0xa8
    {ILLEGAL_OP     , 0}, // 0xa9
    {ILLEGAL_OP     , 0}, // 0xaa
    {ILLEGAL_OP     , 0}, // 0xab
    {ADDR_INDEXED   , 7}, // 0xac - CMPY
    {ILLEGAL_OP     , 0}, // 0xad
    {ADDR_INDEXED   , 6}, // 0xae - LDY
    {ADDR_INDEXED   , 6}, // 0xaf - STY

    {ILLEGAL_OP     , 0}, // 0xb0
    {ILLEGAL_OP     , 0}, // 0xb1
    {ILLEGAL_OP     , 0}, // 0xb2
    {ADDR_EXTENDED  , 8}, // 0xb3 - CMPD
    {ILLEGAL_OP     , 0}, // 0xb4
    {ILLEGAL_OP     , 0}, // 0xb5
    {ILLEGAL_OP     , 0}, // 0xb6
    {ILLEGAL_OP     , 0}, // 0xb7
    {ILLEGAL_OP     , 0}, // 0xb8
    {ILLEGAL_OP     , 0}, // 0xb9
    {ILLEGAL_OP     , 0}, // 0xba
    {ILLEGAL_OP     , 0}, // 0xbb
    {ADDR_EXTENDED  , 8}, // 0xbc - CMPY
    {ILLEGAL_OP     , 0}, // 0xbd
    {ADDR_EXTENDED  , 7}, // 0xbe - LDY
    {ADDR_EXTENDED  , 7}, // 0xbf - STY

    {ILLEGAL_OP     , 0}, // 0xc0
    {ILLEGAL_OP     , 0}, // 0xc1
    {ILLEGAL_OP     , 0}, // 0xc2
    {ILLEGAL_OP     , 0}, // 0xc3
    {ILLEGAL_OP     , 0}, // 0xc4
    {ILLEGAL_OP     , 0}, // 0xc5
    {ILLEGAL_OP     , 0}, // 0xc6
    {ILLEGAL_OP     , 0}, // 0xc7
    {ILLEGAL_OP     , 0}, // 0xc8
    {ILLEGAL_OP     , 0}, // 0xc9
    {ILLEGAL_OP     , 0}, // 0xca
    {ILLEGAL_OP     , 0}, // 0xcb
    {ILLEGAL_OP     , 0}, // 0xcc
    {ILLEGAL_OP     , 0}, // 0xcd
    {ADDR_LIMMEDIATE, 4}, // 0xce - LDS
    {ILLEGAL_OP     , 0}, // 0xcf
    
    {ILLEGAL_OP     , 0}, // 0xd0
    {ILLEGAL_OP     , 0}, // 0xd1
    {ILLEGAL_OP     , 0}, // 0xd2
    {ILLEGAL_OP     , 0}, // 0xd3
    {ILLEGAL_OP     , 0}, // 0xd4
    {ILLEGAL_OP     , 0}, // 0xd5
    {ILLEGAL_OP     , 0}, // 0xd6
    {ILLEGAL_OP     , 0}, // 0xd7
    {ILLEGAL_OP     , 0}, // 0xd8
    {ILLEGAL_OP     , 0}, // 0xd9
    {ILLEGAL_OP     , 0}, // 0xda
    {ILLEGAL_OP     , 0}, // 0xdb
    {ILLEGAL_OP     , 0}, // 0xdc
    {ILLEGAL_OP     , 0}, // 0xdd
    {ADDR_DIRECT    , 6}, // 0xde - LDS
    {ADDR_DIRECT    , 6}, // 0xdf - STS
    
    {ILLEGAL_OP     , 0}, // 0xe0
    {ILLEGAL_OP     , 0}, // 0xe1
    {ILLEGAL_OP     , 0}, // 0xe2
    {ILLEGAL_OP     , 0}, // 0xe3
    {ILLEGAL_OP     , 0}, // 0xe4
    {ILLEGAL_OP     , 0}, // 0xe5
    {ILLEGAL_OP     , 0}, // 0xe6
    {ILLEGAL_OP     , 0}, // 0xe7
    {ILLEGAL_OP     , 0}, // 0xe8
    {ILLEGAL_OP     , 0}, // 0xe9
    {ILLEGAL_OP     , 0}, // 0xea
    {ILLEGAL_OP     , 0}, // 0xeb
    {ILLEGAL_OP     , 0}, // 0xec
    {ILLEGAL_OP     , 0}, // 0xed
    {ADDR_INDEXED   , 6}, // 0xee - LDS
    {ADDR_INDEXED   , 6}, // 0xef - STS
    
    {ILLEGAL_OP     , 0}, // 0xf0
    {ILLEGAL_OP     , 0}, // 0xf1
    {ILLEGAL_OP     , 0}, // 0xf2
    {ILLEGAL_OP     , 0}, // 0xf3
    {ILLEGAL_OP     , 0}, // 0xf4
    {ILLEGAL_OP     , 0}, // 0xf5
    {ILLEGAL_OP     , 0}, // 0xf6
    {ILLEGAL_OP     , 0}, // 0xf7
    {ILLEGAL_OP     , 0}, // 0xf8
    {ILLEGAL_OP     , 0}, // 0xf9
    {ILLEGAL_OP     , 0}, // 0xfa
    {ILLEGAL_OP     , 0}, // 0xfb
    {ILLEGAL_OP     , 0}, // 0xfc
    {ILLEGAL_OP     , 0}, // 0xfd
    {ADDR_EXTENDED  , 7}, // 0xfe - LDS
    {ADDR_EXTENDED  , 7}  // 0xff - STS
};

/* Double byte 0x11 op-codes */
machine_code_t machine_code_11[] __attribute__((section(".dtcm"))) = 
{
    {ILLEGAL_OP     , 0}, // 0x00
    {ILLEGAL_OP     , 0}, // 0x01
    {ILLEGAL_OP     , 0}, // 0x02
    {ILLEGAL_OP     , 0}, // 0x03
    {ILLEGAL_OP     , 0}, // 0x04
    {ILLEGAL_OP     , 0}, // 0x05
    {ILLEGAL_OP     , 0}, // 0x06
    {ILLEGAL_OP     , 0}, // 0x07
    {ILLEGAL_OP     , 0}, // 0x08
    {ILLEGAL_OP     , 0}, // 0x09
    {ILLEGAL_OP     , 0}, // 0x0a
    {ILLEGAL_OP     , 0}, // 0x0b
    {ILLEGAL_OP     , 0}, // 0x0c
    {ILLEGAL_OP     , 0}, // 0x0d
    {ILLEGAL_OP     , 0}, // 0x0e
    {ILLEGAL_OP     , 0}, // 0x0f

    {ILLEGAL_OP     , 0}, // 0x10
    {ILLEGAL_OP     , 0}, // 0x11
    {ILLEGAL_OP     , 0}, // 0x12
    {ILLEGAL_OP     , 0}, // 0x13
    {ILLEGAL_OP     , 0}, // 0x14
    {ILLEGAL_OP     , 0}, // 0x15
    {ILLEGAL_OP     , 0}, // 0x16
    {ILLEGAL_OP     , 0}, // 0x17
    {ILLEGAL_OP     , 0}, // 0x18
    {ILLEGAL_OP     , 0}, // 0x19
    {ILLEGAL_OP     , 0}, // 0x1a
    {ILLEGAL_OP     , 0}, // 0x1b
    {ILLEGAL_OP     , 0}, // 0x1c
    {ILLEGAL_OP     , 0}, // 0x1d
    {ILLEGAL_OP     , 0}, // 0x1e
    {ILLEGAL_OP     , 0}, // 0x1f
    
    {ILLEGAL_OP     , 0}, // 0x20
    {ILLEGAL_OP     , 0}, // 0x21
    {ILLEGAL_OP     , 0}, // 0x22
    {ILLEGAL_OP     , 0}, // 0x23
    {ILLEGAL_OP     , 0}, // 0x24
    {ILLEGAL_OP     , 0}, // 0x25
    {ILLEGAL_OP     , 0}, // 0x26
    {ILLEGAL_OP     , 0}, // 0x27
    {ILLEGAL_OP     , 0}, // 0x28
    {ILLEGAL_OP     , 0}, // 0x29
    {ILLEGAL_OP     , 0}, // 0x2a
    {ILLEGAL_OP     , 0}, // 0x2b
    {ILLEGAL_OP     , 0}, // 0x2c
    {ILLEGAL_OP     , 0}, // 0x2d
    {ILLEGAL_OP     , 0}, // 0x2e
    {ILLEGAL_OP     , 0}, // 0x2f

    {ILLEGAL_OP     , 0}, // 0x30
    {ILLEGAL_OP     , 0}, // 0x31
    {ILLEGAL_OP     , 0}, // 0x32
    {ILLEGAL_OP     , 0}, // 0x33
    {ILLEGAL_OP     , 0}, // 0x34
    {ILLEGAL_OP     , 0}, // 0x35
    {ILLEGAL_OP     , 0}, // 0x36
    {ILLEGAL_OP     , 0}, // 0x37
    {ILLEGAL_OP     , 0}, // 0x38
    {ILLEGAL_OP     , 0}, // 0x39
    {ILLEGAL_OP     , 0}, // 0x3a
    {ILLEGAL_OP     , 0}, // 0x3b
    {ILLEGAL_OP     , 0}, // 0x3c
    {ILLEGAL_OP     ,12}, // 0x3d - illegal... but MUL
    {ILLEGAL_OP     , 0}, // 0x3e
    {ADDR_INHERENT  ,20}, // 0x3f - SWI3

    {ILLEGAL_OP     , 0}, // 0x40
    {ILLEGAL_OP     , 0}, // 0x41
    {ILLEGAL_OP     , 0}, // 0x42
    {ILLEGAL_OP     , 0}, // 0x43
    {ILLEGAL_OP     , 0}, // 0x44
    {ILLEGAL_OP     , 0}, // 0x45
    {ILLEGAL_OP     , 0}, // 0x46
    {ILLEGAL_OP     , 0}, // 0x47
    {ILLEGAL_OP     , 0}, // 0x48
    {ILLEGAL_OP     , 0}, // 0x49
    {ILLEGAL_OP     , 0}, // 0x4a
    {ILLEGAL_OP     , 0}, // 0x4b
    {ILLEGAL_OP     , 0}, // 0x4c
    {ILLEGAL_OP     , 0}, // 0x4d
    {ILLEGAL_OP     , 0}, // 0x4e
    {ILLEGAL_OP     , 0}, // 0x4f

    {ILLEGAL_OP     , 0}, // 0x50
    {ILLEGAL_OP     , 0}, // 0x51
    {ILLEGAL_OP     , 0}, // 0x52
    {ILLEGAL_OP     , 0}, // 0x53
    {ILLEGAL_OP     , 0}, // 0x54
    {ILLEGAL_OP     , 0}, // 0x55
    {ILLEGAL_OP     , 0}, // 0x56
    {ILLEGAL_OP     , 0}, // 0x57
    {ILLEGAL_OP     , 0}, // 0x58
    {ILLEGAL_OP     , 0}, // 0x59
    {ILLEGAL_OP     , 0}, // 0x5a
    {ILLEGAL_OP     , 0}, // 0x5b
    {ILLEGAL_OP     , 0}, // 0x5c
    {ILLEGAL_OP     , 0}, // 0x5d
    {ILLEGAL_OP     , 0}, // 0x5e
    {ILLEGAL_OP     , 0}, // 0x5f

    {ILLEGAL_OP     , 0}, // 0x60
    {ILLEGAL_OP     , 0}, // 0x61
    {ILLEGAL_OP     , 0}, // 0x62
    {ILLEGAL_OP     , 0}, // 0x63
    {ILLEGAL_OP     , 0}, // 0x64
    {ILLEGAL_OP     , 0}, // 0x65
    {ILLEGAL_OP     , 0}, // 0x66
    {ILLEGAL_OP     , 0}, // 0x67
    {ILLEGAL_OP     , 0}, // 0x68
    {ILLEGAL_OP     , 0}, // 0x69
    {ILLEGAL_OP     , 0}, // 0x6a
    {ILLEGAL_OP     , 0}, // 0x6b
    {ILLEGAL_OP     , 0}, // 0x6c
    {ILLEGAL_OP     , 0}, // 0x6d
    {ILLEGAL_OP     , 0}, // 0x6e
    {ILLEGAL_OP     , 0}, // 0x6f

    {ILLEGAL_OP     , 0}, // 0x70
    {ILLEGAL_OP     , 0}, // 0x71
    {ILLEGAL_OP     , 0}, // 0x72
    {ILLEGAL_OP     , 0}, // 0x73
    {ILLEGAL_OP     , 0}, // 0x74
    {ILLEGAL_OP     , 0}, // 0x75
    {ILLEGAL_OP     , 0}, // 0x76
    {ILLEGAL_OP     , 0}, // 0x77
    {ILLEGAL_OP     , 0}, // 0x78
    {ILLEGAL_OP     , 0}, // 0x79
    {ILLEGAL_OP     , 0}, // 0x7a
    {ILLEGAL_OP     , 0}, // 0x7b
    {ILLEGAL_OP     , 0}, // 0x7c
    {ILLEGAL_OP     , 0}, // 0x7d
    {ILLEGAL_OP     , 0}, // 0x7e
    {ILLEGAL_OP     , 0}, // 0x7f

    {ILLEGAL_OP     , 0}, // 0x80
    {ILLEGAL_OP     , 0}, // 0x81
    {ILLEGAL_OP     , 0}, // 0x82
    {ADDR_LIMMEDIATE, 5}, // 0x83 - CMPU
    {ILLEGAL_OP     , 0}, // 0x84
    {ILLEGAL_OP     , 0}, // 0x85
    {ILLEGAL_OP     , 0}, // 0x86
    {ILLEGAL_OP     , 0}, // 0x87
    {ILLEGAL_OP     , 0}, // 0x88
    {ILLEGAL_OP     , 0}, // 0x89
    {ILLEGAL_OP     , 0}, // 0x8a
    {ILLEGAL_OP     , 0}, // 0x8b
    {ADDR_LIMMEDIATE, 5}, // 0x8c - CMPS
    {ILLEGAL_OP     , 0}, // 0x8d
    {ILLEGAL_OP     , 0}, // 0x8e
    {ILLEGAL_OP     , 0}, // 0x8f

    {ILLEGAL_OP     , 0}, // 0x90
    {ILLEGAL_OP     , 0}, // 0x91
    {ILLEGAL_OP     , 0}, // 0x92
    {ADDR_DIRECT    , 7}, // 0x93 - CMPU
    {ILLEGAL_OP     , 0}, // 0x94
    {ILLEGAL_OP     , 0}, // 0x95
    {ILLEGAL_OP     , 0}, // 0x96
    {ILLEGAL_OP     , 0}, // 0x97
    {ILLEGAL_OP     , 0}, // 0x98
    {ILLEGAL_OP     , 0}, // 0x99
    {ILLEGAL_OP     , 0}, // 0x9a
    {ILLEGAL_OP     , 0}, // 0x9b
    {ADDR_DIRECT    , 7}, // 0x9c - CMPS
    {ILLEGAL_OP     , 0}, // 0x9d
    {ILLEGAL_OP     , 0}, // 0x9e
    {ILLEGAL_OP     , 0}, // 0x9f

    {ILLEGAL_OP     , 0}, // 0xa0
    {ILLEGAL_OP     , 0}, // 0xa1
    {ILLEGAL_OP     , 0}, // 0xa2
    {ADDR_INDEXED   , 7}, // 0xa3 - CMPU
    {ILLEGAL_OP     , 0}, // 0xa4
    {ILLEGAL_OP     , 0}, // 0xa5
    {ILLEGAL_OP     , 0}, // 0xa6
    {ILLEGAL_OP     , 0}, // 0xa7
    {ILLEGAL_OP     , 0}, // 0xa8
    {ILLEGAL_OP     , 0}, // 0xa9
    {ILLEGAL_OP     , 0}, // 0xaa
    {ILLEGAL_OP     , 0}, // 0xab
    {ADDR_INDEXED   , 7}, // 0xac - CMPS
    {ILLEGAL_OP     , 0}, // 0xad
    {ILLEGAL_OP     , 0}, // 0xae
    {ILLEGAL_OP     , 0}, // 0xaf

    {ILLEGAL_OP     , 0}, // 0xb0
    {ILLEGAL_OP     , 0}, // 0xb1
    {ILLEGAL_OP     , 0}, // 0xb2
    {ADDR_EXTENDED  , 8}, // 0xb3 - CMPU
    {ILLEGAL_OP     , 0}, // 0xb4
    {ILLEGAL_OP     , 0}, // 0xb5
    {ILLEGAL_OP     , 0}, // 0xb6
    {ILLEGAL_OP     , 0}, // 0xb7
    {ILLEGAL_OP     , 0}, // 0xb8
    {ILLEGAL_OP     , 0}, // 0xb9
    {ILLEGAL_OP     , 0}, // 0xba
    {ILLEGAL_OP     , 0}, // 0xbb
    {ADDR_EXTENDED  , 8}, // 0xbc - CMPS
    {ILLEGAL_OP     , 0}, // 0xbd
    {ILLEGAL_OP     , 0}, // 0xbe
    {ILLEGAL_OP     , 0}, // 0xbf

    {ILLEGAL_OP     , 0}, // 0xc0
    {ILLEGAL_OP     , 0}, // 0xc1
    {ILLEGAL_OP     , 0}, // 0xc2
    {ILLEGAL_OP     , 0}, // 0xc3
    {ILLEGAL_OP     , 0}, // 0xc4
    {ILLEGAL_OP     , 0}, // 0xc5
    {ILLEGAL_OP     , 0}, // 0xc6
    {ILLEGAL_OP     , 0}, // 0xc7
    {ILLEGAL_OP     , 0}, // 0xc8
    {ILLEGAL_OP     , 0}, // 0xc9
    {ILLEGAL_OP     , 0}, // 0xca
    {ILLEGAL_OP     , 0}, // 0xcb
    {ILLEGAL_OP     , 0}, // 0xcc
    {ILLEGAL_OP     , 0}, // 0xcd
    {ILLEGAL_OP     , 0}, // 0xce
    {ILLEGAL_OP     , 0}, // 0xcf

    {ILLEGAL_OP     , 0}, // 0xd0
    {ILLEGAL_OP     , 0}, // 0xd1
    {ILLEGAL_OP     , 0}, // 0xd2
    {ILLEGAL_OP     , 0}, // 0xd3
    {ILLEGAL_OP     , 0}, // 0xd4
    {ILLEGAL_OP     , 0}, // 0xd5
    {ILLEGAL_OP     , 0}, // 0xd6
    {ILLEGAL_OP     , 0}, // 0xd7
    {ILLEGAL_OP     , 0}, // 0xd8
    {ILLEGAL_OP     , 0}, // 0xd9
    {ILLEGAL_OP     , 0}, // 0xda
    {ILLEGAL_OP     , 0}, // 0xdb
    {ILLEGAL_OP     , 0}, // 0xdc
    {ILLEGAL_OP     , 0}, // 0xdd
    {ILLEGAL_OP     , 0}, // 0xde
    {ILLEGAL_OP     , 0}, // 0xdf

    {ILLEGAL_OP     , 0}, // 0xe0
    {ILLEGAL_OP     , 0}, // 0xe1
    {ILLEGAL_OP     , 0}, // 0xe2
    {ILLEGAL_OP     , 0}, // 0xe3
    {ILLEGAL_OP     , 0}, // 0xe4
    {ILLEGAL_OP     , 0}, // 0xe5
    {ILLEGAL_OP     , 0}, // 0xe6
    {ILLEGAL_OP     , 0}, // 0xe7
    {ILLEGAL_OP     , 0}, // 0xe8
    {ILLEGAL_OP     , 0}, // 0xe9
    {ILLEGAL_OP     , 0}, // 0xea
    {ILLEGAL_OP     , 0}, // 0xeb
    {ILLEGAL_OP     , 0}, // 0xec
    {ILLEGAL_OP     , 0}, // 0xed
    {ILLEGAL_OP     , 0}, // 0xee
    {ILLEGAL_OP     , 0}, // 0xef

    {ILLEGAL_OP     , 0}, // 0xf0
    {ILLEGAL_OP     , 0}, // 0xf1
    {ILLEGAL_OP     , 0}, // 0xf2
    {ILLEGAL_OP     , 0}, // 0xf3
    {ILLEGAL_OP     , 0}, // 0xf4
    {ILLEGAL_OP     , 0}, // 0xf5
    {ILLEGAL_OP     , 0}, // 0xf6
    {ILLEGAL_OP     , 0}, // 0xf7
    {ILLEGAL_OP     , 0}, // 0xf8
    {ILLEGAL_OP     , 0}, // 0xf9
    {ILLEGAL_OP     , 0}, // 0xfa
    {ILLEGAL_OP     , 0}, // 0xfb
    {ILLEGAL_OP     , 0}, // 0xfc
    {ILLEGAL_OP     , 0}, // 0xfd
    {ILLEGAL_OP     , 0}, // 0xfe
    {ILLEGAL_OP     , 0}  // 0xff
};

#endif  /* __MC6809E_H__ */
