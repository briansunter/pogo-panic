#include <gb/gb.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define NUM_ADVENTURE_LEVELS 80
#define LEVELS_PER_WORLD 16
#define SCREEN_TILES_W 20
#define SCREEN_TILES_H 18
#define FP_SHIFT 4
#define TILE_PX 8
#define TILE_FP (TILE_PX << FP_SHIFT)
#define PLAYER_W 7
#define PLAYER_H 13
#define MAX_ENEMIES 5
#define FONT_BASE 18
#define SPRITE_BASE 96
#define FONT_COUNT 42
#define HUD_Y 1
#define CEILING_Y 2
#define SAVE_MAGIC 0x5047u
#define SAVE_VERSION 1u

#define FIX(v) ((int16_t)((v) << FP_SHIFT))
#define TILE8(a,b,c,d,e,f,g,h) a,a,b,b,c,c,d,d,e,e,f,f,g,g,h,h

enum TileType {
    T_EMPTY = 0,
    T_SOLID,
    T_CRACK,
    T_SPRING,
    T_SPIKE,
    T_COIN,
    T_BATTERY,
    T_EXIT,
    T_SWITCH,
    T_TOGGLE,
    T_FAN_L,
    T_FAN_R,
    T_CONV_L,
    T_CONV_R,
    T_WATER,
    T_BUBBLE,
    T_MOVING,
    T_ROCK
};

enum GameMode {
    MODE_ADVENTURE = 0,
    MODE_PANIC,
    MODE_TRIAL
};

enum GameState {
    STATE_TITLE = 0,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_CLEAR,
    STATE_DEAD
};

typedef struct Enemy {
    int16_t x;
    int16_t y;
    int8_t vx;
    uint8_t alive;
} Enemy;

typedef struct SaveData {
    uint16_t magic;
    uint8_t version;
    uint8_t unlocked;
    uint16_t best_time[NUM_ADVENTURE_LEVELS];
    uint16_t panic_best;
    uint8_t checksum;
} SaveData;

static const uint8_t bg_tiles[] = {
    TILE8(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), /* empty */
    TILE8(0xff,0x81,0xbd,0xa5,0xa5,0xbd,0x81,0xff), /* solid */
    TILE8(0xff,0x81,0xa1,0x91,0x89,0x85,0x81,0xff), /* crack */
    TILE8(0x00,0x7e,0x18,0x3c,0x66,0x7e,0x18,0x00), /* spring */
    TILE8(0x18,0x18,0x3c,0x3c,0x7e,0x7e,0xff,0x00), /* spikes */
    TILE8(0x00,0x3c,0x42,0x5a,0x5a,0x42,0x3c,0x00), /* coin */
    TILE8(0x3c,0x7e,0xe7,0xff,0xff,0xe7,0x7e,0x3c), /* battery */
    TILE8(0x7e,0x42,0x5a,0x5a,0x52,0x5a,0x42,0x7e), /* exit */
    TILE8(0x00,0x3c,0x42,0x99,0x99,0x42,0x3c,0x00), /* switch */
    TILE8(0xff,0x81,0x99,0x81,0x81,0x99,0x81,0xff), /* toggle */
    TILE8(0x10,0x30,0x7e,0xff,0x7e,0x30,0x10,0x00), /* fan left */
    TILE8(0x08,0x0c,0x7e,0xff,0x7e,0x0c,0x08,0x00), /* fan right */
    TILE8(0x44,0x22,0x11,0x88,0x44,0x22,0x11,0x88), /* conveyor left */
    TILE8(0x11,0x22,0x44,0x88,0x11,0x22,0x44,0x88), /* conveyor right */
    TILE8(0x00,0x24,0x5a,0x81,0x24,0x5a,0x81,0x00), /* water */
    TILE8(0x00,0x3c,0x42,0x81,0x81,0x42,0x3c,0x00), /* bubble */
    TILE8(0x3c,0x7e,0xdb,0xff,0xff,0xdb,0x7e,0x3c), /* moving */
    TILE8(0x18,0x3c,0x7e,0xff,0xdb,0xff,0x7e,0x3c)  /* rock */
};

static const uint8_t sprite_tiles[] = {
    TILE8(0x3c,0x7e,0xdb,0xff,0xbd,0x7e,0x24,0x66), /* body */
    TILE8(0x18,0x18,0x18,0x3c,0x18,0x18,0x3c,0x66), /* pogo */
    TILE8(0x00,0x00,0x3c,0x7e,0xdb,0xff,0x7e,0x00), /* slime */
    TILE8(0x18,0x3c,0x7e,0xff,0xdb,0xff,0x7e,0x3c), /* rock */
    TILE8(0x00,0x3c,0x42,0x99,0x99,0x42,0x3c,0x00)  /* bubble ring */
};

