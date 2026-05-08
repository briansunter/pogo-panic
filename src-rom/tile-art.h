#ifndef POGO_TILE_ART_H
#define POGO_TILE_ART_H

#include <stdint.h>

/* Tile pixel data macros. Both the GBDK ROM build (sdcc) and the host C
 * tooling (gcc/clang) consume these.
 *   TILE2 - 16 raw 2bpp bytes laid out as Game Boy expects: each row is two
 *           bytes (low plane, high plane). Pixel column 0 is bit 7.
 *   TILE8 - 1bpp monochrome shorthand: each input byte is duplicated so the
 *           low and high planes match, yielding an on/off (color 0 or 3) row.
 */
#define TILE2(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p
#define TILE8(a,b,c,d,e,f,g,h) a,a,b,b,c,c,d,d,e,e,f,f,g,g,h,h

/* Background tile art, indexed by enum TileType (see game-logic.h).
 * 16 bytes per tile. Defined in tile-art.c so both the ROM and host
 * tools (tools/dump-tiles.c) can link against it. */
extern const uint8_t bg_tiles[];
extern const uint16_t bg_tiles_byte_count;

#define BG_TILE_BYTES 16u

#endif /* POGO_TILE_ART_H */
