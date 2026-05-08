/* Host-side tests for the pure game logic in src-rom/game-logic.c. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game-logic.h"

static void test_tile_flags(void) {
    assert(tile_has_flag(T_SPIKE, TILEF_DANGER));
    assert(tile_has_flag(T_WATER, TILEF_DANGER));
    assert(!tile_has_flag(T_EMPTY, TILEF_DANGER));
    assert(!tile_has_flag(T_COIN, TILEF_DANGER));

    assert(tile_has_flag(T_SOLID, TILEF_SOLID));
    assert(tile_has_flag(T_CRACK, TILEF_SOLID));
    assert(tile_has_flag(T_TOGGLE, TILEF_SOLID));
    assert(tile_has_flag(T_WALL, TILEF_SOLID));
    assert(tile_has_flag(T_MOVING, TILEF_SOLID));

    /* Out-of-range tile id returns 0 (no flags). */
    assert(!tile_has_flag(0xff, TILEF_SOLID));

    switch_on = 1;
    assert(is_solid_tile(T_TOGGLE));
    assert(bg_for_tile(T_TOGGLE) == T_TOGGLE);

    switch_on = 0;
    assert(!is_solid_tile(T_TOGGLE));
    assert(bg_for_tile(T_TOGGLE) == T_TOGGLE_OFF);

    switch_on = 1;

    assert(is_danger_tile(T_SPIKE));
    assert(is_danger_tile(T_WATER));
    assert(!is_danger_tile(T_SOLID));

    /* Out-of-range coordinates clamp to T_SOLID. */
    assert(stage_tile(99, 99) == T_SOLID);
    assert(stage_tile(SCREEN_TILES_W, 0) == T_SOLID);
    assert(stage_tile(0, SCREEN_TILES_H) == T_SOLID);

    puts("PASS: test_tile_flags");
}

static void test_rng(void) {
    uint16_t seed;
    uint16_t first;
    uint16_t second;
    uint8_t distinct[256];
    uint8_t distinct_count;
    uint16_t i;

    /* Formula: seed * 1109 + 1987, truncated to uint16_t. */
    assert(rng_step(0) == (uint16_t)1987u);
    assert(rng_step(1) == (uint16_t)(1109u + 1987u));
    assert(rng_step(123) == (uint16_t)(123u * 1109u + 1987u));

    /* Determinism: repeated calls with the same seed produce identical results. */
    first = rng_step(0xbeefu);
    second = rng_step(0xbeefu);
    assert(first == second);

    /* Sequence variation: at least 8 distinct uint8_t-truncated values
       in the next 16 chained steps. */
    memset(distinct, 0, sizeof(distinct));
    distinct_count = 0;
    seed = 1u;
    for (i = 0; i < 16u; i++) {
        uint8_t lo;
        seed = rng_step(seed);
        lo = (uint8_t)seed;
        if (!distinct[lo]) {
            distinct[lo] = 1;
            distinct_count++;
        }
    }
    assert(distinct_count >= 8);

    puts("PASS: test_rng");
}

static void test_save(void) {
    uint8_t i;
    uint8_t reference;

    memset(&save_data, 0xAA, sizeof(SaveData));
    save_defaults();

    assert(save_data.magic == SAVE_MAGIC);
    assert(save_data.version == SAVE_VERSION);
    assert(save_data.unlocked == 1);
    assert(save_data.panic_best == 0);
    for (i = 0; i < NUM_ADVENTURE_LEVELS; i++) {
        assert(save_data.best_time[i] == 0xffffu);
    }
    assert(save_data.checksum == checksum_save(&save_data));

    /* Tampering with a best_time entry must invalidate the checksum. */
    save_data.best_time[5] = 0x1234u;
    assert(save_data.checksum != checksum_save(&save_data));

    /* Reset, then tamper with magic; checksum should still differ. */
    save_defaults();
    reference = save_data.checksum;
    save_data.magic = 0xdeadu;
    assert(save_data.checksum != checksum_save(&save_data));
    (void)reference;

    puts("PASS: test_save");
}

static uint8_t is_mechanic_tile(uint8_t t) {
    return (uint8_t)(t == T_SPRING || t == T_SWITCH || t == T_TOGGLE ||
                     t == T_FAN_L || t == T_FAN_R || t == T_CONV_L ||
                     t == T_CONV_R || t == T_MOVING || t == T_CRACK ||
                     t == T_BUBBLE || t == T_ROCK);
}

static uint8_t is_platform_tile(uint8_t t) {
    return (uint8_t)(t == T_SOLID || t == T_CRACK || t == T_TOGGLE ||
                     t == T_CONV_L || t == T_CONV_R || t == T_MOVING);
}

