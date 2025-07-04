/********************************************************************
 * sam.h
 *
 *  Header file that defines the Synchronous Address Multiplexer
 *  MC6883 / SN74LS785, as used in the Dragon 32 computer
 *
 *  February 6, 2021
 *
 *******************************************************************/

#ifndef __SAM_H__
#define __SAM_H__

struct sam_reg_t
{
    uint8_t vdg_mode;
    uint8_t vdg_display_offset;
    uint8_t page;
    uint8_t mpu_rate;
    uint8_t memory_size;
    uint8_t memory_map_type;
};

extern struct sam_reg_t sam_registers;

extern void sam_init(void);
extern uint16_t map_upper_to_lower;

#endif  /* __SAM_H__ */
