/* Host-side tool: regenerates every Adventure room via the real ROM C
 * generator and writes them to stdout as JSON. Run as part of `make`.
 * Linked against src-rom/game-logic.c.
 *
 * The JSON shape mirrors what `generateAdventureLevels()` returned in JS
 * before this tool replaced it: each room exposes stage[][], enemies as
 * tile coords with vx, spawn in pixels, switchOn boolean, and metadata. */
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

int main(void) {
    uint8_t lvl;
    /* Mimic the ROM init order: defaults populate save_data so any reads in
     * the generator path don't see uninitialized memory. */
    save_defaults();
    printf("[\n");
    for (lvl = 0; lvl < NUM_ADVENTURE_LEVELS; lvl++) emit_room(lvl);
    printf("]\n");
    return 0;
}