static const uint8_t font_tiles[] = {
    TILE8(0x38,0x44,0x44,0x7c,0x44,0x44,0x44,0x00), /* A */
    TILE8(0x78,0x44,0x44,0x78,0x44,0x44,0x78,0x00), /* B */
    TILE8(0x3c,0x40,0x40,0x40,0x40,0x40,0x3c,0x00), /* C */
    TILE8(0x78,0x44,0x44,0x44,0x44,0x44,0x78,0x00), /* D */
    TILE8(0x7c,0x40,0x40,0x78,0x40,0x40,0x7c,0x00), /* E */
    TILE8(0x7c,0x40,0x40,0x78,0x40,0x40,0x40,0x00), /* F */
    TILE8(0x3c,0x40,0x40,0x5c,0x44,0x44,0x3c,0x00), /* G */
    TILE8(0x44,0x44,0x44,0x7c,0x44,0x44,0x44,0x00), /* H */
    TILE8(0x7c,0x10,0x10,0x10,0x10,0x10,0x7c,0x00), /* I */
    TILE8(0x1c,0x08,0x08,0x08,0x08,0x48,0x30,0x00), /* J */
    TILE8(0x44,0x48,0x50,0x60,0x50,0x48,0x44,0x00), /* K */
    TILE8(0x40,0x40,0x40,0x40,0x40,0x40,0x7c,0x00), /* L */
    TILE8(0x44,0x6c,0x54,0x54,0x44,0x44,0x44,0x00), /* M */
    TILE8(0x44,0x64,0x54,0x4c,0x44,0x44,0x44,0x00), /* N */
    TILE8(0x38,0x44,0x44,0x44,0x44,0x44,0x38,0x00), /* O */
    TILE8(0x78,0x44,0x44,0x78,0x40,0x40,0x40,0x00), /* P */
    TILE8(0x38,0x44,0x44,0x44,0x54,0x48,0x34,0x00), /* Q */
    TILE8(0x78,0x44,0x44,0x78,0x50,0x48,0x44,0x00), /* R */
    TILE8(0x3c,0x40,0x40,0x38,0x04,0x04,0x78,0x00), /* S */
    TILE8(0x7c,0x10,0x10,0x10,0x10,0x10,0x10,0x00), /* T */
    TILE8(0x44,0x44,0x44,0x44,0x44,0x44,0x38,0x00), /* U */
    TILE8(0x44,0x44,0x44,0x44,0x44,0x28,0x10,0x00), /* V */
    TILE8(0x44,0x44,0x44,0x54,0x54,0x6c,0x44,0x00), /* W */
    TILE8(0x44,0x44,0x28,0x10,0x28,0x44,0x44,0x00), /* X */
    TILE8(0x44,0x44,0x28,0x10,0x10,0x10,0x10,0x00), /* Y */
    TILE8(0x7c,0x04,0x08,0x10,0x20,0x40,0x7c,0x00), /* Z */
    TILE8(0x38,0x44,0x4c,0x54,0x64,0x44,0x38,0x00), /* 0 */
    TILE8(0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00), /* 1 */
    TILE8(0x38,0x44,0x04,0x18,0x20,0x40,0x7c,0x00), /* 2 */
    TILE8(0x78,0x04,0x04,0x38,0x04,0x04,0x78,0x00), /* 3 */
    TILE8(0x08,0x18,0x28,0x48,0x7c,0x08,0x08,0x00), /* 4 */
    TILE8(0x7c,0x40,0x40,0x78,0x04,0x04,0x78,0x00), /* 5 */
    TILE8(0x38,0x40,0x40,0x78,0x44,0x44,0x38,0x00), /* 6 */
    TILE8(0x7c,0x04,0x08,0x10,0x20,0x20,0x20,0x00), /* 7 */
    TILE8(0x38,0x44,0x44,0x38,0x44,0x44,0x38,0x00), /* 8 */
    TILE8(0x38,0x44,0x44,0x3c,0x04,0x04,0x38,0x00), /* 9 */
    TILE8(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), /* space */
    TILE8(0x00,0x10,0x10,0x00,0x00,0x10,0x10,0x00), /* colon */
    TILE8(0x00,0x00,0x00,0x7c,0x00,0x00,0x00,0x00), /* dash */
    TILE8(0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00), /* dot */
    TILE8(0x04,0x08,0x08,0x10,0x20,0x20,0x40,0x00), /* slash */
    TILE8(0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00)  /* bang */
};

static uint8_t stage[SCREEN_TILES_H][SCREEN_TILES_W];
static uint8_t scratch[SCREEN_TILES_W];
static Enemy enemies[MAX_ENEMIES];
static SaveData save_data;

static uint8_t state = STATE_TITLE;
static uint8_t game_mode = MODE_ADVENTURE;
static uint8_t selected_level = 0;
static uint8_t current_level = 0;
static uint16_t panic_depth = 0;
static uint8_t level_world = 0;
static uint8_t coins = 0;
static uint8_t battery = 0;
static uint8_t bubble = 0;
static uint8_t switch_on = 1;
static uint8_t switch_cooldown = 0;
static uint16_t level_time = 0;
static uint8_t clear_wait = 0;
static uint8_t dead_wait = 0;
static uint8_t last_joy = 0;
static uint8_t hud_dirty = 0;
static uint8_t hud_second = 0xffu;
static uint8_t enemy_count = 0;
static uint8_t moving_active = 0;
static uint8_t moving_x = 0;
static uint8_t moving_y = 0;
static uint8_t moving_w = 0;
static int8_t moving_dir = 1;
static uint8_t moving_tick = 0;

static int16_t player_x = 0;
static int16_t player_y = 0;
static int16_t player_vx = 0;
static int16_t player_vy = 0;
static uint8_t stomping = 0;
static uint8_t invuln = 0;

static uint8_t glyph_for(char c) {
    if ((c >= 'A') && (c <= 'Z')) return (uint8_t)(c - 'A');
    if ((c >= '0') && (c <= '9')) return (uint8_t)(26 + (c - '0'));
    if (c == ':') return 37;
    if (c == '-') return 38;
    if (c == '.') return 39;
    if (c == '/') return 40;
    if (c == '!') return 41;
    return 36;
}

static void draw_char(uint8_t x, uint8_t y, char c) {
    uint8_t t;
    t = (uint8_t)(FONT_BASE + glyph_for(c));
    set_bkg_tiles(x, y, 1, 1, &t);
}

