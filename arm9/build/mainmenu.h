
//{{BLOCK(mainmenu)

//======================================================================
//
//	mainmenu, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 244 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 4292 + 772 = 5576
//
//	Time-stamp: 2025-07-06, 08:13:04
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_MAINMENU_H
#define GRIT_MAINMENU_H

#define mainmenuTilesLen 4292
extern const unsigned int mainmenuTiles[1073];

#define mainmenuMapLen 772
extern const unsigned short mainmenuMap[386];

#define mainmenuPalLen 512
extern const unsigned short mainmenuPal[256];

#endif // GRIT_MAINMENU_H

//}}BLOCK(mainmenu)
