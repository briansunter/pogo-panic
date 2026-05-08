#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "game-logic.h"

/* ------------------------------------------------------------------ */
/* Tile behavior table                                                */
/*                                                                    */
/* One row per TileType. All per-tile gameplay rules live here so that */
/* adding a new tile is mostly a matter of adding an enum value and a  */
/* row below. Predicates (tile_has_flag, is_solid_tile, is_danger_tile,*/
/* is_stomp_breakable) and call sites (do_bounce, collect_tiles,       */
/* apply_player_world_forces) read this table instead of switching on  */
/* the tile id.                                                        */
/* ------------------------------------------------------------------ */

#define TF_SOLID_PLATFORM (TILEF_SOLID | TILEF_PLATFORM | TILEF_POCKET_SOLID)
#define TF_SOLID_PLATFORM_MECH (TF_SOLID_PLATFORM | TILEF_MECHANIC)

const TileBehavior tile_table[TILE_COUNT] = {
    /* T_EMPTY        */ { 0,                                                  0,             0,                   0,        0,                0, 0 },
    /* T_SOLID        */ { TF_SOLID_PLATFORM,                                  BOUNCE_NORMAL, BOUNCE_SHORT,        0,        0,                0, 0 },
    /* T_CRACK        */ { TF_SOLID_PLATFORM_MECH,                             BOUNCE_NORMAL, BOUNCE_SHORT,        0,        0,                0, 1 },
    /* T_SPRING       */ { TILEF_SOLID | TILEF_MECHANIC | TILEF_POCKET_SOLID,  BOUNCE_SPRING, BOUNCE_SPRING_SHORT, 0,        0,                0, 0 },
    /* T_SPIKE        */ { TILEF_DANGER,                                       0,             0,                   0,        0,                0, 0 },
    /* T_COIN         */ { 0,                                                  0,             0,                   0,        EVENT_COIN + 1,   1, 0 },
    /* T_KEY          */ { 0,                                                  0,             0,                   0,        EVENT_KEY + 1,   1, 0 },
    /* T_EXIT         */ { 0,                                                  0,             0,                   0,        0,                0, 0 },
    /* T_SWITCH       */ { TILEF_SOLID | TILEF_MECHANIC | TILEF_POCKET_SOLID,  BOUNCE_NORMAL, BOUNCE_SHORT,        0,        0,                0, 0 },
    /* T_TOGGLE       */ { TF_SOLID_PLATFORM_MECH,                             BOUNCE_NORMAL, BOUNCE_SHORT,        0,        0,                0, 0 },
    /* T_FAN_L        */ { TILEF_MECHANIC,                                     0,             0,                   -3,       0,                0, 0 },
    /* T_FAN_R        */ { TILEF_MECHANIC,                                     0,             0,                   3,        0,                0, 0 },
    /* T_CONV_L       */ { TF_SOLID_PLATFORM_MECH,                             BOUNCE_NORMAL, BOUNCE_SHORT,        -FIX(1),  0,                0, 0 },
    /* T_CONV_R       */ { TF_SOLID_PLATFORM_MECH,                             BOUNCE_NORMAL, BOUNCE_SHORT,        FIX(1),   0,                0, 0 },
    /* T_WATER        */ { TILEF_DANGER,                                       0,             0,                   0,        0,                0, 0 },
    /* T_BUBBLE       */ { TILEF_MECHANIC,                                     0,             0,                   0,        EVENT_BUBBLE + 1, 1, 0 },
    /* T_MOVING       */ { TF_SOLID_PLATFORM_MECH,                             BOUNCE_NORMAL, BOUNCE_SHORT,        0,        0,                0, 0 },
    /* T_ROCK         */ { TILEF_SOLID | TILEF_MECHANIC | TILEF_POCKET_SOLID,  BOUNCE_NORMAL, BOUNCE_SHORT,        0,        0,                0, 1 },
    /* T_TOGGLE_OFF   */ { 0,                                                  0,             0,                   0,        0,                0, 0 },
    /* T_ARROW_D      */ { 0,                                                  0,             0,                   0,        0,                0, 0 },
    /* T_ARROW_R      */ { 0,                                                  0,             0,                   0,        0,                0, 0 },
    /* T_WALL         */ { TILEF_SOLID | TILEF_POCKET_SOLID,                   BOUNCE_NORMAL, BOUNCE_SHORT,        0,        0,                0, 0 },
    /* T_TITLE_SHADOW */ { 0,                                                  0,             0,                   0,        0,                0, 0 },
    /* T_TITLE_FILL   */ { 0,                                                  0,             0,                   0,        0,                0, 0 }
};

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */

uint8_t stage[SCREEN_TILES_H][SCREEN_TILES_W];
Enemy enemies[MAX_ENEMIES];
static SaveData save_data;

uint8_t game_mode = MODE_ADVENTURE;
uint8_t selected_level = 0;
uint8_t current_level = 0;
uint16_t panic_depth = 0;
uint8_t level_world = 0;
uint8_t coins = 0;
uint8_t key = 0;
uint8_t bubble = 0;
uint8_t switch_on = 1;
uint8_t switch_cooldown = 0;
uint16_t level_time = 0;
uint8_t enemy_count = 0;

uint8_t moving_active = 0;
uint8_t moving_x = 0;
uint8_t moving_y = 0;
uint8_t moving_w = 0;
int8_t moving_dir = 1;
uint8_t moving_tick = 0;

int16_t player_x = 0;
int16_t player_y = 0;
int16_t player_vx = 0;
int16_t player_vy = 0;
uint8_t stomping = 0;
uint8_t stomp_ready = 1;
uint8_t stomp_chain = 0;
uint8_t invuln = 0;

