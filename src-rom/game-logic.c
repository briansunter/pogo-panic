#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "game-logic.h"

/* ------------------------------------------------------------------ */
/* Tile flag table                                                    */
/* ------------------------------------------------------------------ */

static const uint8_t tile_flags[] = {
    0,              /* empty */
    TILEF_SOLID,    /* solid */
    TILEF_SOLID,    /* crack */
    TILEF_SOLID,    /* spring */
    TILEF_DANGER,   /* spikes */
    0,              /* coin */
    0,              /* battery */
    0,              /* exit */
    TILEF_SOLID,    /* switch */
    TILEF_SOLID,    /* toggle */
    0,              /* fan left */
    0,              /* fan right */
    TILEF_SOLID,    /* conveyor left */
    TILEF_SOLID,    /* conveyor right */
    TILEF_DANGER,   /* water */
    0,              /* bubble */
    TILEF_SOLID,    /* moving */
    TILEF_SOLID,    /* rock */
    0,              /* toggle off */
    0,              /* arrow down */
    0,              /* arrow right */
    TILEF_SOLID,    /* wall */
    0,              /* title shadow */
    0               /* title fill */
};

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */

uint8_t stage[SCREEN_TILES_H][SCREEN_TILES_W];
Enemy enemies[MAX_ENEMIES];
SaveData save_data;

uint8_t game_mode = MODE_ADVENTURE;
uint8_t selected_level = 0;
uint8_t current_level = 0;
uint16_t panic_depth = 0;
uint8_t level_world = 0;
uint8_t coins = 0;
uint8_t battery = 0;
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

uint8_t feedback_kind = FEEDBACK_NONE;
uint8_t feedback_timer = 0;

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
    return (uint8_t)((tile_flags[t] & flag) != 0u);
}

uint8_t is_solid_tile(uint8_t t) {
    if (t == T_TOGGLE) return switch_on;
    return tile_has_flag(t, TILEF_SOLID);
}

