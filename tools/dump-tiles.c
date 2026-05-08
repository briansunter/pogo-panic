/* Host-side tool: decodes src-rom/tile-art.c's bg_tiles[] (the same byte
 * array the ROM uploads via set_bkg_data) into JSON the JS debug viewer can
 * render directly. Sibling of tools/dump-levels.c.
 *
 * Output (stdout):
 *   {
 *     "tilePx": 8,
 *     "count": <N>,
 *     "tiles": [
 *       { "index": 0, "pixels": [[0,0,...8 cols], ...8 rows] },
 *       ...
 *     ]
 *   }
 *
 * Each pixel is a 0-3 Game Boy color index (0 = lightest, 3 = darkest);
 * the JS side maps those to the Pocket palette. We emit the decoded grid
 * (rather than raw bytes) so the consumer doesn't have to re-implement the
 * 2bpp interleave; the byte-for-byte source of truth still lives in
 * src-rom/tile-art.c. */
#include <stdio.h>
#include <stdint.h>
#include "tile-art.h"

#define TILE_PX 8

static void emit_tile(uint16_t index, uint8_t last) {
    const uint8_t *bytes = &bg_tiles[index * BG_TILE_BYTES];
    uint8_t row, col;
    printf("    { \"index\": %u, \"pixels\": [\n", (unsigned)index);
    for (row = 0; row < TILE_PX; row++) {
        uint8_t lo = bytes[row * 2u];
        uint8_t hi = bytes[row * 2u + 1u];
        printf("      [");
        for (col = 0; col < TILE_PX; col++) {
            uint8_t shift = (uint8_t)(7u - col);
            uint8_t low_bit = (uint8_t)((lo >> shift) & 1u);
            uint8_t high_bit = (uint8_t)((hi >> shift) & 1u);
            uint8_t color = (uint8_t)((high_bit << 1) | low_bit);
            printf("%u%s", (unsigned)color, col + 1u == TILE_PX ? "" : ",");
        }
        printf("]%s\n", row + 1u == TILE_PX ? "" : ",");
    }
    printf("    ]}%s\n", last ? "" : ",");
}

int main(void) {
    uint16_t count = (uint16_t)(bg_tiles_byte_count / BG_TILE_BYTES);
    uint16_t i;
    printf("{\n");
    printf("  \"tilePx\": %u,\n", (unsigned)TILE_PX);
    printf("  \"count\": %u,\n", (unsigned)count);
    printf("  \"tiles\": [\n");
    for (i = 0; i < count; i++) emit_tile(i, (uint8_t)(i + 1u == count));
    printf("  ]\n");
    printf("}\n");
    return 0;
}
