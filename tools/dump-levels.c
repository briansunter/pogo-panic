/* Host-side tool: regenerates every Adventure room via the real ROM C
 * generator and writes them to stdout as JSON. Run as part of `make`.
 * Linked against src-rom/game-logic.c.
 *
 * The JSON shape mirrors what `generateAdventureLevels()` returned in JS
 * before this tool replaced it: each room exposes stage[][], enemies as
 * tile coords with vx, spawn in pixels, switchOn boolean, and metadata.
 *
 * If argv[1] is provided, the tile-table category flags are written there
 * as JSON so JS callers can ask the table the same questions C callers do
 * (solid/danger/mechanic/platform/pocket_solid) instead of mirroring tile
 * id lists by hand. */
#include <stdio.h>
#include <string.h>
#include "game-logic.h"

static void emit_stage_row(uint8_t y, uint8_t last_row) {
    uint8_t x;
    printf("      [");
    for (x = 0; x < SCREEN_TILES_W; x++) {
        printf("%u%s", (unsigned)stage[y][x], x + 1 == SCREEN_TILES_W ? "" : ",");
    }
    printf("]%s\n", last_row ? "" : ",");
}

static void emit_room(uint8_t lvl) {
    uint8_t y, i;
    uint8_t world = (uint8_t)(lvl >> 4);
    uint8_t local = (uint8_t)(lvl & 15u);
    int spawn_px_x;
    int spawn_px_y;

    /* Re-run generator. Each call invokes reset_common() which clears stage,
     * enemies, and switch_on; safe to call repeatedly. */
    generate_adventure(lvl);

    /* player_x/player_y are stored in fixed-point pixels (FIX = v << 4).
     * Convert back to raw pixels so the spawn matches the JS shape. */
    spawn_px_x = (int)player_x >> FP_SHIFT;
    spawn_px_y = (int)player_y >> FP_SHIFT;

    printf("  {\n");
    printf("    \"level\": %u,\n", (unsigned)lvl);
    printf("    \"world\": %u,\n", (unsigned)world);
    printf("    \"local\": %u,\n", (unsigned)local);
    printf("    \"spawn\": { \"x\": %d, \"y\": %d },\n", spawn_px_x, spawn_px_y);
    printf("    \"switchOn\": %s,\n", switch_on ? "true" : "false");
    printf("    \"moving\": ");
    if (moving_active) {
        printf("{ \"x\": %u, \"y\": %u, \"w\": %u, \"dir\": %d }",
            (unsigned)moving_x, (unsigned)moving_y, (unsigned)moving_w, (int)moving_dir);
    } else {
        printf("null");
    }
    printf(",\n");
    printf("    \"stage\": [\n");
    for (y = 0; y < SCREEN_TILES_H; y++) {
        emit_stage_row(y, (uint8_t)(y + 1u == SCREEN_TILES_H));
    }
    printf("    ],\n");
    printf("    \"enemies\": [");
    for (i = 0; i < enemy_count; i++) {
        /* Enemy x/y are stored as FIX(tx * TILE_PX). Recover tile coords. */
        int tx = ((int)enemies[i].x >> FP_SHIFT) / TILE_PX;
        int ty = ((int)enemies[i].y >> FP_SHIFT) / TILE_PX;
        printf("%s{\"tx\":%d,\"ty\":%d,\"vx\":%d}",
            i == 0 ? "" : ",", tx, ty, (int)enemies[i].vx);
    }
    printf("]\n");
    printf("  }%s\n", lvl + 1u == NUM_ADVENTURE_LEVELS ? "" : ",");
}

/* Tile id -> short name. Stringified from the same TILE_LIST macro that
 * declares enum TileType, so the JSON's name strings are guaranteed to
 * agree with the enum. The JS side reads these to build its TILE map. */
static const char *const tile_names[TILE_COUNT] = {
#define TILE_NAME(name) #name,
    TILE_LIST(TILE_NAME)
#undef TILE_NAME
};

static void emit_tile_table(FILE *out) {
    uint16_t t;
    fprintf(out, "{\n");
    fprintf(out, "  \"count\": %u,\n", (unsigned)TILE_COUNT);
    fprintf(out, "  \"flags\": {\n");
    fprintf(out, "    \"solid\": %u,\n", (unsigned)TILEF_SOLID);
    fprintf(out, "    \"danger\": %u,\n", (unsigned)TILEF_DANGER);
    fprintf(out, "    \"mechanic\": %u,\n", (unsigned)TILEF_MECHANIC);
    fprintf(out, "    \"platform\": %u,\n", (unsigned)TILEF_PLATFORM);
    fprintf(out, "    \"pocketSolid\": %u\n", (unsigned)TILEF_POCKET_SOLID);
    fprintf(out, "  },\n");
    fprintf(out, "  \"tiles\": [\n");
    for (t = 0; t < TILE_COUNT; t++) {
        fprintf(out, "    { \"id\": %u, \"name\": \"%s\", \"flags\": %u, \"solid\": %s, \"danger\": %s, \"mechanic\": %s, \"platform\": %s, \"pocketSolid\": %s }%s\n",
            (unsigned)t,
            tile_names[t],
            (unsigned)tile_table[t].flags,
            tile_has_flag((uint8_t)t, TILEF_SOLID) ? "true" : "false",
            tile_has_flag((uint8_t)t, TILEF_DANGER) ? "true" : "false",
            tile_is_mechanic((uint8_t)t) ? "true" : "false",
            tile_is_platform((uint8_t)t) ? "true" : "false",
            tile_is_pocket_solid((uint8_t)t) ? "true" : "false",
            (t + 1u == TILE_COUNT) ? "" : ",");
    }
    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
}

int main(int argc, char **argv) {
    uint8_t lvl;
    /* Mimic the ROM init order: defaults populate save_data so any reads in
     * the generator path don't see uninitialized memory. */
    save_defaults();
    printf("[\n");
    for (lvl = 0; lvl < NUM_ADVENTURE_LEVELS; lvl++) emit_room(lvl);
    printf("]\n");

    if (argc > 1) {
        FILE *tile_out = fopen(argv[1], "w");
        if (!tile_out) {
            fprintf(stderr, "dump-levels: cannot open %s for writing\n", argv[1]);
            return 1;
        }
        emit_tile_table(tile_out);
        fclose(tile_out);
    }
    return 0;
}