static void draw_text(uint8_t x, uint8_t y, const char *text) {
    while ((*text) && (x < SCREEN_TILES_W)) {
        draw_char(x, y, *text);
        x++;
        text++;
    }
}

static void draw_u8_2(uint8_t x, uint8_t y, uint8_t value) {
    draw_char(x, y, (char)('0' + ((value / 10) % 10)));
    draw_char((uint8_t)(x + 1), y, (char)('0' + (value % 10)));
}

static void draw_u8_3(uint8_t x, uint8_t y, uint8_t value) {
    draw_char(x, y, (char)('0' + (value / 100)));
    draw_char((uint8_t)(x + 1), y, (char)('0' + ((value / 10) % 10)));
    draw_char((uint8_t)(x + 2), y, (char)('0' + (value % 10)));
}

static void draw_time(uint8_t x, uint8_t y, uint16_t frames) {
    uint16_t sec;
    sec = frames / 60u;
    if (sec > 999u) sec = 999u;
    draw_char(x, y, (char)('0' + (sec / 100u)));
    draw_char((uint8_t)(x + 1), y, (char)('0' + ((sec / 10u) % 10u)));
    draw_char((uint8_t)(x + 2), y, (char)('0' + (sec % 10u)));
}

static void fill_screen(uint8_t tile) {
    uint8_t x;
    uint8_t y;
    for (x = 0; x < SCREEN_TILES_W; x++) scratch[x] = tile;
    for (y = 0; y < SCREEN_TILES_H; y++) set_bkg_tiles(0, y, SCREEN_TILES_W, 1, scratch);
}

static uint8_t checksum_save(const SaveData *data) {
    const uint8_t *raw;
    uint16_t i;
    uint8_t sum;
    raw = (const uint8_t *)data;
    sum = 0x5au;
    for (i = 0; i < (uint16_t)(sizeof(SaveData) - 1u); i++) {
        sum = (uint8_t)((sum << 1) ^ (sum >> 7) ^ raw[i]);
    }
    return sum;
}

static void save_defaults(void) {
    uint8_t i;
    save_data.magic = SAVE_MAGIC;
    save_data.version = SAVE_VERSION;
    save_data.unlocked = 1;
    save_data.panic_best = 0;
    for (i = 0; i < NUM_ADVENTURE_LEVELS; i++) save_data.best_time[i] = 0xffffu;
    save_data.checksum = checksum_save(&save_data);
}

static void save_write(void) {
    uint8_t *sram;
    sram = (uint8_t *)0xa000u;
    save_data.checksum = checksum_save(&save_data);
    ENABLE_RAM_MBC1;
    memcpy(sram, &save_data, sizeof(SaveData));
    DISABLE_RAM_MBC1;
}

static void save_read(void) {
    const uint8_t *sram;
    sram = (const uint8_t *)0xa000u;
    ENABLE_RAM_MBC1;
    memcpy(&save_data, sram, sizeof(SaveData));
    DISABLE_RAM_MBC1;
    if ((save_data.magic != SAVE_MAGIC) || (save_data.version != SAVE_VERSION) ||
        (save_data.checksum != checksum_save(&save_data)) ||
        (save_data.unlocked == 0) || (save_data.unlocked > NUM_ADVENTURE_LEVELS)) {
        save_defaults();
        save_write();
    }
}

static uint8_t stage_tile(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return T_SOLID;
    return stage[y][x];
}

static uint8_t bg_for_tile(uint8_t t) {
    if (t == T_TOGGLE) return switch_on ? T_TOGGLE : T_EMPTY;
    return t;
}

static void put_stage_tile(uint8_t x, uint8_t y, uint8_t t) {
    uint8_t b;
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = t;
    b = bg_for_tile(t);
    set_bkg_tiles(x, y, 1, 1, &b);
}

static uint8_t is_solid_tile(uint8_t t) {
    if ((t == T_SOLID) || (t == T_CRACK) || (t == T_SPRING) ||
        (t == T_SWITCH) || (t == T_CONV_L) || (t == T_CONV_R) ||
        (t == T_MOVING) || (t == T_ROCK)) return 1;
    if (t == T_TOGGLE) return switch_on;
    return 0;
}

static uint8_t is_danger_tile(uint8_t t) {
    return (uint8_t)((t == T_SPIKE) || (t == T_WATER));
}

static uint8_t tile_at_fixed(int16_t fx, int16_t fy) {
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

static void redraw_stage(void) {
    uint8_t x;
    uint8_t y;
    for (y = 0; y < SCREEN_TILES_H; y++) {
        for (x = 0; x < SCREEN_TILES_W; x++) scratch[x] = bg_for_tile(stage[y][x]);
        set_bkg_tiles(0, y, SCREEN_TILES_W, 1, scratch);
    }
}

static void add_platform(uint8_t x, uint8_t y, uint8_t w, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < w; i++) {
        if ((uint8_t)(x + i) < SCREEN_TILES_W) stage[y][x + i] = tile;
    }
}

static void add_enemy(uint8_t tx, uint8_t ty, int8_t vx) {
    if (enemy_count >= MAX_ENEMIES) return;
    enemies[enemy_count].x = FIX(tx * TILE_PX);
    enemies[enemy_count].y = FIX(ty * TILE_PX);
    enemies[enemy_count].vx = vx;
    enemies[enemy_count].alive = 1;
    enemy_count++;
}

static uint16_t rng_step(uint16_t seed) {
    return (uint16_t)(seed * 1109u + 1987u);
}