static uint8_t feedback_kind = FEEDBACK_NONE;
static uint8_t feedback_timer = 0;
static uint8_t feedback_expired_flag = 0;

/* ------------------------------------------------------------------ */
/* HUD feedback module                                                */
/*                                                                    */
/* Private state above; callers use the accessors below. The module   */
/* maps incoming GameEvents to the on-screen feedback banner          */
/* (SHIELD/STOMP/CRACK), counts down the display timer, and surfaces  */
/* an "expired this tick" flag so the HUD owner can mark itself dirty */
/* on the frame the banner clears.                                    */
/* ------------------------------------------------------------------ */

void hud_feedback_on_event(uint8_t event) {
    /* Only certain events register; STOMP/CRACK only register if no feedback
       is currently active (preserves prior gating). SHIELD always supersedes. */
    if (event == EVENT_SHIELD) {
        feedback_kind = FEEDBACK_SHIELD;
        feedback_timer = 36;
    } else if (event == EVENT_STOMP) {
        if (feedback_timer == 0) {
            feedback_kind = FEEDBACK_STOMP;
            feedback_timer = 18;
        }
    } else if (event == EVENT_CRACK) {
        if (feedback_timer == 0) {
            feedback_kind = FEEDBACK_CRACK;
            feedback_timer = 18;
        }
    }
    /* All other events: no-op for HUD feedback. */
}

void hud_feedback_tick(void) {
    feedback_expired_flag = 0;
    if (feedback_timer) {
        feedback_timer--;
        if (feedback_timer == 0u) {
            feedback_kind = FEEDBACK_NONE;
            feedback_expired_flag = 1;
        }
    }
}

uint8_t hud_feedback_active(void) { return feedback_timer != 0; }
uint8_t hud_feedback_kind(void)   { return feedback_kind; }
uint8_t hud_feedback_expired_this_tick(void) { return feedback_expired_flag; }

/* ------------------------------------------------------------------ */
/* Save / checksum                                                    */
/* ------------------------------------------------------------------ */

uint8_t checksum_save(const SaveData *data) {
    const uint8_t *raw;
    uint16_t i;
    uint8_t sum;
    raw = (const uint8_t *)data;
    sum = 0x5au;
    for (i = 0; i < (uint16_t)offsetof(SaveData, checksum); i++) {
        sum = (uint8_t)((sum << 1) ^ (sum >> 7) ^ raw[i]);
    }
    return sum;
}

void save_defaults(void) {
    uint8_t i;
    save_data.magic = SAVE_MAGIC;
    save_data.version = SAVE_VERSION;
    save_data.unlocked = 1;
    save_data.panic_best = 0;
    for (i = 0; i < NUM_ADVENTURE_LEVELS; i++) save_data.best_time[i] = 0xffffu;
    save_data.checksum = checksum_save(&save_data);
}

uint8_t save_unlocked_count(void) {
    return save_data.unlocked;
}

uint16_t save_best_time(uint8_t level) {
    if (level >= NUM_ADVENTURE_LEVELS) return 0xffffu;
    return save_data.best_time[level];
}

uint16_t save_panic_best(void) {
    return save_data.panic_best;
}

void save_record_clear(uint8_t level, uint16_t time) {
    if (level >= NUM_ADVENTURE_LEVELS) return;
    if (time < save_data.best_time[level]) {
        save_data.best_time[level] = time;
    }
    save_data.checksum = checksum_save(&save_data);
}

void save_unlock_through(uint8_t level) {
    if ((uint8_t)(level + 1u) >= NUM_ADVENTURE_LEVELS) return;
    if (save_data.unlocked < (uint8_t)(level + 2u)) {
        save_data.unlocked = (uint8_t)(level + 2u);
        save_data.checksum = checksum_save(&save_data);
    }
}

void save_record_panic(uint16_t depth) {
    if (depth > save_data.panic_best) save_data.panic_best = depth;
    save_data.checksum = checksum_save(&save_data);
}

uint8_t save_validate(void) {
    if (save_data.magic != SAVE_MAGIC) return 0;
    if (save_data.version != SAVE_VERSION) return 0;
    if (save_data.checksum != checksum_save(&save_data)) return 0;
    if (save_data.unlocked == 0u || save_data.unlocked > NUM_ADVENTURE_LEVELS) return 0;
    return 1;
}

void save_data_blob(uint8_t **out_ptr, uint16_t *out_len) {
    *out_ptr = (uint8_t *)&save_data;
    *out_len = (uint16_t)sizeof(SaveData);
}

/* ------------------------------------------------------------------ */
/* Tile queries                                                       */
/* ------------------------------------------------------------------ */

uint8_t stage_tile(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return T_SOLID;
    return stage[y][x];
}

uint8_t bg_for_tile(uint8_t t) {
    if (t == T_TOGGLE) return switch_on ? T_TOGGLE : T_TOGGLE_OFF;
    return t;
}

uint8_t tile_has_flag(uint8_t t, uint8_t flag) {
    if (t >= TILE_COUNT) return 0;
    return (uint8_t)((tile_table[t].flags & flag) != 0u);
}

uint8_t is_solid_tile(uint8_t t) {
    if (t == T_TOGGLE) return switch_on;
    return tile_has_flag(t, TILEF_SOLID);
}

uint8_t is_danger_tile(uint8_t t) {
    return tile_has_flag(t, TILEF_DANGER);
}

uint8_t tile_is_mechanic(uint8_t t) {
    return tile_has_flag(t, TILEF_MECHANIC);
}

uint8_t tile_is_platform(uint8_t t) {
    return tile_has_flag(t, TILEF_PLATFORM);
}

uint8_t tile_is_pocket_solid(uint8_t t) {
    return tile_has_flag(t, TILEF_POCKET_SOLID);
}