uint8_t is_danger_tile(uint8_t t) {
    return tile_has_flag(t, TILEF_DANGER);
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
    return (uint8_t)((tile == T_CRACK) || (tile == T_ROCK));
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

void add_column(uint8_t x, uint8_t y, uint8_t h, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < h; i++) {
        if (((uint8_t)(y + i) < SCREEN_TILES_H) && (x < SCREEN_TILES_W)) stage[y + i][x] = tile;
    }
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

void place_battery(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = T_BATTERY;
    place_marker_down(x, y);
}

void clear_if_danger(uint8_t x, uint8_t y) {
    if (is_danger_tile(stage[y][x])) stage[y][x] = T_EMPTY;
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
    battery = 0;
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

void enrich_adventure_room(uint8_t world, uint8_t local) {
    uint8_t lane;
    uint8_t high_x;
    uint8_t lower_x;
    uint8_t high_y;
    uint8_t lower_y;
    uint8_t high_tile;
    uint8_t lower_tile;
    uint8_t rock_x;
    uint8_t rock_y;

    if ((world == 0u) && (local < 8u)) return;

    lane = (uint8_t)(local & 3u);
    high_x = (uint8_t)(3u + (local & 7u));
    lower_x = (uint8_t)(11u + lane);
    high_y = (uint8_t)(6u + (local & 1u));
    lower_y = (uint8_t)(8u + ((local >> 1u) & 1u));
    rock_x = (uint8_t)(2u + ((local + world) & 3u));
    rock_y = (uint8_t)(9u + ((local >> 2u) & 1u));
    high_tile = T_SOLID;
    lower_tile = T_SOLID;

    if (world == 0u) {
        high_tile = T_CRACK;
    } else if (world == 3u) {
        high_tile = (local & 1u) ? T_CONV_R : T_CONV_L;
        lower_tile = (local & 2u) ? T_CONV_L : T_CONV_R;
    } else if (world >= 4u) {
        high_tile = (local & 1u) ? T_CRACK : T_CONV_R;
        lower_tile = (local & 2u) ? T_TOGGLE : T_CRACK;
    }

    add_tile_if_empty((uint8_t)(4u + lane), 5, T_COIN);
    add_tile_if_empty((uint8_t)(8u + lane), 4, T_COIN);
    add_tile_if_empty((uint8_t)(13u + lane), 5, T_COIN);
    add_platform_if_empty(high_x, high_y, 3, high_tile);
    add_coin_line((uint8_t)(high_x + 1u), (uint8_t)(high_y - 1u), 2);
    add_platform_if_empty((uint8_t)(lower_x - 1u), lower_y, 3, lower_tile);

    if (world == 0u) {
        add_tile_if_empty(6, 15, T_SPRING);
    } else if (world == 1u) {
        add_platform_if_empty((uint8_t)(5u + lane), 10, 2, T_TOGGLE);
    } else if (world == 2u) {
        add_tile_if_empty((uint8_t)(3u + lane), 10, T_BUBBLE);
        add_tile_if_empty((uint8_t)(15u - lane), 7, (local & 1u) ? T_FAN_L : T_FAN_R);
    } else if (world == 3u) {
        add_tile_if_empty((uint8_t)(5u + lane), 13, T_SPRING);
    } else {
        add_tile_if_empty((uint8_t)(3u + lane), 10, T_BUBBLE);
        add_tile_if_empty((uint8_t)(15u - lane), 7, (local & 1u) ? T_FAN_L : T_FAN_R);
    }

    if (local >= 4u) add_stomp_route(rock_x, rock_y, (uint8_t)(2u + (local & 1u)));
    if (local >= 12u) add_enemy((uint8_t)(high_x + 1u), (uint8_t)(high_y - 1u), (local & 1u) ? 1 : -1);

    if (world == 1u) {
        add_platform_if_empty((uint8_t)(3u + lane), 6, 3, T_TOGGLE);
        add_coin_line((uint8_t)(3u + lane), 5, 3);
        if (local >= 8u) add_tile_if_empty((uint8_t)(15u - lane), 11, T_SWITCH);
    } else if (world == 2u) {
        add_tile_if_empty((uint8_t)(7u + lane), 6, T_BUBBLE);
        add_tile_if_empty((uint8_t)(11u - lane), 13, (local & 1u) ? T_FAN_L : T_FAN_R);
        if (local >= 8u) add_hazard_line((uint8_t)(4u + lane), 15, 2, T_WATER);
    } else if (world == 3u) {
        add_platform_if_empty((uint8_t)(2u + lane), 6, 3, (local & 1u) ? T_CONV_R : T_CONV_L);
        add_coin_line((uint8_t)(2u + lane), 5, 3);
        if (local >= 8u) add_stomp_route((uint8_t)(14u - lane), 11, 2);
    } else if (world >= 4u) {
        add_stomp_route((uint8_t)(15u - lane), 12, 2);
        add_tile_if_empty((uint8_t)(6u + lane), 7, T_BUBBLE);
        if (local >= 8u) add_hazard_line((uint8_t)(3u + lane), 15, 2, (local & 1u) ? T_SPIKE : T_WATER);
    }
}

void build_tutorial_room(uint8_t local) {
    if (local == 0u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(8, 12, 4, T_SOLID);
        add_platform(13, 10, 5, T_SOLID);
        place_battery(4, 13);
        add_coin_line(8, 11, 4);
        stage[13][7] = T_ARROW_R;
    } else if (local == 1u) {
        add_platform(2, 14, 4, T_SOLID);
        add_column(5, 12, 5, T_SOLID);
        add_column(13, 12, 5, T_SOLID);
        add_platform(6, 12, 7, T_CRACK);
        place_battery(9, 15);
        add_coin_line(7, 10, 5);
        stage[11][9] = T_ARROW_D;
    } else if (local == 2u) {
        add_platform(2, 13, 6, T_SOLID);
        add_platform(11, 11, 5, T_SOLID);
        place_battery(3, 12);
        add_coin_line(12, 10, 3);
        stage[16][17] = T_ARROW_R;
    } else if (local == 3u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(12, 14, 5, T_SOLID);
        add_platform(7, 10, 4, T_SOLID);
        add_hazard_line(7, 16, 5, T_SPIKE);
        place_battery(8, 9);
        add_coin_line(13, 13, 3);
    } else if (local == 4u) {
        add_platform(2, 14, 4, T_SOLID);
        stage[14][6] = T_SPRING;
        add_platform(11, 7, 5, T_SOLID);
        place_battery(14, 6);
        add_coin_line(12, 8, 3);
    } else if (local == 5u) {
        add_platform(3, 13, 5, T_CRACK);
        add_platform(10, 11, 5, T_CRACK);
        add_platform(4, 16, 4, T_SOLID);
        place_battery(12, 10);
        add_coin_line(4, 12, 3);
        stage[12][12] = T_ARROW_D;
    } else if (local == 6u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(9, 12, 4, T_SOLID);
        place_battery(14, 15);
        add_coin_line(10, 11, 3);
        add_enemy(9, 16, 1);
    } else {
        set_room_switch(0);
        add_platform(2, 14, 4, T_SOLID);
        stage[15][4] = T_SWITCH;
        add_platform(8, 12, 6, T_TOGGLE);
        place_battery(13, 11);
        add_coin_line(9, 11, 3);
    }
}

void build_intro_room(uint8_t local) {
    uint8_t route;
    route = (uint8_t)(local & 7u);
    if (route == 0u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(9, 12, 4, T_SOLID);
        add_platform(14, 9, 4, T_SOLID);
        add_hazard_line(7, 16, 3, T_SPIKE);
        place_battery(15, 8);
        add_coin_line(9, 11, 3);
    } else if (route == 1u) {
        add_platform(2, 15, 4, T_SOLID);
        stage[15][7] = T_SPRING;
        add_platform(11, 8, 5, T_SOLID);
        add_hazard_line(9, 16, 4, T_SPIKE);
        place_battery(13, 7);
    } else if (route == 2u) {
        add_platform(2, 13, 5, T_SOLID);
        add_platform(8, 10, 5, T_CRACK);
        add_platform(13, 13, 4, T_SOLID);
        place_battery(10, 9);
        add_coin_line(13, 12, 3);
    } else if (route == 3u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(8, 11, 4, T_SOLID);
        add_platform(13, 14, 4, T_SOLID);
        place_battery(9, 10);
        add_enemy(12, 16, -1);
    } else if (route == 4u) {
        set_room_switch(0);
        stage[15][4] = T_SWITCH;
        add_platform(2, 14, 4, T_SOLID);
        add_platform(7, 11, 4, T_TOGGLE);
        add_platform(13, 9, 4, T_SOLID);
        place_battery(15, 8);
    } else if (route == 5u) {
        add_platform(2, 15, 4, T_CONV_R);
        add_platform(8, 12, 4, T_SOLID);
        add_platform(13, 10, 4, T_SOLID);
        stage[13][6] = T_SPIKE;
        place_battery(14, 9);
        add_coin_line(8, 11, 4);
    } else if (route == 6u) {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(9, 12, 5, T_CRACK);
        stage[15][15] = T_SPRING;
        place_battery(11, 11);
        add_hazard_line(7, 16, 3, T_SPIKE);
    } else {
        add_platform(2, 14, 5, T_SOLID);
        add_platform(8, 10, 4, T_SOLID);
        add_platform(13, 13, 4, T_SOLID);
        stage[9][14] = T_BUBBLE;
        add_hazard_line(9, 16, 6, T_WATER);
        place_battery(9, 9);
        add_enemy(14, 16, -1);
    }
}

void build_switch_room(uint8_t local) {
    uint8_t route;
    route = (uint8_t)(local & 3u);
    set_room_switch((uint8_t)((local & 1u) == 0u));
    add_platform(2, 14, 4, T_SOLID);
    stage[(local & 4u) ? 13 : 15][4] = T_SWITCH;
    if (route == 0u) {
        add_platform(7, 12, 6, T_TOGGLE);
        add_platform(14, 9, 4, T_SOLID);
        place_battery(15, 8);
        add_coin_line(8, 11, 4);
    } else if (route == 1u) {
        add_platform(7, 10, 4, T_SOLID);
        add_platform(12, 13, 5, T_TOGGLE);
        add_hazard_line(8, 16, 5, T_SPIKE);
        place_battery(8, 9);
    } else if (route == 2u) {
        add_platform(6, 13, 4, T_TOGGLE);
        add_platform(12, 10, 4, T_TOGGLE);
        stage[15][15] = T_SPRING;
        place_battery(13, 9);
        add_coin_line(6, 12, 3);
    } else {
        add_platform(6, 11, 5, T_SOLID);
        add_platform(12, 8, 4, T_TOGGLE);
        add_hazard_line(7, 16, 4, T_SPIKE);
        place_battery(13, 7);
        add_enemy(11, 16, 1);
    }
    if (local >= 8u) add_enemy((uint8_t)(8u + (local & 3u)), 16, (local & 1u) ? -1 : 1);
}

void build_water_room(uint8_t local) {
    uint8_t route;
    route = (uint8_t)(local & 3u);
    add_platform(2, 14, 4, T_SOLID);
    if (route == 0u) {
        add_hazard_line(6, 16, 8, T_WATER);
        add_platform(8, 12, 4, T_SOLID);
        stage[12][3] = T_BUBBLE;
        place_battery(9, 11);
        add_coin_line(11, 11, 3);
    } else if (route == 1u) {
        add_hazard_line(5, 16, 5, T_WATER);
        add_hazard_line(13, 16, 3, T_WATER);
        stage[10][5] = T_FAN_R;
        add_platform(10, 11, 4, T_SOLID);
        place_battery(12, 10);
    } else if (route == 2u) {
        add_hazard_line(6, 16, 9, T_WATER);
        stage[13][4] = T_BUBBLE;
        stage[9][15] = T_FAN_L;
        add_platform(8, 9, 4, T_SOLID);
        place_battery(9, 8);
        add_enemy(13, 16, -1);
    } else {
        add_hazard_line(4, 16, 4, T_WATER);
        add_hazard_line(11, 16, 5, T_WATER);
        stage[15][6] = T_SPRING;
        stage[8][14] = (local & 4u) ? T_FAN_L : T_FAN_R;
        add_platform(10, 8, 5, T_SOLID);
        stage[12][3] = T_BUBBLE;
        place_battery(12, 7);
    }
    if (local >= 8u) add_hazard_line((uint8_t)(7u + (local & 3u)), 15, 2, T_SPIKE);
}

void build_motion_room(uint8_t local) {
    uint8_t route;
    route = (uint8_t)(local & 3u);
    add_platform(2, 14, 4, (local & 1u) ? T_CONV_R : T_CONV_L);
    if (route == 0u) {
        add_platform(8, 12, 5, T_CONV_R);
        add_platform(14, 9, 4, T_SOLID);
        place_battery(15, 8);
    } else if (route == 1u) {
        add_platform(7, 10, 4, T_CONV_L);
        add_platform(12, 13, 5, T_CONV_R);
        add_hazard_line(6, 16, 4, T_SPIKE);
        place_battery(8, 9);
    } else if (route == 2u) {
        add_platform(5, 12, 4, T_SOLID);
        add_platform(13, 8, 4, T_CONV_L);
        stage[14][10] = T_FAN_R;
        place_battery(14, 7);
        add_coin_line(5, 11, 3);
    } else {
        add_platform(4, 11, 3, T_CRACK);
        add_platform(12, 10, 4, T_CONV_R);
        stage[13][15] = T_SPRING;
        add_hazard_line(8, 16, 4, T_SPIKE);
        place_battery(13, 9);
    }
    if (local >= 4u) {
        add_moving_platform((uint8_t)(6u + (local & 3u)),
                            (uint8_t)((local & 4u) ? 7u : 8u),
                            3,
                            (local & 1u) ? 1 : -1);
    }
    if (local >= 10u) add_enemy((uint8_t)(10u + (local & 3u)), 16, (local & 1u) ? -1 : 1);
}

void build_mixed_room(uint8_t local) {
    uint8_t route;
    route = (uint8_t)(local & 7u);
    set_room_switch((uint8_t)((local & 1u) == 0u));
    add_platform(2, 14, 4, T_SOLID);
    if (route == 0u) {
        stage[15][4] = T_SWITCH;
        add_platform(7, 12, 4, T_TOGGLE);
        add_platform(12, 9, 4, T_CRACK);
        add_hazard_line(8, 16, 5, T_SPIKE);
        place_battery(13, 8);
    } else if (route == 1u) {
        add_hazard_line(5, 16, 8, T_WATER);
        stage[12][3] = T_BUBBLE;
        stage[10][14] = T_FAN_L;
        add_platform(9, 10, 4, T_CONV_R);
        place_battery(10, 9);
        add_enemy(14, 16, -1);
    } else if (route == 2u) {
        stage[14][6] = T_SPRING;
        add_platform(9, 9, 5, T_TOGGLE);
        stage[13][4] = T_SWITCH;
        add_hazard_line(7, 16, 4, T_SPIKE);
        place_battery(11, 8);
    } else if (route == 3u) {
        add_platform(5, 12, 4, T_CRACK);
        add_platform(12, 10, 5, T_CONV_L);
        add_hazard_line(7, 16, 3, T_WATER);
        place_battery(14, 9);
        add_enemy(9, 16, 1);
    } else if (route == 4u) {
        add_hazard_line(6, 16, 6, T_SPIKE);
        stage[13][3] = T_BUBBLE;
        add_moving_platform(8, 10, 3, 1);
        place_battery(9, 9);
    } else if (route == 5u) {
        add_platform(7, 13, 4, T_CONV_R);
        stage[11][15] = T_FAN_L;
        add_platform(12, 8, 4, T_CRACK);
        add_hazard_line(5, 16, 4, T_WATER);
        place_battery(14, 7);
        add_enemy(13, 16, -1);
    } else if (route == 6u) {
        stage[15][4] = T_SWITCH;
        add_platform(7, 12, 3, T_TOGGLE);
        add_platform(11, 9, 4, T_TOGGLE);
        stage[15][14] = T_SPRING;
        add_hazard_line(7, 16, 5, T_WATER);
        place_battery(12, 8);
    } else {
        add_platform(6, 12, 4, T_CRACK);
        add_platform(12, 11, 5, T_CONV_R);
        add_hazard_line(5, 16, 4, T_SPIKE);
        add_hazard_line(12, 16, 4, T_WATER);
        stage[10][4] = T_BUBBLE;
        place_battery(13, 10);
        add_enemy(8, 16, 1);
        add_enemy(14, 16, -1);
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
    soften_exit();
}

void generate_panic(uint16_t depth) {
    uint16_t seed;
    uint8_t i;
    uint8_t danger;
    uint8_t x;
    uint8_t y;
    uint8_t battery_x;
    uint8_t battery_y;
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

    battery_x = (uint8_t)(5u + (depth % 10u));
    battery_y = (uint8_t)(4u + (depth & 1u));
    support_x = (uint8_t)(battery_x - 1u);
    place_battery(battery_x, battery_y);
    add_platform_if_empty(support_x, (uint8_t)(battery_y + 1u), 3, (depth & 2u) ? T_CRACK : T_SOLID);
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
    soften_exit();
}
