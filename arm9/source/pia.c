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
 * pia.c
 *
 *  Module that implements the MC6821 PIA functionality.
 *
 *  February 6, 2021
 *
 *******************************************************************/
#include    <nds.h>
#include    <stdint.h>
#include    <string.h>

#include    "cpu.h"
#include    "mem.h"
#include    "vdg.h"
#include    "pia.h"
#include    "DracoUtils.h"

/* -----------------------------------------
   Local definitions
----------------------------------------- */
#define     PIA0_PA             0xff00
#define     PIA0_CRA            0xff01
#define     PIA0_PB             0xff02
#define     PIA0_CRB            0xff03

#define     PIA1_PA             0xff20
#define     PIA1_CRA            0xff21
#define     PIA1_PB             0xff22
#define     PIA1_CRB            0xff23

#define     PIA_CR_INTR         0x01    // CA1/CB1 interrupt enable bit
#define     PIA_CR_IRQ_STAT     0x80    // IRQA1/IRQB1 status bit

#define     PIA_DDR             0x04    // 1=Normal, 0=DDR

#define     MUX_RIGHT_X         0x00
#define     MUX_RIGHT_Y         0x01
#define     MUX_LEFT_X          0x02
#define     MUX_LEFT_Y          0x03

#define     MOTOR_ON            0b00001000
#define     CA2_SET_CLR         0b00110000
#define     BIT_THRESHOLD_HI    4
#define     BIT_THRESHOLD_LO    20

uint32_t tape_pos   __attribute__((section(".dtcm"))) = 0;
uint16_t tape_motor __attribute__((section(".dtcm"))) = 0;

uint8_t  pia0_ddr_a __attribute__((section(".dtcm"))) = PIA_DDR;
uint8_t  pia0_ddr_b __attribute__((section(".dtcm"))) = PIA_DDR;
uint8_t  pia1_ddr_a __attribute__((section(".dtcm"))) = PIA_DDR;
uint8_t  pia1_ddr_b __attribute__((section(".dtcm"))) = PIA_DDR;