uint8_t tile_at_fixed(int16_t fx, int16_t fy) {
    int16_t px;
    int16_t py;
    int8_t tx;
    int8_t ty;
    px = fx >> FP_SHIFT;
    py = fy >> FP_SHIFT;
    tx = (int8_t)(px >> 3);
    ty = (int8_t)(py >> 3);
    if ((tx < 0) || (ty < 0) || (tx >= SCREEN_TILES_W) || (ty >= SCREEN_TILES_H)) return T_SOLID;
    return stage[(uint8_t)ty][(uint8_t)tx];
}

uint8_t is_stomp_breakable(uint8_t tile) {
    if (tile >= TILE_COUNT) return 0;
    return tile_table[tile].breakable_by_stomp;
}

/* ------------------------------------------------------------------ */
/* Stage builders                                                     */
/* ------------------------------------------------------------------ */

void add_platform(uint8_t x, uint8_t y, uint8_t w, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < w; i++) {
        if ((uint8_t)(x + i) < SCREEN_TILES_W) stage[y][x + i] = tile;
    }
}

void add_enemy(uint8_t tx, uint8_t ty, int8_t vx) {
    if (enemy_count >= MAX_ENEMIES) return;
    enemies[enemy_count].x = FIX(tx * TILE_PX);
    enemies[enemy_count].y = FIX(ty * TILE_PX);
    enemies[enemy_count].vx = vx;
    enemies[enemy_count].alive = 1;
    enemy_count++;
}

void set_spawn_px(uint8_t x, uint8_t y) {
    player_x = FIX(x);
    player_y = FIX(y);
    player_vx = 0;
    player_vy = FIX(-5);
    stomping = 0;
    stomp_ready = 1;
    stomp_chain = 0;
}

void add_coin_line(uint8_t x, uint8_t y, uint8_t w) {
    uint8_t i;
    for (i = 0; i < w; i++) {
        if (((uint8_t)(x + i) < SCREEN_TILES_W) && (y < SCREEN_TILES_H) && (stage[y][x + i] == T_EMPTY)) {
            stage[y][x + i] = T_COIN;
        }
    }
}

void add_hazard_line(uint8_t x, uint8_t y, uint8_t w, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < w; i++) {
        if (((uint8_t)(x + i) < SCREEN_TILES_W) && (y < SCREEN_TILES_H) && (stage[y][x + i] == T_EMPTY)) {
            stage[y][x + i] = tile;
        }
    }
}

void add_tile_if_empty(uint8_t x, uint8_t y, uint8_t tile) {
    if ((x < SCREEN_TILES_W) && (y < SCREEN_TILES_H) && (stage[y][x] == T_EMPTY)) stage[y][x] = tile;
}

void add_platform_if_empty(uint8_t x, uint8_t y, uint8_t w, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < w; i++) add_tile_if_empty((uint8_t)(x + i), y, tile);
}

void add_stomp_route(uint8_t x, uint8_t y, uint8_t w) {
    add_platform_if_empty(x, y, w, T_ROCK);
    if (y > 3u) add_coin_line(x, (uint8_t)(y - 1u), w);
}

void add_route_step(uint8_t x, uint8_t y, uint8_t w, uint8_t tile) {
    add_platform_if_empty(x, y, w, tile);
    if (y > 3u) add_coin_line(x, (uint8_t)(y - 1u), w);
}

void place_marker_down(uint8_t x, uint8_t y) {
    uint8_t my;
    if ((x >= SCREEN_TILES_W) || (y <= (uint8_t)(CEILING_Y + 1u))) return;
    my = (uint8_t)(y - 1u);
    if (stage[my][x] == T_EMPTY) stage[my][x] = T_ARROW_D;
}

void place_exit(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = T_EXIT;
    place_marker_down(x, y);
}

void place_key(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = T_KEY;
    place_marker_down(x, y);
}

void clear_if_danger(uint8_t x, uint8_t y) {
    if (is_danger_tile(stage[y][x])) stage[y][x] = T_EMPTY;
}

static void clear_if_platform(uint8_t x, uint8_t y) {
    if (stage[y][x] == T_SOLID ||
        stage[y][x] == T_CRACK ||
        stage[y][x] == T_TOGGLE ||
        stage[y][x] == T_CONV_L ||
        stage[y][x] == T_CONV_R ||
        stage[y][x] == T_MOVING ||
        stage[y][x] == T_ROCK) {
        stage[y][x] = T_EMPTY;
    }
}

void soften_exit(void) {
    clear_if_danger(15, 16);
    clear_if_danger(16, 16);
    clear_if_danger(17, 16);
    clear_if_danger(16, 15);
    clear_if_danger(17, 15);
    clear_if_danger(18, 15);
    place_marker_down(18, 16);
}

void unpin_goal_tiles(void) {
    uint8_t x;
    uint8_t y;
    for (y = (uint8_t)(CEILING_Y + 1u); y < 17u; y++) {
        for (x = 1; x < 19u; x++) {
            if (stage[y][x] == T_KEY) {
                clear_if_platform((uint8_t)(x - 1u), y);
                clear_if_platform((uint8_t)(x + 1u), y);
                clear_if_platform(x, (uint8_t)(y - 1u));
            }
        }
    }
}

void fill_one_tile_pockets(void) {
    uint8_t x;
    uint8_t y;
    uint8_t tile;
    for (y = (uint8_t)(CEILING_Y + 1u); y < 17u; y++) {
        for (x = 1; x < 19u; x++) {
            tile = stage[y][x];
            if ((tile == T_EMPTY || tile == T_COIN || tile == T_ARROW_D || tile == T_ARROW_R) &&
                tile_is_pocket_solid(stage[(uint8_t)(y + 1u)][x]) &&
                tile_is_pocket_solid(stage[y][(uint8_t)(x - 1u)]) &&
                tile_is_pocket_solid(stage[y][(uint8_t)(x + 1u)])) {
                stage[y][x] = T_SOLID;
            }
        }
    }
}