static void reset_common(void) {
    uint8_t x;
    uint8_t y;
    for (y = 0; y < SCREEN_TILES_H; y++) {
        for (x = 0; x < SCREEN_TILES_W; x++) stage[y][x] = T_EMPTY;
    }
    for (x = 0; x < SCREEN_TILES_W; x++) stage[CEILING_Y][x] = T_SOLID;
    for (x = 0; x < SCREEN_TILES_W; x++) stage[17][x] = T_SOLID;
    for (y = CEILING_Y; y < SCREEN_TILES_H; y++) {
        stage[y][0] = T_SOLID;
        stage[y][19] = T_SOLID;
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
    level_time = 0;
    player_x = FIX(18);
    player_y = FIX(80);
    player_vx = 0;
    player_vy = FIX(-5);
    stage[16][18] = T_EXIT;
}

static void sprinkle_coins(uint8_t seed, uint8_t count) {
    uint8_t i;
    uint8_t x;
    uint8_t y;
    for (i = 0; i < count; i++) {
        x = (uint8_t)(2u + ((uint8_t)(seed + i * 5u) % 16u));
        y = (uint8_t)(4u + ((uint8_t)(seed + i * 3u) % 10u));
        if (stage[y][x] == T_EMPTY) stage[y][x] = T_COIN;
    }
}

static void generate_adventure(uint8_t level) {
    uint8_t local;
    uint8_t world;
    uint8_t x;
    uint8_t seed;
    world = (uint8_t)(level >> 4);
    local = (uint8_t)(level & 15u);
    seed = (uint8_t)(level * 17u + 9u);
    level_world = world;
    reset_common();

    add_platform(2, (uint8_t)(14u - (local & 1u)), 5, (world == 3) ? T_CONV_R : T_SOLID);
    add_platform((uint8_t)(11u - (local % 4u)), 11, 6, (world == 1) ? T_TOGGLE : T_SOLID);
    add_platform((uint8_t)(4u + (local % 5u)), (uint8_t)(7u + (local % 2u)), 4, (world == 3) ? T_CONV_L : T_SOLID);

    stage[(uint8_t)(4u + (local % 4u))][(uint8_t)(5u + ((local * 3u) % 10u))] = T_BATTERY;
    sprinkle_coins(seed, (uint8_t)(5u + (local % 4u)));

    if (world == 0) {
        add_platform(7, 15, 4, T_CRACK);
        stage[16][(uint8_t)(5u + (local % 5u))] = T_SPIKE;
        stage[13][(uint8_t)(12u + (local % 5u))] = T_SPRING;
        add_enemy((uint8_t)(8u + (local % 6u)), 16, (local & 1u) ? 1 : -1);
    } else if (world == 1) {
        switch_on = (uint8_t)((local & 1u) == 0u);
        stage[15][4] = T_SWITCH;
        stage[13][14] = T_SPRING;
        add_platform(9, 9, 4, T_TOGGLE);
        add_enemy((uint8_t)(10u + (local % 4u)), 10, 1);
    } else if (world == 2) {
        for (x = 4; x < 9; x++) stage[16][x] = T_WATER;
        for (x = 12; x < 16; x++) stage[16][x] = T_WATER;
        stage[12][(uint8_t)(3u + (local % 5u))] = T_BUBBLE;
        stage[8][15] = (local & 1u) ? T_FAN_L : T_FAN_R;
        add_enemy((uint8_t)(11u + (local % 5u)), 16, -1);
    } else if (world == 3) {
        stage[12][5] = T_FAN_R;
        stage[8][15] = T_FAN_L;
        moving_active = 1;
        moving_x = (uint8_t)(5u + (local % 4u));
        moving_y = 5;
        moving_w = 3;
        moving_dir = (local & 1u) ? 1 : -1;
        add_platform(moving_x, moving_y, moving_w, T_MOVING);
        stage[16][9] = T_SPIKE;
    } else {
        add_platform(5, 15, 3, T_CRACK);
        add_platform(12, 13, 5, T_TOGGLE);
        stage[10][3] = T_SWITCH;
        stage[14][15] = T_FAN_L;
        stage[16][7] = T_SPIKE;
        stage[16][12] = T_WATER;
        stage[12][17] = T_BUBBLE;
        stage[9][9] = T_SPRING;
        add_enemy(9, 16, 1);
        add_enemy(14, 12, -1);
    }
}

static void generate_panic(uint16_t depth) {
    uint16_t seed;
    uint8_t i;
    uint8_t danger;
    uint8_t x;
    uint8_t y;
    seed = (uint16_t)(depth * 97u + 31u);
    danger = (uint8_t)(depth / 10u);
    if (danger > 5u) danger = 5u;
    level_world = 4;
    reset_common();
    player_x = FIX(18);
    player_y = FIX(92);
    switch_on = (uint8_t)((depth & 1u) == 0u);

    for (i = 0; i < 4u; i++) {
        seed = rng_step(seed);
        x = (uint8_t)(2u + (seed % 13u));
        seed = rng_step(seed);
        y = (uint8_t)(6u + (seed % 9u));
        add_platform(x, y, (uint8_t)(3u + (seed % 4u)), (i == 2u) ? T_TOGGLE : T_SOLID);
    }

    stage[4][(uint8_t)(4u + (depth % 12u))] = T_BATTERY;
    stage[13][4] = T_SWITCH;
    stage[8][15] = (depth & 1u) ? T_FAN_L : T_FAN_R;
    stage[12][6] = T_BUBBLE;
    sprinkle_coins((uint8_t)seed, (uint8_t)(4u + danger));

    for (i = 0; i < (uint8_t)(3u + danger); i++) {
        seed = rng_step(seed);
        x = (uint8_t)(2u + (seed % 16u));
        y = (uint8_t)(16u - (i & 1u));
        if (stage[y][x] == T_EMPTY) stage[y][x] = ((i + depth) & 1u) ? T_SPIKE : T_WATER;
    }
    for (i = 0; i < (uint8_t)(1u + (danger / 2u)); i++) {
        add_enemy((uint8_t)(4u + ((seed + i * 5u) % 11u)), 16, (i & 1u) ? 1 : -1);
    }
    if (danger >= 2u) {
        moving_active = 1;
        moving_x = 7;
        moving_y = 7;
        moving_w = 3;
        moving_dir = 1;
        add_platform(moving_x, moving_y, moving_w, T_MOVING);
    }
}

static void load_level(void) {
    if (game_mode == MODE_PANIC) {
        generate_panic(panic_depth);
    } else {
        generate_adventure(current_level);
    }
    redraw_stage();
    hud_dirty = 1;
    hud_second = 0xffu;
    state = STATE_PLAY;
}

static void hide_sprites(void) {
    uint8_t i;
    for (i = 0; i < 16u; i++) move_sprite(i, 0, 0);
}

static void draw_hud(void) {
    uint8_t i;
    for (i = 0; i < SCREEN_TILES_W; i++) scratch[i] = T_EMPTY;
    set_bkg_tiles(0, 0, SCREEN_TILES_W, 1, scratch);
    set_bkg_tiles(0, HUD_Y, SCREEN_TILES_W, 1, scratch);
    if (game_mode == MODE_ADVENTURE) {
        draw_text(0, HUD_Y, "ADV");
        draw_u8_2(4, HUD_Y, (uint8_t)(current_level + 1u));
    } else if (game_mode == MODE_TRIAL) {
        draw_text(0, HUD_Y, "TIM");
        draw_u8_2(4, HUD_Y, (uint8_t)(current_level + 1u));
    } else {
        draw_text(0, HUD_Y, "PAN");
        if (panic_depth > 99u) draw_u8_3(4, HUD_Y, 99);
        else draw_u8_2(5, HUD_Y, (uint8_t)panic_depth);
    }
    draw_char(8, HUD_Y, 'G');
    draw_u8_2(9, HUD_Y, coins);
    if (battery) draw_char(12, HUD_Y, 'P');
    if (bubble) draw_char(13, HUD_Y, 'O');
    draw_time(16, HUD_Y, level_time);
}

static void mark_hud_dirty(void) {
    hud_dirty = 1;
}

static void present_frame(void) {
    uint8_t second;
    vsync();
    if (state != STATE_PLAY) return;

    second = (uint8_t)(level_time / 60u);
    if (second != hud_second) {
        hud_second = second;
        hud_dirty = 1;
    }
    if (hud_dirty) {
        draw_hud();
        hud_dirty = 0;
    }
}

static uint8_t point_tile_x(int16_t fx) {
    int16_t px;
    px = fx >> FP_SHIFT;
    if (px < 0) return 0;
    return (uint8_t)(px >> 3);
}

static uint8_t point_tile_y(int16_t fy) {
    int16_t py;
    py = fy >> FP_SHIFT;
    if (py < 0) return 0;
    return (uint8_t)(py >> 3);
}

static void trigger_switch(void) {
    if (switch_cooldown) return;
    switch_on = (uint8_t)!switch_on;
    switch_cooldown = 20;
    redraw_stage();
    mark_hud_dirty();
}

static void hurt_player(void) {
    if (invuln) return;
    if (bubble) {
        bubble = 0;
        invuln = 70;
        player_vy = FIX(-5);
        mark_hud_dirty();
        return;
    }
    state = STATE_DEAD;
    dead_wait = 24;
    hide_sprites();
    fill_screen(T_EMPTY);
    draw_text(5, 5, "POGO PANIC");
    draw_text(4, 8, "A TRY AGAIN");
    draw_text(5, 10, "B MENU");
}

static void complete_level(void) {
    if ((game_mode != MODE_PANIC) && (level_time < save_data.best_time[current_level])) {
        save_data.best_time[current_level] = level_time;
    }
    if (game_mode == MODE_ADVENTURE) {
        if ((current_level + 1u) < NUM_ADVENTURE_LEVELS) {
            if (save_data.unlocked < (uint8_t)(current_level + 2u)) save_data.unlocked = (uint8_t)(current_level + 2u);
        }
    } else if (game_mode == MODE_PANIC) {
        panic_depth++;
        if (panic_depth > save_data.panic_best) save_data.panic_best = panic_depth;
    }
    save_write();
    state = STATE_CLEAR;
    clear_wait = 24;
    hide_sprites();
    fill_screen(T_EMPTY);
    draw_text(7, 4, "CLEAR");
    draw_text(4, 6, "TIME");
    draw_time(10, 6, level_time);
    draw_text(4, 8, "COINS");
    draw_u8_2(11, 8, coins);
    if (game_mode == MODE_PANIC) {
        draw_text(4, 10, "DEPTH");
        if (panic_depth > 99u) draw_u8_3(11, 10, 99);
        else draw_u8_2(12, 10, (uint8_t)panic_depth);
    } else {
        draw_text(4, 10, "BEST");
        draw_time(10, 10, save_data.best_time[current_level]);
    }
    draw_text(3, 13, "A NEXT  B MENU");
}

static uint8_t player_center_tile(void) {
    return tile_at_fixed((int16_t)(player_x + FIX(PLAYER_W / 2)), (int16_t)(player_y + FIX(PLAYER_H / 2)));
}

static void collect_tiles(void) {
    uint8_t tx1;
    uint8_t tx2;
    uint8_t ty1;
    uint8_t ty2;
    uint8_t x;
    uint8_t y;
    uint8_t t;
    tx1 = point_tile_x(player_x);
    tx2 = point_tile_x((int16_t)(player_x + FIX(PLAYER_W - 1)));
    ty1 = point_tile_y(player_y);
    ty2 = point_tile_y((int16_t)(player_y + FIX(PLAYER_H - 1)));
    for (y = ty1; y <= ty2; y++) {
        for (x = tx1; x <= tx2; x++) {
            t = stage_tile(x, y);
            if (t == T_COIN) {
                coins++;
                put_stage_tile(x, y, T_EMPTY);
                mark_hud_dirty();
            } else if (t == T_BATTERY) {
                battery = 1;
                put_stage_tile(x, y, T_EMPTY);
                mark_hud_dirty();
            } else if (t == T_BUBBLE) {
                bubble = 1;
                put_stage_tile(x, y, T_EMPTY);
                mark_hud_dirty();
            } else if (is_danger_tile(t)) {
                hurt_player();
                return;
            }
        }
    }
    if (battery && (player_center_tile() == T_EXIT)) {
        complete_level();
        return;
    }
}

static void do_bounce(uint8_t tile, uint8_t joy) {
    if (tile == T_SWITCH) trigger_switch();
    if (tile == T_SPRING) {
        player_vy = (joy & J_B) ? FIX(-6) : FIX(-8);
    } else {
        player_vy = (joy & J_B) ? FIX(-4) : FIX(-6);
    }
    if (tile == T_CONV_L) player_vx -= FIX(1);
    if (tile == T_CONV_R) player_vx += FIX(1);
    if ((joy & J_B) && (joy & J_LEFT)) player_vx -= FIX(1);
    if ((joy & J_B) && (joy & J_RIGHT)) player_vx += FIX(1);
    stomping = 0;
}

static void break_crack_at(uint8_t tx, uint8_t ty) {
    if (stage_tile(tx, ty) == T_CRACK) put_stage_tile(tx, ty, T_EMPTY);
}

static void resolve_horizontal(void) {
    int16_t right;
    int16_t left;
    int16_t top;
    int16_t mid;
    int16_t bottom;
    uint8_t hit;
    uint8_t tx;
    top = (int16_t)(player_y + FIX(1));
    mid = (int16_t)(player_y + FIX(PLAYER_H / 2));
    bottom = (int16_t)(player_y + FIX(PLAYER_H - 2));
    if (player_vx > 0) {
        right = (int16_t)(player_x + FIX(PLAYER_W - 1));
        hit = (uint8_t)(is_solid_tile(tile_at_fixed(right, top)) ||
                        is_solid_tile(tile_at_fixed(right, mid)) ||
                        is_solid_tile(tile_at_fixed(right, bottom)));
        if (hit) {
            tx = point_tile_x(right);
            player_x = (int16_t)FIX((tx * TILE_PX) - PLAYER_W);
            player_vx = 0;
        }
    } else if (player_vx < 0) {
        left = player_x;
        hit = (uint8_t)(is_solid_tile(tile_at_fixed(left, top)) ||
                        is_solid_tile(tile_at_fixed(left, mid)) ||
                        is_solid_tile(tile_at_fixed(left, bottom)));
        if (hit) {
            tx = point_tile_x(left);
            player_x = FIX((tx + 1u) * TILE_PX);
            player_vx = 0;
        }
    }
}

static void resolve_vertical(uint8_t joy) {
    int16_t foot;
    int16_t head;
    int16_t left;
    int16_t right;
    uint8_t tile_l;
    uint8_t tile_r;
    uint8_t tx_l;
    uint8_t tx_r;
    uint8_t ty;
    uint8_t land_tile;
    uint8_t broke;
    left = (int16_t)(player_x + FIX(1));
    right = (int16_t)(player_x + FIX(PLAYER_W - 2));
    if (player_vy > 0) {
        foot = (int16_t)(player_y + FIX(PLAYER_H));
        tile_l = tile_at_fixed(left, foot);
        tile_r = tile_at_fixed(right, foot);
        tx_l = point_tile_x(left);
        tx_r = point_tile_x(right);
        ty = point_tile_y(foot);
        broke = 0;
        if (stomping && (tile_l == T_CRACK)) {
            break_crack_at(tx_l, ty);
            broke = 1;
        }
        if (stomping && (tile_r == T_CRACK)) {
            break_crack_at(tx_r, ty);
            broke = 1;
        }
        if (broke) {
            tile_l = tile_at_fixed(left, foot);
            tile_r = tile_at_fixed(right, foot);
        }
        if (is_solid_tile(tile_l) || is_solid_tile(tile_r)) {
            land_tile = is_solid_tile(tile_l) ? tile_l : tile_r;
            player_y = (int16_t)FIX((ty * TILE_PX) - PLAYER_H);
            do_bounce(land_tile, joy);
        }
    } else if (player_vy < 0) {
        head = player_y;
        tile_l = tile_at_fixed(left, head);
        tile_r = tile_at_fixed(right, head);
        if (is_solid_tile(tile_l) || is_solid_tile(tile_r)) {
            ty = point_tile_y(head);
            player_y = FIX((ty + 1u) * TILE_PX);
            player_vy = 0;
        }
    }
}

static uint8_t rect_overlap(int16_t ax, int16_t ay, uint8_t aw, uint8_t ah, int16_t bx, int16_t by, uint8_t bw, uint8_t bh) {
    int16_t apx;
    int16_t apy;
    int16_t bpx;
    int16_t bpy;
    apx = ax >> FP_SHIFT;
    apy = ay >> FP_SHIFT;
    bpx = bx >> FP_SHIFT;
    bpy = by >> FP_SHIFT;
    if ((apx + aw) <= bpx) return 0;
    if ((bpx + bw) <= apx) return 0;
    if ((apy + ah) <= bpy) return 0;
    if ((bpy + bh) <= apy) return 0;
    return 1;
}

static void update_enemies(void) {
    uint8_t i;
    Enemy *e;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (i >= enemy_count) {
            move_sprite((uint8_t)(4u + i), 0, 0);
            continue;
        }
        e = &enemies[i];
        if (!e->alive) {
            move_sprite((uint8_t)(4u + i), 0, 0);
            continue;
        }
        e->x = (int16_t)(e->x + e->vx);
        if (e->vx > 0) {
            if (is_solid_tile(tile_at_fixed((int16_t)(e->x + FIX(7)), e->y)) ||
                !is_solid_tile(tile_at_fixed((int16_t)(e->x + FIX(8)), (int16_t)(e->y + FIX(9))))) {
                e->vx = (int8_t)-e->vx;
            }
        } else {
            if (is_solid_tile(tile_at_fixed(e->x, e->y)) ||
                !is_solid_tile(tile_at_fixed((int16_t)(e->x - FIX(1)), (int16_t)(e->y + FIX(9))))) {
                e->vx = (int8_t)-e->vx;
            }
        }
        if (rect_overlap(player_x, player_y, PLAYER_W, PLAYER_H, e->x, e->y, 8, 8)) {
            if ((player_vy > 0) && ((player_y + FIX(PLAYER_H - 3)) < e->y)) {
                e->alive = 0;
                player_vy = FIX(-6);
                stomping = 0;
                coins++;
            } else {
                hurt_player();
                return;
            }
        }
        move_sprite((uint8_t)(4u + i), (uint8_t)((e->x >> FP_SHIFT) + 8), (uint8_t)((e->y >> FP_SHIFT) + 16));
    }
}

