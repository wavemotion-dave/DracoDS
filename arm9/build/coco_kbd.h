
//{{BLOCK(coco_kbd)

//======================================================================
//
//	coco_kbd, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 593 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 11344 + 1748 = 13604
//
//	Time-stamp: 2025-07-06, 08:13:04
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_COCO_KBD_H
#define GRIT_COCO_KBD_H

#define coco_kbdTilesLen 11344
extern const unsigned int coco_kbdTiles[2836];

#define coco_kbdMapLen 1748
extern const unsigned short coco_kbdMap[874];

#define coco_kbdPalLen 512
extern const unsigned short coco_kbdPal[256];

#endif // GRIT_COCO_KBD_H

//}}BLOCK(coco_kbd)