uint16_t rng_step(uint16_t seed) {
    return (uint16_t)(seed * 1109u + 1987u);
}

void reset_common(void) {
    uint8_t x;
    uint8_t y;
    for (y = 0; y < SCREEN_TILES_H; y++) {
        for (x = 0; x < SCREEN_TILES_W; x++) stage[y][x] = T_EMPTY;
    }
    for (x = 0; x < SCREEN_TILES_W; x++) stage[CEILING_Y][x] = T_WALL;
    for (x = 0; x < SCREEN_TILES_W; x++) stage[17][x] = T_WALL;
    for (y = CEILING_Y; y < SCREEN_TILES_H; y++) {
        stage[y][0] = T_WALL;
        stage[y][19] = T_WALL;
    }
    enemy_count = 0;
    moving_active = 0;
    moving_tick = 0;
    switch_on = 1;
    switch_cooldown = 0;
    coins = 0;
    key = 0;
    bubble = 0;
    invuln = 0;
    stomping = 0;
    stomp_ready = 1;
    stomp_chain = 0;
    feedback_kind = FEEDBACK_NONE;
    feedback_timer = 0;
    level_time = 0;
    set_spawn_px(18, 80);
}

void begin_room(uint8_t world) {
    level_world = world;
    reset_common();
    place_exit(18, 16);
}

void set_room_switch(uint8_t on) {
    switch_on = on;
}

void add_moving_platform(uint8_t x, uint8_t y, uint8_t w, int8_t dir) {
    moving_active = 1;
    moving_x = x;
    moving_y = y;
    moving_w = w;
    moving_dir = dir;
    add_platform(moving_x, moving_y, moving_w, T_MOVING);
}

void sprinkle_coins(uint8_t seed, uint8_t count) {
    uint8_t i;
    uint8_t x;
    uint8_t y;
    for (i = 0; i < count; i++) {
        x = (uint8_t)(2u + ((uint8_t)(seed + i * 5u) % 16u));
        y = (uint8_t)(4u + ((uint8_t)(seed + i * 3u) % 10u));
        if (stage[y][x] == T_EMPTY) stage[y][x] = T_COIN;
    }
}

static uint8_t adventure_accent_tile(uint8_t world, uint8_t local) {
    if (world == 1u) return T_TOGGLE;
    if (world == 2u) return (local & 1u) ? T_CRACK : T_SOLID;
    if (world == 3u) return (local & 1u) ? T_CONV_R : T_CONV_L;
    if (world >= 4u) return (local & 1u) ? T_CRACK : T_CONV_R;
    return (local & 1u) ? T_CRACK : T_SOLID;
}

static void add_short_hop_gate(uint8_t x, uint8_t floor_y, uint8_t w, uint8_t ceiling_tile) {
    add_platform_if_empty(x, floor_y, w, T_SOLID);
    if (floor_y > 5u) add_platform_if_empty(x, (uint8_t)(floor_y - 4u), w, ceiling_tile);
    if (floor_y > 3u) add_coin_line(x, (uint8_t)(floor_y - 1u), w);
}

static void add_opening_upper_route(uint8_t lane, uint8_t accent) {
    if (lane == 0u) {
        add_platform_if_empty(12, 7, 3, accent);
        add_coin_line(12, 6, 2);
    } else if (lane == 1u) {
        add_platform_if_empty(5, 6, 3, accent);
        add_coin_line(5, 5, 2);
    } else if (lane == 2u) {
        add_platform_if_empty(9, 8, 3, accent);
        add_coin_line(9, 7, 2);
    } else {
        add_platform_if_empty(14, 6, 3, accent);
        add_coin_line(14, 5, 2);
    }
}

void enrich_adventure_room(uint8_t world, uint8_t local) {
    uint8_t lane;
    uint8_t tier;
    uint8_t accent;
    uint8_t danger;

    lane = (uint8_t)(local & 3u);
    tier = (uint8_t)(local >> 2u);
    accent = adventure_accent_tile(world, local);
    danger = (world == 2u || world >= 4u) ? ((local & 1u) ? T_WATER : T_SPIKE) : T_SPIKE;

    if ((world == 0u) && (local < 8u)) {
        return;
    }

    add_opening_upper_route(lane, accent);

    if (tier >= 1u) {
        add_stomp_route((uint8_t)(4u + lane), (uint8_t)(9u + (lane & 1u)), 2);
    }

    if (tier >= 2u) {
        add_short_hop_gate((uint8_t)(8u + (lane & 1u)), 13, 3, accent);
        add_hazard_line((uint8_t)(7u + lane), 16, 2, danger);
    }

    if (tier >= 3u) {
        add_platform_if_empty((uint8_t)(4u + ((lane * 3u) & 7u)), (uint8_t)(5u + (lane & 1u)), 3, accent);
        add_coin_line((uint8_t)(4u + ((lane * 3u) & 7u)), (uint8_t)(4u + (lane & 1u)), 2);
        add_enemy((uint8_t)(11u + lane), 16, (local & 1u) ? -1 : 1);
    }

    if (world == 1u) {
        if (tier >= 2u) stage[11][15u - lane] = T_SWITCH;
        if (tier >= 3u) add_platform_if_empty((uint8_t)(9u + lane), 6, 3, T_TOGGLE);
    } else if (world == 2u) {
        if (tier >= 1u) add_tile_if_empty((uint8_t)(3u + lane), 10, T_BUBBLE);
        if (tier >= 2u) add_tile_if_empty((uint8_t)(15u - lane), 8, (local & 1u) ? T_FAN_L : T_FAN_R);
    } else if (world == 3u) {
        if (tier >= 1u) add_tile_if_empty((uint8_t)(5u + lane), 13, T_SPRING);
        if (tier >= 2u) add_platform_if_empty((uint8_t)(2u + lane), 6, 3, accent);
    } else if (world >= 4u) {
        if (tier >= 1u) add_tile_if_empty((uint8_t)(3u + lane), 10, T_BUBBLE);
        if (tier >= 2u) add_tile_if_empty((uint8_t)(15u - lane), 8, (local & 1u) ? T_FAN_L : T_FAN_R);
        if (tier >= 3u) add_stomp_route((uint8_t)(15u - lane), 12, 2);
    }
}

