
//{{BLOCK(cassette)

//======================================================================
//
//	cassette, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 330 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 7712 + 988 = 9212
//
//	Time-stamp: 2025-07-06, 11:28:27
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_CASSETTE_H
#define GRIT_CASSETTE_H

#define cassetteTilesLen 7712
extern const unsigned int cassetteTiles[1928];

#define cassetteMapLen 988
extern const unsigned short cassetteMap[494];

#define cassettePalLen 512
extern const unsigned short cassettePal[256];

#endif // GRIT_CASSETTE_H

//}}BLOCK(cassette)