static void update_moving_platform(void) {
    uint8_t i;
    uint8_t blocked;
    if (!moving_active) return;
    moving_tick++;
    if (moving_tick < 18u) return;
    moving_tick = 0;
    blocked = 0;
    if (moving_dir > 0) {
        if ((moving_x + moving_w + 1u) >= 19u) blocked = 1;
        else if (stage[moving_y][moving_x + moving_w] != T_EMPTY) blocked = 1;
    } else {
        if (moving_x <= 1u) blocked = 1;
        else if (stage[moving_y][moving_x - 1u] != T_EMPTY) blocked = 1;
    }
    if (blocked) moving_dir = (int8_t)-moving_dir;
    for (i = 0; i < moving_w; i++) put_stage_tile((uint8_t)(moving_x + i), moving_y, T_EMPTY);
    moving_x = (uint8_t)(moving_x + moving_dir);
    for (i = 0; i < moving_w; i++) put_stage_tile((uint8_t)(moving_x + i), moving_y, T_MOVING);
    if (((player_y + FIX(PLAYER_H)) >> FP_SHIFT) == (int16_t)(moving_y * TILE_PX)) {
        if (((player_x >> FP_SHIFT) + PLAYER_W) > (int16_t)(moving_x * TILE_PX) &&
            (player_x >> FP_SHIFT) < (int16_t)((moving_x + moving_w) * TILE_PX)) {
            player_x = (int16_t)(player_x + (moving_dir << FP_SHIFT));
        }
    }
}