void build_tutorial_room(uint8_t local) {
    if (local == 0u) {
        add_platform(2, 14, 4, T_SOLID);
        add_platform(7, 12, 3, T_SOLID);
        add_platform(12, 10, 4, T_SOLID);
        add_platform(15, 14, 2, T_SOLID);
        add_hazard_line(8, 16, 2, T_SPIKE);
        place_key(13, 9);
        add_coin_line(7, 11, 3);
        add_coin_line(12, 9, 3);
        stage[13][6] = T_ARROW_R;
    } else if (local == 1u) {
        add_platform(2, 15, 4, T_SOLID);
        add_short_hop_gate(7, 14, 4, T_CRACK);
        add_platform(12, 12, 3, T_CRACK);
        add_platform(14, 8, 3, T_SOLID);
        add_hazard_line(6, 16, 3, T_SPIKE);
        place_key(9, 13);
        add_coin_line(12, 11, 2);
        stage[13][6] = T_ARROW_R;
    } else if (local == 2u) {
        add_platform(2, 13, 4, T_SOLID);
        add_platform(6, 9, 3, T_SOLID);
        add_platform(10, 12, 3, T_SOLID);
        add_platform(14, 8, 3, T_SOLID);
        add_platform(15, 14, 2, T_SOLID);
        add_hazard_line(7, 16, 4, T_SPIKE);
        place_key(7, 8);
        add_coin_line(10, 11, 3);
        add_coin_line(14, 7, 2);
        stage[16][17] = T_ARROW_R;
    } else if (local == 3u) {
        add_platform(2, 14, 4, T_SOLID);
        add_platform(7, 11, 4, T_SOLID);
        add_platform(12, 14, 5, T_SOLID);
        add_platform(13, 9, 3, T_SOLID);
        add_hazard_line(6, 16, 6, T_SPIKE);
        place_key(8, 10);
        add_coin_line(13, 13, 3);
        add_coin_line(13, 8, 2);
    } else if (local == 4u) {
        add_platform(2, 14, 4, T_SOLID);
        stage[14][6] = T_SPRING;
        add_platform(8, 11, 5, T_SOLID);
        add_platform(10, 7, 4, T_SOLID);
        add_platform(14, 13, 3, T_SOLID);
        add_hazard_line(8, 16, 3, T_SPIKE);
        place_key(11, 10);
        add_coin_line(8, 10, 3);
        add_coin_line(14, 12, 2);
    } else if (local == 5u) {
        add_platform(2, 14, 4, T_SOLID);
        add_stomp_route(6, 12, 3);
        add_platform(10, 9, 3, T_CRACK);
        add_platform(14, 14, 3, T_SOLID);
        add_hazard_line(8, 16, 4, T_SPIKE);
        place_key(11, 8);
        add_coin_line(14, 13, 2);
        stage[12][12] = T_ARROW_D;
    } else if (local == 6u) {
        add_platform(2, 14, 4, T_SOLID);
        add_platform(5, 10, 3, T_SOLID);
        add_platform(10, 12, 3, T_SOLID);
        add_platform(14, 14, 3, T_SOLID);
        add_hazard_line(8, 16, 4, T_SPIKE);
        place_key(6, 9);
        add_coin_line(10, 11, 3);
        add_coin_line(14, 13, 2);
        add_enemy(13, 16, -1);
    } else {
        set_room_switch(0);
        add_platform(2, 14, 4, T_SOLID);
        stage[14][4] = T_SWITCH;
        add_platform(6, 10, 3, T_TOGGLE);
        add_platform(10, 13, 3, T_SOLID);
        add_platform(13, 8, 4, T_TOGGLE);
        add_platform(15, 14, 2, T_SOLID);
        add_hazard_line(8, 16, 4, T_SPIKE);
        place_key(14, 7);
        add_coin_line(10, 12, 3);
        add_coin_line(13, 7, 3);
    }
}

