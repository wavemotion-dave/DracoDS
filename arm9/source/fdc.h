// =====================================================================================
// Copyright (c) 2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The DracoDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================

#ifndef _FDC_H
#define _FDC_H

#define WD1770  0
#define WD2793  1

// The Tandy CoCo FDC controller - we only allow for single sided 40 track max
struct FDC_t
{
    u8  status;
    u8  command;
    u8  track;
    u8  sector;
    u8  data;
    u8  drive;
    u8  side;
    u8  motor;
    u8  wait_for_read;
    u8  wait_for_write;
    u8  commandType;
    u8  write_track_allowed;
    u8  stepDirection;
    u8  spare;
    u8  track_dirty;         // True if the track must be written back to the main buffer
    u8  disk_write;          // True if the disk has been written and not saved
    u8  write_tracks[40];    // Up to 40 tracks can be marked as needing writing
    u8  track_buffer[4608];  // Enough for 18 sectors of 256 bytes
    u16 track_buffer_idx;
    u16 track_buffer_end;
    u16 indexPulseCounter;
    u16 sector_byte_counter;
    u16 write_track_byte_counter;
};

struct FDC_GEOMETRY_t
{
    u8  fdc_type;        // Either WD1770 or WD2793
    u8  drives;
    u8  sides;
    u8  tracks;
    u8  sectors;
    u16 sectorSize;
    u8  startSector;
    u8 *disk0;
    u8 *disk1;
};

extern struct FDC_t             FDC;
extern struct FDC_GEOMETRY_t    Geom;

extern u8 io_show_status;

extern u8   fdc_read(u8 addr);
extern void fdc_write(u8 addr, u8 data);
extern void fdc_setSide(u8 side);
extern void fdc_setDrive(u8 drive);
extern void fdc_setMotor(u8 onOff);
extern void fdc_reset(u8 full_reset);
extern void fdc_init(u8 fdc_type, u8 drives, u8 sides, u8 tracks, u8 sectors, u16 sectorSize, u8 startSector, u8 *diskBuffer0, u8 *diskBuffer1);

#endif //_FDC_H