static void update_player(uint8_t joy) {
    uint8_t center_tile;
    int16_t gravity;
    if (invuln) invuln--;
    if (switch_cooldown) switch_cooldown--;
    if (joy & J_LEFT) player_vx -= (joy & J_B) ? 4 : 3;
    if (joy & J_RIGHT) player_vx += (joy & J_B) ? 4 : 3;
    if (!(joy & (J_LEFT | J_RIGHT))) {
        if (player_vx > 0) player_vx--;
        else if (player_vx < 0) player_vx++;
    }
    if (player_vx > FIX(3)) player_vx = FIX(3);
    if (player_vx < FIX(-3)) player_vx = FIX(-3);

    if ((joy & J_A) && (player_vy > FIX(-2))) {
        player_vy = FIX(5);
        stomping = 1;
    }

    center_tile = tile_at_fixed((int16_t)(player_x + FIX(PLAYER_W / 2)), (int16_t)(player_y + FIX(PLAYER_H / 2)));
    if (center_tile == T_FAN_L) player_vx -= 3;
    if (center_tile == T_FAN_R) player_vx += 3;
    gravity = (level_world == 2u) ? 3 : 4;
    if (stomping) gravity = 6;
    player_vy = (int16_t)(player_vy + gravity);
    if (player_vy > FIX(6)) player_vy = FIX(6);

    player_x = (int16_t)(player_x + player_vx);
    resolve_horizontal();
    player_y = (int16_t)(player_y + player_vy);
    resolve_vertical(joy);

    if (player_x < FIX(8)) player_x = FIX(8);
    if (player_x > FIX(145)) player_x = FIX(145);
    if (player_y > FIX(144)) hurt_player();
    collect_tiles();
}