void build_intro_room(uint8_t local) {
    uint8_t route;
    route = (uint8_t)(local & 7u);
    if (route == 0u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(9, 12, 4, T_SOLID);
        add_platform(14, 8, 4, T_SOLID);
        add_hazard_line(7, 16, 3, T_SPIKE);
        place_key(15, 7);
        add_coin_line(9, 11, 3);
    } else if (route == 1u) {
        add_platform(2, 15, 4, T_SOLID);
        stage[15][7] = T_SPRING;
        add_platform(11, 8, 5, T_SOLID);
        add_hazard_line(9, 16, 4, T_SPIKE);
        place_key(13, 7);
    } else if (route == 2u) {
        add_platform(2, 13, 5, T_SOLID);
        add_platform(8, 10, 5, T_CRACK);
        add_platform(12, 8, 4, T_SOLID);
        add_platform(14, 13, 3, T_SOLID);
        place_key(13, 7);
        add_coin_line(8, 9, 3);
    } else if (route == 3u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(8, 11, 4, T_SOLID);
        add_platform(12, 8, 3, T_SOLID);
        add_platform(13, 14, 4, T_SOLID);
        place_key(9, 10);
        add_coin_line(12, 7, 2);
        add_enemy(12, 16, -1);
    } else if (route == 4u) {
        set_room_switch(0);
        add_platform(2, 14, 4, T_SOLID);
        stage[14][4] = T_SWITCH;
        add_platform(7, 11, 4, T_TOGGLE);
        add_platform(13, 9, 4, T_SOLID);
        place_key(15, 8);
    } else if (route == 5u) {
        add_platform(2, 15, 4, T_CONV_R);
        add_platform(8, 12, 4, T_SOLID);
        add_platform(13, 10, 4, T_SOLID);
        stage[13][6] = T_SPIKE;
        place_key(14, 9);
        add_coin_line(8, 11, 4);
    } else if (route == 6u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(9, 12, 5, T_CRACK);
        stage[15][15] = T_SPRING;
        place_key(11, 11);
        add_hazard_line(7, 16, 3, T_SPIKE);
    } else {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(8, 10, 4, T_SOLID);
        add_platform(13, 13, 4, T_SOLID);
        stage[9][14] = T_BUBBLE;
        add_hazard_line(9, 16, 6, T_WATER);
        place_key(9, 9);
        add_enemy(14, 16, -1);
    }
}

void build_switch_room(uint8_t local) {
    uint8_t route;
    uint8_t band;
    route = (uint8_t)(local & 3u);
    band = (uint8_t)(local >> 2u);
    set_room_switch(0);
    add_platform(2, 14, 4, T_SOLID);
    stage[14][4] = T_SWITCH;
    if (route == 0u) {
        add_platform(7, 12, 4, T_TOGGLE);
        add_platform(12, 10, 3, T_TOGGLE);
        add_platform(15, 13, 2, T_SOLID);
        add_hazard_line(8, 16, 3, T_SPIKE);
        place_key(13, 9);
        add_coin_line(7, 11, 3);
    } else if (route == 1u) {
        add_platform(7, 11, 3, T_TOGGLE);
        add_platform(11, 9, 4, T_SOLID);
        add_platform(13, 13, 4, T_TOGGLE);
        add_hazard_line(7, 16, 5, T_SPIKE);
        place_key(12, 8);
        add_coin_line(11, 8, 3);
    } else if (route == 2u) {
        add_platform(6, 13, 4, T_TOGGLE);
        add_platform(10, 10, 3, T_TOGGLE);
        add_platform(14, 12, 3, T_SOLID);
        add_platform(9, 7, 4, T_SOLID);
        stage[15][15] = T_SPRING;
        add_hazard_line(8, 16, 4, T_SPIKE);
        place_key(11, 9);
        add_coin_line(6, 12, 3);
    } else {
        add_platform(6, 12, 3, T_TOGGLE);
        add_platform(10, 10, 3, T_SOLID);
        add_platform(13, 8, 4, T_TOGGLE);
        add_platform(14, 13, 3, T_SOLID);
        add_hazard_line(6, 16, 6, T_SPIKE);
        place_key(14, 7);
        add_enemy(10, 16, 1);
    }
    if (band == 1u) {
        add_stomp_route((uint8_t)(12u - route), (uint8_t)(12u - (route & 1u)), 2);
        add_platform_if_empty((uint8_t)(4u + route), 8, 2, T_TOGGLE);
    } else if (band == 2u) {
        add_short_hop_gate((uint8_t)(5u + route), 13, 2, T_TOGGLE);
        add_tile_if_empty((uint8_t)(15u - route), 10, T_SWITCH);
    } else if (band >= 3u) {
        add_platform_if_empty((uint8_t)(4u + ((route * 3u) & 7u)), (uint8_t)(5u + (route & 1u)), 3, T_TOGGLE);
        add_stomp_route((uint8_t)(10u + route), 11, 2);
        add_tile_if_empty((uint8_t)(5u + route), 9, T_SWITCH);
    }
    if (local >= 8u) {
        stage[11][15u - (local & 3u)] = T_SWITCH;
        add_enemy((uint8_t)(8u + (local & 3u)), 16, (local & 1u) ? -1 : 1);
    }
}

void build_water_room(uint8_t local) {
    uint8_t route;
    uint8_t band;
    route = (uint8_t)(local & 3u);
    band = (uint8_t)(local >> 2u);
    add_platform(2, 14, 4, T_SOLID);
    if (route == 0u) {
        add_hazard_line(6, 16, 8, T_WATER);
        add_platform(8, 12, 4, T_SOLID);
        stage[12][3] = T_BUBBLE;
        place_key(9, 11);
        add_coin_line(11, 11, 3);
    } else if (route == 1u) {
        add_hazard_line(5, 16, 5, T_WATER);
        add_hazard_line(13, 16, 3, T_WATER);
        stage[10][5] = T_FAN_R;
        add_platform(10, 11, 4, T_SOLID);
        place_key(12, 10);
    } else if (route == 2u) {
        add_hazard_line(6, 16, 9, T_WATER);
        stage[13][4] = T_BUBBLE;
        stage[9][15] = T_FAN_L;
        add_platform(5, 12, 3, T_SOLID);
        add_platform(8, 9, 4, T_SOLID);
        place_key(9, 8);
        add_enemy(13, 16, -1);
    } else {
        add_hazard_line(4, 16, 4, T_WATER);
        add_hazard_line(11, 16, 5, T_WATER);
        stage[15][6] = T_SPRING;
        stage[8][14] = (local & 4u) ? T_FAN_L : T_FAN_R;
        add_platform(10, 8, 5, T_SOLID);
        stage[12][3] = T_BUBBLE;
        place_key(12, 7);
    }
    if (band == 1u) {
        add_platform_if_empty((uint8_t)(6u + route), (uint8_t)(7u + (route & 1u)), 3, (route & 1u) ? T_CRACK : T_SOLID);
        add_coin_line((uint8_t)(6u + route), (uint8_t)(6u + (route & 1u)), 2);
        add_tile_if_empty((uint8_t)(13u - route), 11, T_BUBBLE);
    } else if (band == 2u) {
        add_short_hop_gate((uint8_t)(5u + route), 13, 2, (route & 1u) ? T_CRACK : T_SOLID);
        add_tile_if_empty((uint8_t)(15u - route), 8, (route & 1u) ? T_FAN_L : T_FAN_R);
    } else if (band >= 3u) {
        add_platform_if_empty((uint8_t)(4u + route), (uint8_t)(6u + (route & 1u)), 3, (route & 1u) ? T_CRACK : T_SOLID);
        add_coin_line((uint8_t)(4u + route), (uint8_t)(5u + (route & 1u)), 2);
        add_tile_if_empty((uint8_t)(6u + route), 11, T_BUBBLE);
        add_tile_if_empty((uint8_t)(15u - route), 6, (route & 1u) ? T_FAN_L : T_FAN_R);
    }
    if (local >= 8u) add_hazard_line((uint8_t)(7u + (local & 3u)), 15, 2, T_SPIKE);
}

