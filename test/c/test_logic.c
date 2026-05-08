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

    /* In-bounds reads return the actual tile. */
    stage[5][7] = T_SPIKE;
    assert(stage_tile(7, 5) == T_SPIKE);
    stage[5][7] = T_EMPTY;

    /* Non-T_TOGGLE tiles pass through is_solid_tile and bg_for_tile. */
    assert(is_solid_tile(T_SOLID));
    assert(!is_solid_tile(T_EMPTY));
    assert(bg_for_tile(T_SPIKE) == T_SPIKE);
    assert(bg_for_tile(T_COIN) == T_COIN);

    /* tile_at_fixed converts world fixed-point to a tile lookup. Each tile is
     * 8 px wide; FP_SHIFT scales pixels to fixed-point. */
    stage[3][4] = T_WATER;
    assert(tile_at_fixed(FIX(4 * TILE_PX + 1), FIX(3 * TILE_PX + 1)) == T_WATER);
    /* Out-of-range fixed-point coords clamp to T_SOLID (treated as wall). */
    assert(tile_at_fixed((int16_t)-1, FIX(0)) == T_SOLID);
    stage[3][4] = T_EMPTY;

    puts("PASS: test_tile_flags");
}

static void test_tile_table(void) {
    uint16_t t;

    /* Predicates must agree with the table's flag bits for every tile id. */
    for (t = 0; t < TILE_COUNT; t++) {
        uint8_t solid_pred = tile_has_flag((uint8_t)t, TILEF_SOLID);
        uint8_t solid_tab  = (uint8_t)((tile_table[t].flags & TILEF_SOLID) != 0u);
        uint8_t danger_pred = tile_has_flag((uint8_t)t, TILEF_DANGER);
        uint8_t danger_tab  = (uint8_t)((tile_table[t].flags & TILEF_DANGER) != 0u);
        assert(solid_pred == solid_tab);
        assert(danger_pred == danger_tab);
    }

    /* Spring is the one tile with the spring bounce velocity pair. */
    assert(tile_table[T_SPRING].bounce_vy == BOUNCE_SPRING);
    assert(tile_table[T_SPRING].bounce_vy_short == BOUNCE_SPRING_SHORT);

    /* All other solid tiles use the standard bounce pair. */
    assert(tile_table[T_SOLID].bounce_vy == BOUNCE_NORMAL);
    assert(tile_table[T_SOLID].bounce_vy_short == BOUNCE_SHORT);
    assert(tile_table[T_CRACK].bounce_vy == BOUNCE_NORMAL);
    assert(tile_table[T_CONV_R].bounce_vy == BOUNCE_NORMAL);

    /* Pickup tiles fire the right event. The table stores GameEvent + 1
     * because EVENT_COIN == 0 collides with the "no event" sentinel. */
    assert(tile_table[T_COIN].pickup_event == (uint8_t)(EVENT_COIN + 1));
    assert(tile_table[T_COIN].consumed_on_pickup == 1);
    assert(tile_table[T_KEY].pickup_event == (uint8_t)(EVENT_KEY + 1));
    assert(tile_table[T_KEY].consumed_on_pickup == 1);
    assert(tile_table[T_BUBBLE].pickup_event == (uint8_t)(EVENT_BUBBLE + 1));
    assert(tile_table[T_BUBBLE].consumed_on_pickup == 1);

    /* Stomp-breakable tiles. */
    assert(tile_table[T_ROCK].breakable_by_stomp == 1);
    assert(tile_table[T_CRACK].breakable_by_stomp == 1);
    assert(tile_table[T_SOLID].breakable_by_stomp == 0);
    assert(tile_table[T_SPRING].breakable_by_stomp == 0);

    /* Empty tile is fully inert. */
    assert(tile_table[T_EMPTY].flags == 0);
    assert(tile_table[T_EMPTY].bounce_vy == 0);
    assert(tile_table[T_EMPTY].bounce_vy_short == 0);
    assert(tile_table[T_EMPTY].conveyor_dx == 0);
    assert(tile_table[T_EMPTY].pickup_event == 0);
    assert(tile_table[T_EMPTY].consumed_on_pickup == 0);
    assert(tile_table[T_EMPTY].breakable_by_stomp == 0);

    /* Conveyors push opposite directions; fans do too with smaller magnitude. */
    assert(tile_table[T_CONV_R].conveyor_dx > 0);
    assert(tile_table[T_CONV_L].conveyor_dx < 0);
    assert(tile_table[T_FAN_R].conveyor_dx > 0);
    assert(tile_table[T_FAN_L].conveyor_dx < 0);

    /* is_stomp_breakable agrees with the table for every tile id. */
    for (t = 0; t < TILE_COUNT; t++) {
        assert(is_stomp_breakable((uint8_t)t) == tile_table[t].breakable_by_stomp);
    }

    puts("PASS: test_tile_table");
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
    uint8_t *blob;
    uint16_t len;

    /* Scribble random bytes through the blob accessor (we no longer have
     * direct access to save_data) before calling save_defaults(). */
    save_data_blob(&blob, &len);
    memset(blob, 0xAA, len);
    save_defaults();

    /* Defaults pass validation. */
    assert(save_validate() == 1);
    assert(save_unlocked_count() == 1);
    assert(save_panic_best() == 0);
    for (i = 0; i < NUM_ADVENTURE_LEVELS; i++) {
        assert(save_best_time(i) == 0xffffu);
    }

    /* Out-of-range level returns the sentinel. */
    assert(save_best_time(NUM_ADVENTURE_LEVELS) == 0xffffu);
    assert(save_best_time(0xff) == 0xffffu);

    /* Blob accessor reports correct address and size. */
    save_data_blob(&blob, &len);
    assert(blob != NULL);
    assert(len == sizeof(SaveData));

    /* Tampering with the magic byte invalidates the save. */
    blob[0] ^= 0xff;
    assert(!save_validate());

    /* Resetting restores validity. */
    save_defaults();
    assert(save_validate());

    /* save_record_clear records best time only; it does NOT touch unlocked. */
    save_record_clear(5, 1234);
    assert(save_best_time(5) == 1234);
    assert(save_unlocked_count() == 1); /* unlocked unchanged */
    assert(save_validate());

    /* save_unlock_through is the explicit unlock API.  After clearing level 5
     * the caller decides whether to unlock; main.c gates this on MODE_ADVENTURE
     * so TRIAL/PANIC plays cannot grant Adventure progress. */
    save_unlock_through(5);
    assert(save_unlocked_count() == 7);
    assert(save_validate());

    /* Monotonicity: a slower time does not overwrite. */
    save_record_clear(5, 9999);
    assert(save_best_time(5) == 1234);
    assert(save_validate());

    /* A faster time does overwrite. */
    save_record_clear(5, 100);
    assert(save_best_time(5) == 100);
    assert(save_validate());

    /* Out-of-range level is a no-op (does not corrupt anything). */
    save_record_clear(NUM_ADVENTURE_LEVELS, 0);
    assert(save_validate());
    assert(save_unlocked_count() == 7);

    /* For the final level, (level + 1) is not < NUM_ADVENTURE_LEVELS, so the
     * unlock bump is skipped. unlocked stays at 7. */
    save_record_clear((uint8_t)(NUM_ADVENTURE_LEVELS - 1u), 50);
    save_unlock_through((uint8_t)(NUM_ADVENTURE_LEVELS - 1u));
    assert(save_validate());
    assert(save_unlocked_count() == 7);

    /* Panic: monotonic increase only. */
    save_defaults();
    assert(save_validate());
    save_record_panic(50);
    assert(save_panic_best() == 50);
    assert(save_validate());
    save_record_panic(30);
    assert(save_panic_best() == 50);
    assert(save_validate());
    save_record_panic(75);
    assert(save_panic_best() == 75);
    assert(save_validate());

    puts("PASS: test_save");
}