static void draw_player(void) {
    uint8_t px;
    uint8_t py;
    px = (uint8_t)((player_x >> FP_SHIFT) + 8);
    py = (uint8_t)((player_y >> FP_SHIFT) + 16);
    if (invuln && (invuln & 4u)) {
        move_sprite(0, 0, 0);
        move_sprite(1, 0, 0);
    } else {
        move_sprite(0, px, py);
        move_sprite(1, px, (uint8_t)(py + 8u));
    }
    if (bubble) move_sprite(10, px, (uint8_t)(py + 4u));
    else move_sprite(10, 0, 0);
}

static void start_selected(void) {
    if (game_mode == MODE_ADVENTURE) {
        if (selected_level >= save_data.unlocked) selected_level = (uint8_t)(save_data.unlocked - 1u);
        current_level = selected_level;
    } else if (game_mode == MODE_TRIAL) {
        current_level = selected_level;
    } else {
        panic_depth = 0;
    }
    load_level();
}

static void show_title(void) {
    hide_sprites();
    fill_screen(T_EMPTY);
    draw_text(2, 2, "POCKET POGO");
    draw_text(5, 4, "PANIC");
    draw_text(1, 6, "BOUNCE BETTER");
    draw_text(1, 9, "LEFT RIGHT MODE");
    draw_text(1, 10, "UP DOWN STAGE");
    draw_text(1, 11, "A START");
    if (game_mode == MODE_ADVENTURE) {
        draw_text(3, 14, "ADVENTURE");
        draw_u8_2(14, 14, (uint8_t)(selected_level + 1u));
    } else if (game_mode == MODE_PANIC) {
        draw_text(5, 14, "PANIC");
        draw_text(3, 15, "BEST");
        if (save_data.panic_best > 99u) draw_u8_3(9, 15, 99);
        else draw_u8_2(10, 15, (uint8_t)save_data.panic_best);
    } else {
        draw_text(3, 14, "TIME TRIAL");
        draw_u8_2(15, 14, (uint8_t)(selected_level + 1u));
    }
}