void build_motion_room(uint8_t local) {
    uint8_t route;
    uint8_t band;
    route = (uint8_t)(local & 3u);
    band = (uint8_t)(local >> 2u);
    add_platform(2, 14, 4, (local & 1u) ? T_CONV_R : T_CONV_L);
    if (route == 0u) {
        add_platform(8, 12, 5, T_CONV_R);
        add_platform(14, 9, 4, T_SOLID);
        place_key(15, 8);
    } else if (route == 1u) {
        add_platform(7, 10, 4, T_CONV_L);
        add_platform(12, 13, 5, T_CONV_R);
        add_hazard_line(6, 16, 4, T_SPIKE);
        place_key(8, 9);
    } else if (route == 2u) {
        add_platform(5, 12, 4, T_SOLID);
        add_platform(13, 8, 4, T_CONV_L);
        stage[14][10] = T_FAN_R;
        place_key(14, 7);
        add_coin_line(5, 11, 3);
    } else {
        add_platform(4, 11, 3, T_CRACK);
        add_platform(12, 10, 4, T_CONV_R);
        stage[13][15] = T_SPRING;
        add_hazard_line(8, 16, 4, T_SPIKE);
        place_key(13, 9);
    }
    if (local >= 4u) {
        add_moving_platform((uint8_t)(6u + (local & 3u)),
                            (uint8_t)((local & 4u) ? 7u : 8u),
                            3,
                            (local & 1u) ? 1 : -1);
    }
    if (band == 1u) {
        add_platform_if_empty((uint8_t)(4u + route), 7, 3, (route & 1u) ? T_CONV_R : T_CONV_L);
        add_coin_line((uint8_t)(4u + route), 6, 2);
    } else if (band == 2u) {
        add_short_hop_gate((uint8_t)(7u + (route & 1u)), 13, 3, (route & 1u) ? T_CONV_R : T_CONV_L);
        add_tile_if_empty((uint8_t)(14u - route), 8, (route & 1u) ? T_FAN_L : T_FAN_R);
    } else if (band >= 3u) {
        add_platform_if_empty((uint8_t)(3u + route), (uint8_t)(5u + (route & 1u)), 3, (route & 1u) ? T_CONV_R : T_CONV_L);
        add_stomp_route((uint8_t)(12u - (route & 1u)), 11, 2);
        add_tile_if_empty((uint8_t)(15u - route), 7, (route & 1u) ? T_FAN_L : T_FAN_R);
    }
    if (local >= 10u) add_enemy((uint8_t)(10u + (local & 3u)), 16, (local & 1u) ? -1 : 1);
}

