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
 * disk.c
 *
 *  This module defines disk cartridge function including
 *  controller IC WD2797, drive and motor control register, and interrupts
 *
 *  resources:
 *  WD2797 floppy disk controller data sheet
 *  Dragon DOS programmer's guide, Grosvenor Software 1985
 *  Dragon DOS cartridge schematics
 *  Dragon DOS source code and ROM iamges https://github.com/prime6809/DragonDOS
 *  https://worldofdragon.org/index.php?title=Tape%5CDisk_Preservation#JVC.2FDSK_File_Format
 *
 *  July 2024
 *
 *******************************************************************/

#include    <stdint.h>
#include    <string.h>

#include    "DracoDS.h"
#include    "DracoUtils.h"
#include    "disk.h"
#include    "cpu.h"
#include    "mem.h"
#include    "pia.h"
#include    "fdc.h"

/* -----------------------------------------
   Module static functions
----------------------------------------- */
static uint8_t  io_handler_wd2797(uint16_t address, uint8_t data, mem_operation_t op);
static uint8_t  io_handler_drive_ctrl(uint16_t address, uint8_t data, mem_operation_t op);

/* -----------------------------------------
   Module globals
----------------------------------------- */
uint8_t nmi_enable = 0;
uint8_t halt_flag = 0;

/*------------------------------------------------
 * disk_init()
 *
 *  Initialize the disk subsystem
 *
 *  param:  Nothing
 *  return: Nothing
 */
void disk_init(void)
{
    if (draco_mode >= MODE_DSK)
    {
        mem_define_io(0xff48, 0xff4b, io_handler_wd2797);
        mem_define_io(0xff40, 0xff40, io_handler_drive_ctrl);

        nmi_enable = 0;

        fdc_reset(true);

        fdc_init(WD2793, 1, 1, (last_file_size >= (180*1024) ? 40:35), 18, 256, 1, TapeCartDiskBuffer, NULL);
    }
}

/*------------------------------------------------
 * io_handler_wd2797()
 *
 *  IO call-back handler disk controller WD2797 registers
 */
static uint8_t io_handler_wd2797(uint16_t address, uint8_t data, mem_operation_t op)
{
    uint8_t     response = 0;

    if ( op == MEM_WRITE )
    {
        fdc_write(address & 7, data);
    }
    else // op == MEM_READ
    {
        response = fdc_read(address & 7);
    }

    return response;
}


/*------------------------------------------------
 * io_handler_drive_ctrl()
 *
 *  IO call-back handler for disk drive and motor control register/IO-port.
 *  The call-back handles and updates drive state/mode parameters.
 *
 *  param:  Call address, data byte for write operation, and operation type
 *  return: Status or data byte
 */
static uint8_t io_handler_drive_ctrl(uint16_t address, uint8_t data, mem_operation_t op)
{
    int drive_num;

    if (op == MEM_WRITE)
    {
        drive_num = (int) (data & 0x07);
        if (drive_num == 1)
        {
            halt_flag  = (data & 0x80);             // Halt flag enable (we assume halt enabled anyway)
            nmi_enable = ((data & 0x20) ? 1 : 0);   // This is normally the density flag... but CoCo re-uses it.
            fdc_setMotor ((data & 0x08) ? 1 : 0);   // Motor enable (on) or disabled (off)
            fdc_setDrive(0);                        // We are drive 0 always
            fdc_setSide(0);                         // Tandy support is single sided only
        }
    }
    
    return data;
}

/*------------------------------------------------
 * disk_intrq()
 *
 *  Trigger an interrupt request on the NMI line.
 *
 *  param:  None
 *  return: None
 */
void disk_intrq(void)
{
    if ( nmi_enable )
    {
        cpu_nmi_trigger();
    }
}