static void test_hud_feedback(void) {
    int i;

    /* Defaults: nothing active. (feedback state is private; observe via accessors.) */

    /* SHIELD activates with kind FEEDBACK_SHIELD. */
    hud_feedback_on_event(EVENT_SHIELD);
    assert(hud_feedback_active());
    assert(hud_feedback_kind() == FEEDBACK_SHIELD);

    /* Tick down to expiry. */
    for (i = 0; i < 36; i++) hud_feedback_tick();
    assert(!hud_feedback_active());
    assert(hud_feedback_kind() == FEEDBACK_NONE);

    /* STOMP activates if nothing else is up. */
    hud_feedback_on_event(EVENT_STOMP);
    assert(hud_feedback_active());
    assert(hud_feedback_kind() == FEEDBACK_STOMP);

    /* CRACK does NOT supersede an active STOMP (current behavior preserved). */
    hud_feedback_on_event(EVENT_CRACK);
    assert(hud_feedback_kind() == FEEDBACK_STOMP);

    /* But SHIELD DOES supersede (it doesn't gate on active timer). */
    hud_feedback_on_event(EVENT_SHIELD);
    assert(hud_feedback_kind() == FEEDBACK_SHIELD);

    /* Events without feedback mapping are no-ops. First, drain the SHIELD timer. */
    for (i = 0; i < 36; i++) hud_feedback_tick();
    assert(!hud_feedback_active());
    hud_feedback_on_event(EVENT_COIN);
    assert(!hud_feedback_active());
    hud_feedback_on_event(EVENT_SWITCH);
    assert(!hud_feedback_active());

    /* Expired flag is true only on the tick where timer reached 0. */
    hud_feedback_on_event(EVENT_CRACK); /* timer = 18 */
    for (i = 0; i < 17; i++) {
        hud_feedback_tick();
        assert(!hud_feedback_expired_this_tick());
    }
    hud_feedback_tick();
    assert(hud_feedback_expired_this_tick());
    hud_feedback_tick();
    assert(!hud_feedback_expired_this_tick());

    puts("PASS: test_hud_feedback");
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

static uint8_t is_pocket_solid_tile(uint8_t t) {
    return (uint8_t)(t == T_SOLID || t == T_CRACK || t == T_SPRING ||
                     t == T_SWITCH || t == T_TOGGLE || t == T_CONV_L ||
                     t == T_CONV_R || t == T_MOVING || t == T_ROCK ||
                     t == T_WALL);
}

static uint8_t key_is_in_one_tile_pocket(uint8_t x, uint8_t y) {
    if (x == 0u || x >= (uint8_t)(SCREEN_TILES_W - 1u) || y >= (uint8_t)(SCREEN_TILES_H - 1u)) return 0;
    return (uint8_t)(is_pocket_solid_tile(stage[y][x - 1u]) &&
                     is_pocket_solid_tile(stage[y][x + 1u]) &&
                     is_pocket_solid_tile(stage[y + 1u][x]));
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
    uint8_t found_key;
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

        /* At least one key tile somewhere on the stage. */
        found_key = 0;
        for (y = 0; y < SCREEN_TILES_H && !found_key; y++) {
            for (x = 0; x < SCREEN_TILES_W; x++) {
                if (stage[y][x] == T_KEY) {
                    found_key = 1;
                    assert(!key_is_in_one_tile_pocket(x, y));
                    break;
                }
            }
        }
        assert(found_key);

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
    uint8_t found_key;
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

        found_key = 0;
        danger_count = 0;
        for (y = 0; y < SCREEN_TILES_H; y++) {
            for (x = 0; x < SCREEN_TILES_W; x++) {
                uint8_t t = stage[y][x];
                if (t == T_KEY) {
                    found_key = 1;
                    assert(!key_is_in_one_tile_pocket(x, y));
                }
                if (t == T_SPIKE || t == T_WATER) danger_count++;
            }
        }
        assert(found_key);

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

    test_tile_table();
    reset_common();

    test_rng();
    reset_common();

    test_save();
    reset_common();

    test_hud_feedback();
    reset_common();

    test_adventure_levels();
    reset_common();

    test_panic_levels();
    reset_common();

    puts("All C tests passed.");
    return 0;
}