static void update_title(uint8_t pressed) {
    if (pressed & J_LEFT) {
        game_mode = (game_mode == 0u) ? MODE_TRIAL : (uint8_t)(game_mode - 1u);
        show_title();
    } else if (pressed & J_RIGHT) {
        game_mode = (uint8_t)((game_mode + 1u) % 3u);
        show_title();
    }
    if ((game_mode != MODE_PANIC) && (pressed & J_UP)) {
        if (selected_level >= 10u) selected_level = (uint8_t)(selected_level - 10u);
        else selected_level = 0;
        show_title();
    } else if ((game_mode != MODE_PANIC) && (pressed & J_DOWN)) {
        if (selected_level < (NUM_ADVENTURE_LEVELS - 10u)) selected_level = (uint8_t)(selected_level + 10u);
        else selected_level = (NUM_ADVENTURE_LEVELS - 1u);
        show_title();
    }
    if ((game_mode != MODE_PANIC) && (pressed & J_B)) {
        selected_level = (uint8_t)((selected_level + 1u) % NUM_ADVENTURE_LEVELS);
        show_title();
    }
    if (pressed & (J_A | J_START)) start_selected();
}

static void update_clear(uint8_t pressed) {
    if (clear_wait) {
        clear_wait--;
        return;
    }
    if (pressed & J_B) {
        state = STATE_TITLE;
        show_title();
    } else if (pressed & (J_A | J_START)) {
        if (game_mode == MODE_ADVENTURE) {
            if ((current_level + 1u) < NUM_ADVENTURE_LEVELS) {
                current_level++;
                selected_level = current_level;
            }
        }
        load_level();
    }
}

static void update_dead(uint8_t pressed) {
    if (dead_wait) {
        dead_wait--;
        return;
    }
    if (pressed & J_B) {
        state = STATE_TITLE;
        show_title();
    } else if (pressed & (J_A | J_START)) {
        load_level();
    }
}

static void show_pause(void) {
    hide_sprites();
    fill_screen(T_EMPTY);
    draw_text(7, 5, "PAUSED");
    draw_text(4, 8, "START RESUME");
    draw_text(5, 10, "B MENU");
}

static void init_video(void) {
    DISPLAY_OFF;
    set_bkg_data(0, (uint8_t)(sizeof(bg_tiles) / 16u), bg_tiles);
    set_bkg_data(FONT_BASE, FONT_COUNT, font_tiles);
    set_sprite_data(SPRITE_BASE, (uint8_t)(sizeof(sprite_tiles) / 16u), sprite_tiles);
    set_sprite_tile(0, SPRITE_BASE);
    set_sprite_tile(1, (uint8_t)(SPRITE_BASE + 1u));
    set_sprite_tile(4, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(5, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(6, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(7, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(8, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(10, (uint8_t)(SPRITE_BASE + 4u));
    BGP_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
    OBP0_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;
}

void main(void) {
    uint8_t joy;
    uint8_t pressed;
    init_video();
    save_read();
    show_title();
    while (1) {
        joy = joypad();
        pressed = (uint8_t)(joy & (uint8_t)(~last_joy));
        last_joy = joy;

        if (state == STATE_TITLE) {
            update_title(pressed);
        } else if (state == STATE_PLAY) {
            if (pressed & J_START) {
                state = STATE_PAUSE;
                show_pause();
            } else {
                level_time++;
                update_moving_platform();
                update_player(joy);
                if (state == STATE_PLAY) {
                    update_enemies();
                    draw_player();
                }
            }
        } else if (state == STATE_PAUSE) {
            if (pressed & J_START) {
                redraw_stage();
                mark_hud_dirty();
                state = STATE_PLAY;
            } else if (pressed & J_B) {
                state = STATE_TITLE;
                show_title();
            }
        } else if (state == STATE_CLEAR) {
            update_clear(pressed);
        } else if (state == STATE_DEAD) {
            update_dead(pressed);
        }
        present_frame();
    }
}