void build_mixed_room(uint8_t local) {
    uint8_t route;
    uint8_t band;
    route = (uint8_t)(local & 7u);
    band = (uint8_t)(local >> 2u);
    set_room_switch(0);
    add_platform(2, 14, 4, T_SOLID);
    stage[14][4] = T_SWITCH;
    if (route == 0u) {
        add_platform(7, 12, 4, T_TOGGLE);
        add_platform(12, 9, 4, T_CRACK);
        stage[11][5] = T_BUBBLE;
        add_hazard_line(8, 16, 5, T_SPIKE);
        place_key(13, 8);
    } else if (route == 1u) {
        add_hazard_line(5, 16, 8, T_WATER);
        stage[12][3] = T_BUBBLE;
        stage[10][14] = T_FAN_L;
        add_platform(6, 13, 2, T_TOGGLE);
        add_platform(9, 10, 4, T_CONV_R);
        place_key(10, 9);
        add_enemy(14, 16, -1);
    } else if (route == 2u) {
        stage[14][6] = T_SPRING;
        stage[11][5] = T_BUBBLE;
        add_platform(9, 9, 5, T_TOGGLE);
        stage[13][4] = T_SWITCH;
        add_hazard_line(7, 16, 4, T_SPIKE);
        place_key(11, 8);
    } else if (route == 3u) {
        add_platform(5, 12, 4, T_CRACK);
        add_platform(9, 13, 2, T_TOGGLE);
        add_platform(12, 10, 5, T_CONV_L);
        add_hazard_line(7, 16, 3, T_WATER);
        place_key(14, 9);
        add_enemy(9, 16, 1);
    } else if (route == 4u) {
        add_hazard_line(6, 16, 6, T_SPIKE);
        stage[13][3] = T_BUBBLE;
        add_moving_platform(8, 10, 3, 1);
        add_platform(12, 13, 3, T_TOGGLE);
        place_key(9, 9);
    } else if (route == 5u) {
        add_platform(5, 12, 2, T_TOGGLE);
        add_platform(7, 13, 4, T_CONV_R);
        stage[11][15] = T_FAN_L;
        add_platform(12, 8, 4, T_CRACK);
        add_hazard_line(5, 16, 4, T_WATER);
        place_key(14, 7);
        add_enemy(13, 16, -1);
    } else if (route == 6u) {
        stage[14][4] = T_SWITCH;
        add_platform(7, 12, 3, T_TOGGLE);
        add_platform(11, 9, 4, T_TOGGLE);
        stage[15][14] = T_SPRING;
        add_hazard_line(7, 16, 5, T_WATER);
        place_key(12, 8);
    } else {
        add_platform(6, 12, 4, T_CRACK);
        add_platform(10, 9, 2, T_TOGGLE);
        add_platform(12, 11, 5, T_CONV_R);
        add_hazard_line(5, 16, 4, T_SPIKE);
        add_hazard_line(12, 16, 4, T_WATER);
        stage[10][4] = T_BUBBLE;
        place_key(13, 10);
        add_enemy(8, 16, 1);
        add_enemy(14, 16, -1);
    }
    if (band == 2u) {
        add_short_hop_gate((uint8_t)(5u + (route & 3u)), 13, 2, T_TOGGLE);
        add_tile_if_empty((uint8_t)(15u - (route & 3u)), 8, (route & 1u) ? T_FAN_L : T_FAN_R);
    } else if (band >= 3u) {
        add_platform_if_empty((uint8_t)(4u + ((route * 2u) & 7u)), (uint8_t)(5u + (route & 1u)), 3,
                              (route & 1u) ? T_CRACK : T_TOGGLE);
        add_stomp_route((uint8_t)(11u + (route & 1u)), 11, 2);
        add_tile_if_empty((uint8_t)(15u - (route & 3u)), 7, (route & 1u) ? T_FAN_L : T_FAN_R);
    }
}

void generate_adventure(uint8_t level) {
    uint8_t local;
    uint8_t world;
    world = (uint8_t)(level >> 4);
    local = (uint8_t)(level & 15u);
    begin_room(world);
    if ((world == 0u) && (local < 8u)) {
        build_tutorial_room(local);
    } else if (world == 0u) {
        build_intro_room(local);
    } else if (world == 1u) {
        build_switch_room(local);
    } else if (world == 2u) {
        build_water_room(local);
    } else if (world == 3u) {
        build_motion_room(local);
    } else {
        build_mixed_room(local);
    }
    enrich_adventure_room(world, local);
    unpin_goal_tiles();
    fill_one_tile_pockets();
    soften_exit();
}

void generate_panic(uint16_t depth) {
    uint16_t seed;
    uint8_t i;
    uint8_t danger;
    uint8_t x;
    uint8_t y;
    uint8_t key_x;
    uint8_t key_y;
    uint8_t support_x;
    seed = (uint16_t)(depth * 97u + 31u);
    danger = (uint8_t)(depth / 10u);
    if (danger > 5u) danger = 5u;
    begin_room(4);
    set_spawn_px(18, 92);
    set_room_switch((uint8_t)((depth & 1u) == 0u));
    add_platform(2, 14, 4, T_SOLID);
    add_platform(13, 14, 3, (depth & 1u) ? T_CONV_L : T_CONV_R);

    for (i = 0; i < 4u; i++) {
        seed = rng_step(seed);
        x = (uint8_t)(5u + (seed % 10u));
        seed = rng_step(seed);
        y = (uint8_t)(6u + (seed % 8u));
        add_platform(x, y, (uint8_t)(3u + (seed % 4u)), (i == 2u) ? T_TOGGLE : ((depth & 3u) == i) ? T_CRACK : T_SOLID);
    }

    key_x = (uint8_t)(5u + (depth % 10u));
    key_y = (uint8_t)(4u + (depth & 1u));
    support_x = (uint8_t)(key_x - 1u);
    place_key(key_x, key_y);
    add_platform_if_empty(support_x, (uint8_t)(key_y + 1u), 3, (depth & 2u) ? T_CRACK : T_SOLID);
    stage[13][4] = T_SWITCH;
    stage[8][15] = (depth & 1u) ? T_FAN_L : T_FAN_R;
    stage[12][6] = T_BUBBLE;
    if (danger >= 1u) add_stomp_route((uint8_t)(9u + (depth & 3u)), 10, 2);
    if (danger >= 3u) add_platform_if_empty((uint8_t)(4u + (depth & 3u)), 7, 2, T_CONV_R);
    sprinkle_coins((uint8_t)seed, (uint8_t)(4u + danger));

    for (i = 0; i < (uint8_t)(3u + danger); i++) {
        seed = rng_step(seed);
        x = (uint8_t)(6u + (seed % 10u));
        y = (uint8_t)(16u - (i & 1u));
        if (x > 14u) x = 14u;
        if (stage[y][x] == T_EMPTY) stage[y][x] = ((i + depth) & 1u) ? T_SPIKE : T_WATER;
    }
    for (i = 0; i < (uint8_t)(1u + (danger / 2u)); i++) {
        add_enemy((uint8_t)(7u + ((seed + i * 5u) % 8u)), 16, (i & 1u) ? 1 : -1);
    }
    if (danger >= 2u) {
        add_moving_platform((uint8_t)(7u + (depth & 3u)), 7, 3, 1);
    }
    unpin_goal_tiles();
    fill_one_tile_pockets();
    soften_exit();
}
