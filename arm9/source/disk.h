/********************************************************************
 * disk.h
 *
 *  Header file that defines disk cartridge function including
 *  controller IC WD2797, drive and motor control register, and interrupts
 *
 *  July 2024
 *
 *******************************************************************/

#ifndef __DISK_H__
#define __DISK_H__

extern uint8_t nmi_enable;

void disk_init(void);
void disk_io_interrupt(void);

#endif  /* __DISK_H__ */