/* -----------------------------------------
   Module static functions
----------------------------------------- */
static uint8_t io_handler_pia0_pa(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_handler_pia0_pb(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_handler_pia0_cra(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_handler_pia0_crb(uint16_t address, uint8_t data, mem_operation_t op);

static uint8_t io_handler_pia1_pa(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_handler_pia1_pb(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_handler_pia1_cra(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t io_handler_pia1_crb(uint16_t address, uint8_t data, mem_operation_t op);

ITCM_CODE uint8_t get_keyboard_row_scan(uint8_t data);

/* -----------------------------------------
   Module globals
----------------------------------------- */
uint8_t   pia0_ca1_int_enabled __attribute__((section(".dtcm"))) = 0;    // HSYNC FIRQ
uint8_t   pia0_cb1_int_enabled __attribute__((section(".dtcm"))) = 0;    // VSYNC IRQ
uint8_t   pia1_cb1_int_enabled __attribute__((section(".dtcm"))) = 0;    // CART  FIRQ

uint8_t   mux_select           __attribute__((section(".dtcm"))) = 0x00;
uint16_t  dac_output           __attribute__((section(".dtcm"))) = 0;
uint8_t   sound_enable         __attribute__((section(".dtcm"))) = 1;
uint8_t   last_comparator      __attribute__((section(".dtcm"))) = 0;
uint8_t   cas_eof              __attribute__((section(".dtcm"))) = 0;

uint8_t tape_byte              __attribute__((section(".dtcm"))) = 0;
int     bit_index              __attribute__((section(".dtcm"))) = 0;
int     bit_timing_threshold   __attribute__((section(".dtcm"))) = 0;
int     bit_timing_count       __attribute__((section(".dtcm"))) = 0;


/*
    Dragon keyboard map

          LSB              $FF02                    MSB
        | PB0   PB1   PB2   PB3   PB4   PB5   PB6   PB7 | <- column
    ----|-----------------------------------------------|-----------
    PA0 |   0     1     2     3     4     5     6     7 |   LSB
    PA1 |   8     9     :     ;     ,     -     .     / |
    PA2 |   @     A     B     C     D     E     F     G |
    PA3 |   H     I     J     K     L     M     N     O | $FF00
    PA4 |   P     Q     R     S     T     U     V     W |
    PA5 |   X     Y     Z    Up  Down  Left Right Space |
    PA6 | ENT   CLR   BRK   N/C   N/C   N/C   N/C  SHFT |
    PA7 | Comparator input                              |   MSB
*/
uint8_t kbd_scan_dragon[60][2] __attribute__((section(".dtcm"))) = {
        // Column     Row
        { 0xff,       255 }, // #0   Reserved for Joy Up
        { 0xff,       255 }, // #1   Reserved for Joy Down
        { 0xff,       255 }, // #2   Reserved for Joy Left
        { 0xff,       255 }, // #3   Reserved for Joy Right
        { 0xff,       255 }, // #4   Reserved for Joy Fire

        { 0b11111101,   2 }, // #5   A
        { 0b11111011,   2 }, //      B
        { 0b11110111,   2 }, //      C
        { 0b11101111,   2 }, //      D
        { 0b11011111,   2 }, //      E
        { 0b10111111,   2 }, // #10  F
        { 0b01111111,   2 }, //      G

        { 0b11111110,   3 }, //      H
        { 0b11111101,   3 }, //      I
        { 0b11111011,   3 }, //      J
        { 0b11110111,   3 }, // #15  K
        { 0b11101111,   3 }, //      L
        { 0b11011111,   3 }, //      M
        { 0b10111111,   3 }, //      N
        { 0b01111111,   3 }, //      O

        { 0b11111110,   4 }, // #20  P
        { 0b11111101,   4 }, //      Q
        { 0b11111011,   4 }, //      R
        { 0b11110111,   4 }, //      S
        { 0b11101111,   4 }, //      T
        { 0b11011111,   4 }, // #25  U
        { 0b10111111,   4 }, //      V
        { 0b01111111,   4 }, //      W

        { 0b11111110,   5 }, //      X
        { 0b11111101,   5 }, //      Y
        { 0b11111011,   5 }, // #30  Z

        { 0b11111101,   0 }, //      1
        { 0b11111011,   0 }, //      2
        { 0b11110111,   0 }, //      3
        { 0b11101111,   0 }, //      4
        { 0b11011111,   0 }, // #35  5
        { 0b10111111,   0 }, //      6
        { 0b01111111,   0 }, //      7
        { 0b11111110,   1 }, //      8
        { 0b11111101,   1 }, //      9
        { 0b11111110,   0 }, // #40  0

        { 0b11011111,   1 }, //      -
        { 0b11101111,   1 }, //      ,
        { 0b10111111,   1 }, //      .
        { 0b11111011,   1 }, //      :
        { 0b11110111,   1 }, // #45  ;
        { 0b01111111,   1 }, //      /
        { 0b11111110,   2 }, //      @

        { 0b11111110,   6 }, // #48  Enter
        { 0b01111111,   5 }, // #49  Space bar

        { 0b11110111,   5 }, // #50  Up arrow
        { 0b11011111,   5 }, // #51  Left arrow
        { 0b10111111,   5 }, // #52  Right arrow
        { 0b11101111,   5 }, // #53  Down arrow

        { 0b11111101,   6 }, // #54  CLEAR
        { 0b01111111,   6 }, // #55  Shift key
        { 0b11111011,   6 }, // #56  Break (ESC key)

        { 0b00000000, 255 }, // Reserved 1
        { 0b00000000, 255 }, // Reserved 2
        { 0b00000000, 255 }, // Reserved 3
};

/*
    Tandy CoCo keyboard map

          LSB              $FF02                    MSB
        | PB0   PB1   PB2   PB3   PB4   PB5   PB6   PB7 | <- column
    ----|-----------------------------------------------|-----------
    PA0 |   @     A     B     C     D     E     F     G |   LSB
    PA1 |   H     I     J     K     L     M     N     O |
    PA2 |   P     Q     R     S     T     U     V     W |
    PA3 |   X     Y     Z    Up  Down  Left Right Space | $FF00
    PA4 |   0     1     2     3     4     5     6     7 |
    PA5 |   8     9     :     ;     ,     -     .     / |
    PA6 | ENT   CLR   BRK   N/C   N/C   N/C   N/C  SHFT |
    PA7 | Comparator input                              |   MSB
*/
uint8_t kbd_scan_coco[60][2] __attribute__((section(".dtcm"))) = {
        // Column     Row
        { 0xff,       255 }, // #0   Reserved for Joy Up
        { 0xff,       255 }, // #1   Reserved for Joy Down
        { 0xff,       255 }, // #2   Reserved for Joy Left
        { 0xff,       255 }, // #3   Reserved for Joy Right
        { 0xff,       255 }, // #4   Reserved for Joy Fire

        { 0b11111101,   0 }, // #5   A
        { 0b11111011,   0 }, //      B
        { 0b11110111,   0 }, //      C
        { 0b11101111,   0 }, //      D
        { 0b11011111,   0 }, //      E
        { 0b10111111,   0 }, // #10  F
        { 0b01111111,   0 }, //      G

        { 0b11111110,   1 }, //      H
        { 0b11111101,   1 }, //      I
        { 0b11111011,   1 }, //      J
        { 0b11110111,   1 }, // #15  K
        { 0b11101111,   1 }, //      L
        { 0b11011111,   1 }, //      M
        { 0b10111111,   1 }, //      N
        { 0b01111111,   1 }, //      O

        { 0b11111110,   2 }, // #20  P
        { 0b11111101,   2 }, //      Q
        { 0b11111011,   2 }, //      R
        { 0b11110111,   2 }, //      S
        { 0b11101111,   2 }, //      T
        { 0b11011111,   2 }, //      U
        { 0b10111111,   2 }, //      V
        { 0b01111111,   2 }, //      W

        { 0b11111110,   3 }, //      X
        { 0b11111101,   3 }, //      Y
        { 0b11111011,   3 }, // #30  Z


        { 0b11111101,   4 }, // #31  1
        { 0b11111011,   4 }, //      2
        { 0b11110111,   4 }, //      3
        { 0b11101111,   4 }, //      4
        { 0b11011111,   4 }, //      5
        { 0b10111111,   4 }, //      6
        { 0b01111111,   4 }, //      7
        { 0b11111110,   5 }, //      8
        { 0b11111101,   5 }, //      9
        { 0b11111110,   4 }, // #40  0

        { 0b11011111,   5 }, //      -
        { 0b11101111,   5 }, //      ,
        { 0b10111111,   5 }, //      .
        { 0b11111011,   5 }, //      :
        { 0b11110111,   5 }, // #45  ;
        { 0b01111111,   5 }, //      /
        { 0b11111110,   0 }, //      @

        { 0b11111110,   6 }, //      Enter
        { 0b01111111,   3 }, //      Space bar

        { 0b11110111,   3 }, // #50  Up arrow
        { 0b11011111,   3 }, //      Left arrow
        { 0b10111111,   3 }, //      Right arrow
        { 0b11101111,   3 }, //      Down arrow

        { 0b11111101,   6 }, //      CLEAR
        { 0b01111111,   6 }, // #55  Shift key
        { 0b11111011,   6 }, //      Break (ESC key)

        { 0b00000000, 255 }, // Reserved 1
        { 0b00000000, 255 }, // Reserved 2
        { 0b00000000, 255 }, // Reserved 3
};


uint8_t keyboard_rows[KBD_ROWS] __attribute__((section(".dtcm"))) = {
        255,    // row PIA0_PA0
        255,    // row PIA0_PA1
        255,    // row PIA0_PA2
        255,    // row PIA0_PA3
        255,    // row PIA0_PA4
        255,    // row PIA0_PA5
        255,    // row PIA0_PA6
};

/*------------------------------------------------
 * pia_init()
 *
 *  Initialize the PIA device
 *
 *  param:  Nothing
 *  return: Nothing
 */
void pia_init(void)
{
    /* Link IO call-backs
     */
    memory_IO[PIA0_PA] = 0x7f;

    // Handle all mirrors of the PIA across the IO range of memory
    for (int mirror = 0; mirror < 32; mirror += 4)
    {
        mem_define_io(PIA0_PA  + mirror,  PIA0_PA  + mirror, io_handler_pia0_pa);    // Joystick comparator, keyboard row input
        mem_define_io(PIA0_PB  + mirror,  PIA0_PB  + mirror, io_handler_pia0_pb);    // Keyboard column output
        mem_define_io(PIA0_CRA + mirror,  PIA0_CRA + mirror, io_handler_pia0_cra);   // Audio multiplexer select bit.0
        mem_define_io(PIA0_CRB + mirror,  PIA0_CRB + mirror, io_handler_pia0_crb);   // Field sync interrupt

        mem_define_io(PIA1_PA  + mirror,  PIA1_PA  + mirror, io_handler_pia1_pa);    // 6-bit DAC output, cassette interface input bit
        mem_define_io(PIA1_PB  + mirror,  PIA1_PB  + mirror, io_handler_pia1_pb);    // VDG mode bits output
        mem_define_io(PIA1_CRA + mirror,  PIA1_CRA + mirror, io_handler_pia1_cra);   // Cassette tape motor control
        mem_define_io(PIA1_CRB + mirror,  PIA1_CRB + mirror, io_handler_pia1_crb);   // Audio multiplexer select bit.1
    }

    pia0_ca1_int_enabled = 0;    // HSYNC FIRQ
    pia0_cb1_int_enabled = 0;    // VSYNC IRQ
    pia1_cb1_int_enabled = 0;    // CART  FIRQ
    dac_output           = 0;    // No DAC output to start
    sound_enable         = 1;    // Sound enable/disable
    last_comparator      = 0;    // Last comparator value
    tape_pos             = 0;    // Current tape position
    tape_motor           = 0;    // Motor on (1) or off (0)
    mux_select           = 0x00; // The Comparator Mux
    cas_eof              = 0;    // End of Cassette File

    pia0_ddr_a = PIA_DDR;        // Normal Data Register Map
    pia0_ddr_b = PIA_DDR;        // Normal Data Register Map
    pia1_ddr_a = PIA_DDR;        // Normal Data Register Map
    pia1_ddr_b = PIA_DDR;        // Normal Data Register Map
}

/*------------------------------------------------
 * pia_vsync_irq()
 *
 *  Assert an external interrupt from the VDG Field Sync line (V-Sync)
 *  through PIA0-CB1 that generates an IRQ interrupt.
 *
 *  param:  Nothing
 *  return: Nothing
 */
void pia_vsync_irq(void)
{
    /* Set the VSYNC 'on' bit - turns off when port read
     */
    memory_IO[PIA0_CRB] |= PIA_CR_IRQ_STAT;

    /* Assert vsync interrupt if enabled
     */
    if ( pia0_cb1_int_enabled )
    {
        cpu_irq(1);
    }
}


/*------------------------------------------------
 * pia_hsync_firq()
 *
 *  Assert an external interrupt from the VDG fast Sync line (H-Sync)
 *  through PIA0-CA1 that generates a FIRQ interrupt.
 *
 *  param:  Nothing
 *  return: Nothing
 */
void pia_hsync_firq(void)
{
    /* Set the HSYNC 'on' bit - turns off on next port read
     */
    memory_IO[PIA0_CRA] |= PIA_CR_IRQ_STAT;

    /* Assert hsync interrupt if enabled
     */
    if ( pia0_ca1_int_enabled )
    {
        cpu_firq(1);
    }
}


/*------------------------------------------------
 * pia_cart_firq()
 *
 *  Assert an external interrupt from the Expansion Cartridge
 *  through PIA1-CB1 that geterates an FIRQ interrupt (e,g, Dragon DOS disk system).
 *
 *  param:  Nothing
 *  return: Nothing
 */
void pia_cart_firq(void)
{
    /* Set the cart FIRQ status bit - turns off on next port read
     */
    memory_IO[PIA1_CRB] |= PIA_CR_IRQ_STAT;

    /* Assert interrupt if enabled
     */
    if ( pia1_cb1_int_enabled )
    {
        cpu_firq(1);
    }
}


/*------------------------------------------------
 * io_handler_pia0_pa()
 *
 *  IO call-back handler 0xFF00 PIA0-A Data read:
 *
 *  Bit 0..6 keyboard row input
 *  Bit 0    Right joystick button input
 *  Bit 1    Left joystick button input
 *  Bit 7    Joystick comparator input
 *
 *  This call-back will only deal with joystick comparator input read.
 *  Keyboard row inputs are handled by io_handler_pia0_pb()
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 *
 * Position Joystick - mux_select bits:
 *   00 Right, Horiz
 *   01 Right, Vert
 *   10 Left, Horiz
 *   11 Left, Vert
 */
ITCM_CODE static uint8_t io_handler_pia0_pa(uint16_t address, uint8_t data, mem_operation_t op)
{
    uint8_t scan_code = 0;
    uint8_t row_switch_bits;
    int     row_index;

    if ( op == MEM_READ )
    {
        /* We are reading the keyboard... plus the comparator bit 7  */

        memset(keyboard_rows, 255, sizeof(keyboard_rows));

        for (int i=0; i<kbd_keys_pressed; i++)
        {
            scan_code = (uint8_t) kbd_keys[i];

            /* Sanity check
             */
            if ( (row_index = (myConfig.machine ? kbd_scan_coco[(scan_code & 0x7f)][1] : kbd_scan_dragon[(scan_code & 0x7f)][1])) != 255 )
            {
                /* Generate row bit patterns emulating row key closures
                 * and match to 'make' or 'break' codes (bit.7 of scan code)
                 */
                row_switch_bits = (myConfig.machine ? kbd_scan_coco[(scan_code & 0x7f)][0] : kbd_scan_dragon[(scan_code & 0x7f)][0]);

                keyboard_rows[row_index] &= row_switch_bits;
            }
        }

        /* Store the appropriate row bit value for PIA0_PA bit pattern
         */
        row_switch_bits = get_keyboard_row_scan(memory_IO[PIA0_PB]);
        mem_write(PIA0_PA, (int) row_switch_bits);

        data = row_switch_bits;

        uint16_t input = 31+myConfig.analogCenter;
        if (myConfig.joystick == 0) // Right Joystick
        {            
            if (mux_select == MUX_RIGHT_Y) // Up-Down axis
            {
                input = joy_y;
            }
            else if (mux_select == MUX_RIGHT_X) // Left-Right axis
            {
                input = joy_x;
            }

            if (input >= dac_output)
            {
                data |= 0x80;
                last_comparator = 0x80;
            }
            else
            {
                data &= 0x7f;
                last_comparator = 0x00;
            }

            if (!kbd_key)
            {
                if ( JoyState & JST_FIRE )
                {
                    data &= ~0x01;
                }
                else
                {
                    data |= 0x01;
                }

                if ( JoyState & JST_FIRE2 )
                {
                    data &= ~0x02;
                }
                else
                {
                    data |= 0x02;
                }
            }
        }
        else // Left Joystick
        {
            if (mux_select == MUX_LEFT_Y) // Up-Down axis
            {
                input = joy_y;
            }
            else if (mux_select == MUX_LEFT_X) // Left-Right axis
            {
                input = joy_x;
            }

            if (input >= dac_output)
            {
                data |= 0x80;
                last_comparator = 0x80;
            }
            else
            {
                data &= 0x7f;
                last_comparator = 0x00;
            }

            if (!kbd_key)
            {
                if ( JoyState & JST_FIRE )
                {
                    data &= ~0x02;
                }
                else
                {
                    data |= 0x02;
                }
            }
        }

        // A read from this port clears the HSync FIRQ
        memory_IO[PIA0_CRA] &= ~PIA_CR_IRQ_STAT;
        cpu_firq(0);
    }

    return data;
}

/*------------------------------------------------
 * io_handler_pia0_pb()
 *
 *  IO call-back handler 0xFF02 PIA0-B Data
 *  Bit 0..7 Output to keyboard columns
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
ITCM_CODE static uint8_t io_handler_pia0_pb(uint16_t address, uint8_t data, mem_operation_t op)
{
    /* Activate the call back to read keyboard scan code from
     * external AVR interface only when testing the keyboard columns
     * for a key press.
     */
    if ( op == MEM_WRITE )
    {
        // The ROM is setting up to read the keyboard...
        // memory_IO[PIA0_PB] will light up a column and we can read the rows
        if (!pia0_ddr_b)
        {
            //data = memory_IO[PIA0_PB];  // If we are not in write mode, return the old value
        }
    }
    /* A read from the port address has the effect of resetting
     * the IRQ status line
     */
    else
    {
        memory_IO[PIA0_CRB] &= ~PIA_CR_IRQ_STAT;  // VSYNC IRQ
        cpu_irq(0);
    }

    return data;
}

/*------------------------------------------------
 * io_handler_pia0_cra()
 *
 *  IO call-back handler 0xFF01 PIA0-A Control register
 *  responding the audio multiplexer select bits
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
ITCM_CODE static uint8_t io_handler_pia0_cra(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        if ( data & 0x08 )
            mux_select |= 0x01;
        else
            mux_select &= ~0x01;

        pia0_ca1_int_enabled = (data & PIA_CR_INTR);

        pia0_ddr_a = (data & PIA_DDR);
    }
    else
    {

    }

    return data;
}

/*------------------------------------------------
 * io_handler_pia0_crb()
 *
 *  IO call-back handler 0xFF03 PIA0-B Control register
 *  to enabled/disable IRQ interrupt source.
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
ITCM_CODE static uint8_t io_handler_pia0_crb(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        if ( data & 0x08 )
            mux_select |= 0x02;
        else
            mux_select &= ~0x02;

        pia0_cb1_int_enabled = (data & PIA_CR_INTR);

        pia0_ddr_b = (data & PIA_DDR);
    }
    else
    {

    }

    return data;
}


inline uint8_t loader_tape_fread(void)
{
    if (tape_pos > file_size)
    {
        cas_eof = 1;
        return 0x00;
    }
    else return TapeCartDiskBuffer[tape_pos++];
}

/*------------------------------------------------
 * io_handler_pia1_pa()
 *
 *  IO call-back handler 0xFF20 Dir PIA1-A output to 6-bit DAC
 *  Traps and handles writes to PA bit.2 to bit.7,
 *  and cassette tape input bit.
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
ITCM_CODE static uint8_t io_handler_pia1_pa(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        if (pia1_ddr_a) // Does the DDR tell us we are normal output?
        {
            dac_output = (data >> 2) & 0x3f;
        }
    }
    else
    {
        /* Reading the cassette tape input bit PIA1-PA0:
         * 1) Bits are fed into PA0 with LSB first
         * 2) a '1' bit toggles PA0 to '0' then '1' for BIT_THRESHOLD_HI/2 reads of PA0
         * 3) a '0' bit toggles PA0 to '0' then '1' for BIT_THRESHOLD_LO/2 reads of PA0
         * 4) The read count threshold of PA0 that determines the bit state is 18
         *    according to the Dragon ROM listing
         * 5) The normal PA0 state is '0'
         *
         * This process fakes the bit stream coming from the cassette tape interface
         * with the advantage that it can synchronize on the bit reads. The interface
         * can be hacked to speed up the load time by changing the threshold of 18
         * in Dragon RAM location 0x0092 to a lower number.
         *
         */
        if ( bit_index == 0 )
        {
            tape_byte = loader_tape_fread();

            bit_index = 9;
            bit_timing_threshold = 0;
            bit_timing_count = 0;

            /* Force sync/fill bytes just in case.
             */
            if ( cas_eof )
            {
                tape_byte = 0x55;
            }
        }

        if ( bit_timing_count == bit_timing_threshold )
        {
            if ( tape_byte & 0b00000001 )
            {
                bit_timing_threshold = BIT_THRESHOLD_HI;
            }
            else
            {
                bit_timing_threshold = BIT_THRESHOLD_LO;
            }

            bit_timing_count = 0;

            tape_byte = tape_byte >> 1;
            bit_index--;
        }

        if ( bit_timing_count < (bit_timing_threshold / 2) )
        {
            data &= 0b11111110;
        }
        else
        {
            data |= 0b00000001;
        }

        bit_timing_count++;
    }

    return data;
}

/*------------------------------------------------
 * io_handler_pia1_pb()
 *
 *  IO call-back handler 0xFF22 Dir PIA1-B Data
 *  Bit 7   O   Screen Mode G/^A
 *  Bit 6   O   Screen Mode GM2
 *  Bit 5   O   Screen Mode GM1
 *  Bit 4   O   Screen Mode GM0 / INT
 *  Bit 3   O   Screen Mode CSS
 *  Bit 2   I   Ram Size (1=16k 0=32/64k)
 *  Bit 1   O   Single bit sound output
 *  Bit 0   I   Rs232 In / Printer Busy, not implemented
 *
 *  A read resets FIRQ request output.
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
ITCM_CODE static uint8_t io_handler_pia1_pb(uint16_t address, uint8_t data, mem_operation_t op)
{
    extern uint8_t pia_video_mode;
    if ( op == MEM_WRITE )
    {
        if (pia1_ddr_b) // Does the DDR tell us we are normal output?
        {
            vdg_set_mode_pia(((data >> 3) & 0x1f));

            extern signed short int beeper_vol;

            if (data & 0x02) // Beeper Pulse
            {
                beeper_vol = (beeper_vol ? 0x000:0xFFF);
            }
        }
    }
    /* A read from the port address has the effect of resetting
     * the IRQ status line
     */
    else
    {
        data = (pia_video_mode << 3); // Also reports 32K (0 for bit 2)
        data |= 1;  // RS232 In/Printer Busy
        memory_IO[PIA1_CRB] &= ~PIA_CR_IRQ_STAT; // Cart IRQ
        cpu_firq(0);
    }

    return data;
}

/*------------------------------------------------
 * io_handler_pia1_cra()
 *
 *  IO call-back handler 0xFF21 PIA1-A Control register
 *  responding the cassette motor on-off select bit CA2
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
static uint8_t io_handler_pia1_cra(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        if ( data & CA2_SET_CLR )
        {
            if ( data & MOTOR_ON )
            {
                tape_motor = 1;
            }
            else
            {
                tape_motor = 0;
            }
        }

        pia1_ddr_a = (data & PIA_DDR);
    }
    else
    {

    }

    return data;
}

/*------------------------------------------------
 * io_handler_pia1_crb()
 *
 *  IO call-back handler 0xFF23 PIA1-B Control register
 *  responding the audio multiplexer select bits, and
 *  PIA1-CRB1 interrupt enable/disable.
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
ITCM_CODE static uint8_t io_handler_pia1_crb(uint16_t address, uint8_t data, mem_operation_t op)
{
    if ( op == MEM_WRITE )
    {
        if ( data & PIA_CR_INTR )
            pia1_cb1_int_enabled = 1;
        else
            pia1_cb1_int_enabled = 0;

        sound_enable = (data & 0x08);

        pia1_ddr_b = (data & PIA_DDR);
    }
    else
    {

    }

    return data;
}

/*------------------------------------------------
 * get_keyboard_row_scan()
 *
 *  Using the Row scan bit pattern and the key closure
 *  matrix in 'keyboard_rows', generate the row scan bit pattern
 *
 *  param:  Row scan bit pattern
 *  return: Column scan bit pattern
 */
ITCM_CODE uint8_t get_keyboard_row_scan(uint8_t row_scan)
{
    uint8_t result = 0;
    uint8_t bit_position = 0x01;
    uint8_t test;
    int     row;

    for ( row = 0; row < KBD_ROWS; row++ )
    {
        test = (~row_scan) & keyboard_rows[row];

        if ( test == (uint8_t)(~row_scan) )
        {
            result |= bit_position;
        }

        bit_position = bit_position << 1;
    }

    return result;
}