static void test_adventure_levels(void) {
    /* Per-world mechanic-tile aggregates (skip world 0 levels 0..7 tutorial). */
    uint32_t world_mech_sum[5];
    uint32_t world_room_count[5];
    uint8_t level;
    uint8_t world;
    uint8_t local;
    uint8_t x;
    uint8_t y;
    uint8_t found_battery;
    uint8_t found_platform;
    uint32_t mech_count;
    static const uint32_t minimum_avg[5] = {4u, 7u, 3u, 12u, 10u};

    memset(world_mech_sum, 0, sizeof(world_mech_sum));
    memset(world_room_count, 0, sizeof(world_room_count));

    for (level = 0; level < NUM_ADVENTURE_LEVELS; level++) {
        generate_adventure(level);
        world = (uint8_t)(level >> 4);
        local = (uint8_t)(level & 15u);

        /* Exit tile always at (18, 16). */
        assert(stage[16][18] == T_EXIT);

        /* At least one battery tile somewhere on the stage. */
        found_battery = 0;
        for (y = 0; y < SCREEN_TILES_H && !found_battery; y++) {
            for (x = 0; x < SCREEN_TILES_W; x++) {
                if (stage[y][x] == T_BATTERY) {
                    found_battery = 1;
                    break;
                }
            }
        }
        assert(found_battery);

        /* Boundary walls. */
        assert(stage[CEILING_Y][0] == T_WALL);
        assert(stage[CEILING_Y][SCREEN_TILES_W - 1] == T_WALL);
        assert(stage[17][0] == T_WALL);
        assert(stage[17][SCREEN_TILES_W - 1] == T_WALL);

        /* Exit pocket is clear of danger. */
        assert(stage[16][15] != T_SPIKE && stage[16][15] != T_WATER);
        assert(stage[16][17] != T_SPIKE && stage[16][17] != T_WATER);

        /* At least one platform-style tile is present. */
        found_platform = 0;
        for (y = 0; y < SCREEN_TILES_H && !found_platform; y++) {
            for (x = 0; x < SCREEN_TILES_W; x++) {
                if (is_platform_tile(stage[y][x])) {
                    found_platform = 1;
                    break;
                }
            }
        }
        assert(found_platform);

        /* Mechanic-tile totals (skip world 0 tutorial levels). */
        if (world == 0u && local < 8u) continue;

        mech_count = 0;
        for (y = 0; y < SCREEN_TILES_H; y++) {
            for (x = 0; x < SCREEN_TILES_W; x++) {
                if (is_mechanic_tile(stage[y][x])) mech_count++;
            }
        }
        world_mech_sum[world] += mech_count;
        world_room_count[world] += 1u;
    }

    for (world = 0; world < 5u; world++) {
        uint32_t avg;
        assert(world_room_count[world] > 0u);
        avg = world_mech_sum[world] / world_room_count[world];
        assert(avg >= minimum_avg[world]);
    }

    puts("PASS: test_adventure_levels");
}

static void test_panic_levels(void) {
    static const uint16_t depths[] = {0u, 1u, 5u, 10u, 25u, 50u};
    uint8_t snapshot[SCREEN_TILES_H][SCREEN_TILES_W];
    uint16_t i;
    uint8_t x;
    uint8_t y;
    uint8_t found_battery;
    uint32_t danger_at_zero;
    uint32_t danger_at_fifty;
    uint32_t danger_count;

    danger_at_zero = 0;
    danger_at_fifty = 0;

    for (i = 0; i < (uint16_t)(sizeof(depths) / sizeof(depths[0])); i++) {
        uint16_t depth = depths[i];
        generate_panic(depth);

        assert(stage[16][18] == T_EXIT);
        assert(stage[13][4] == T_SWITCH);
        assert(stage[12][6] == T_BUBBLE);

        found_battery = 0;
        danger_count = 0;
        for (y = 0; y < SCREEN_TILES_H; y++) {
            for (x = 0; x < SCREEN_TILES_W; x++) {
                uint8_t t = stage[y][x];
                if (t == T_BATTERY) found_battery = 1;
                if (t == T_SPIKE || t == T_WATER) danger_count++;
            }
        }
        assert(found_battery);

        if (depth == 0u) danger_at_zero = danger_count;
        if (depth == 50u) danger_at_fifty = danger_count;
    }

    assert(danger_at_fifty >= danger_at_zero);

    /* Determinism: regenerating the same depth must produce identical stage. */
    generate_panic(7);
    memcpy(snapshot, stage, sizeof(snapshot));
    reset_common();
    generate_panic(7);
    assert(memcmp(snapshot, stage, sizeof(snapshot)) == 0);

    puts("PASS: test_panic_levels");
}

int main(void) {
    test_tile_flags();
    reset_common();

    test_rng();
    reset_common();

    test_save();
    reset_common();

    test_adventure_levels();
    reset_common();

    test_panic_levels();
    reset_common();

    puts("All C tests passed.");
    return 0;
}
