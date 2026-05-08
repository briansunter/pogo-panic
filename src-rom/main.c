#include <gb/gb.h>
#include <gb/hardware.h>
#include <stdint.h>
#include <string.h>

#define NUM_ADVENTURE_LEVELS 80
#define LEVELS_PER_WORLD 16
#define SCREEN_TILES_W 20
#define SCREEN_TILES_H 18
#define FP_SHIFT 4
#define TILE_PX 8
#define PLAYER_W 6
#define PLAYER_H 8
#define MAX_ENEMIES 5
#define FONT_BASE 22
#define SPRITE_BASE 96
#define FONT_COUNT 42
#define HUD_Y 1
#define CEILING_Y 2
#define SAVE_MAGIC 0x5047u
#define SAVE_VERSION 1u

#define FIX(v) ((int16_t)((v) << FP_SHIFT))
#define TILE8(a,b,c,d,e,f,g,h) a,a,b,b,c,c,d,d,e,e,f,f,g,g,h,h
#define TILE2(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p
#define TILEF_SOLID 0x01u
#define TILEF_DANGER 0x02u
#define PLAYER_ACCEL 2
#define PLAYER_FAST_ACCEL 3
#define PLAYER_MAX_VX 40
#define PLAYER_AIR_KICK 12
#define BOUNCE_NORMAL ((int16_t)-88)
#define BOUNCE_SHORT FIX(-4)
#define BOUNCE_SPRING ((int16_t)-120)
#define BOUNCE_SPRING_SHORT FIX(-6)

#define NOTE_REST 0u
#define NOTE_C3 1046u
#define NOTE_CS3 1102u
#define NOTE_D3 1155u
#define NOTE_DS3 1205u
#define NOTE_E3 1253u
#define NOTE_F3 1297u
#define NOTE_FS3 1339u
#define NOTE_G3 1379u
#define NOTE_GS3 1417u
#define NOTE_A3 1452u
#define NOTE_AS3 1486u
#define NOTE_B3 1517u
#define NOTE_C4 1547u
#define NOTE_CS4 1575u
#define NOTE_D4 1602u
#define NOTE_DS4 1627u
#define NOTE_E4 1650u
#define NOTE_F4 1673u
#define NOTE_FS4 1694u
#define NOTE_G4 1714u
#define NOTE_GS4 1732u
#define NOTE_A4 1750u
#define NOTE_AS4 1767u
#define NOTE_B4 1783u
#define NOTE_C5 1798u
#define NOTE_CS5 1812u
#define NOTE_D5 1825u
#define NOTE_DS5 1837u
#define NOTE_E5 1849u
#define NOTE_F5 1860u
#define NOTE_FS5 1871u
#define NOTE_G5 1881u
#define NOTE_GS5 1890u
#define NOTE_A5 1899u
#define NOTE_AS5 1907u
#define NOTE_B5 1915u
#define NOTE_C6 1923u

#define BASS_C3 NOTE_C3
#define BASS_D3 NOTE_D3
#define BASS_E3 NOTE_E3
#define BASS_F3 NOTE_F3
#define BASS_G3 NOTE_G3
#define BASS_A3 NOTE_A3
#define BASS_B3 NOTE_B3

#define DRUM_NONE 0u
#define DRUM_KICK 1u
#define DRUM_SNARE 2u
#define DRUM_HAT 3u
#define DRUM_TICK 4u
#define DRUM_BOOM 5u

#define MUSIC_NONE 0u
#define MUSIC_TITLE 1u
#define MUSIC_PLAY 2u
#define MUSIC_SWITCH 3u
#define MUSIC_WATER 4u
#define MUSIC_MOTION 5u
#define MUSIC_MIXED 6u
#define MUSIC_PANIC 7u
#define MUSIC_CLEAR 8u
#define MUSIC_DEAD 9u

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
    T_ROCK,
    T_TOGGLE_OFF,
    T_ARROW_D,
    T_ARROW_R,
    T_WALL,
    TILE_COUNT
};

enum GameMode {
    MODE_ADVENTURE = 0,
    MODE_PANIC,
    MODE_TRIAL
};

enum GameState {
    STATE_TITLE = 0,
    STATE_HOWTO,
    STATE_MODE,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_CLEAR,
    STATE_DEAD
};

enum FeedbackKind {
    FEEDBACK_NONE = 0,
    FEEDBACK_BATTERY,
    FEEDBACK_SHIELD,
    FEEDBACK_STOMP,
    FEEDBACK_CRACK
};

enum GameEvent {
    EVENT_COIN = 0,
    EVENT_BATTERY,
    EVENT_BUBBLE,
    EVENT_SHIELD,
    EVENT_STOMP,
    EVENT_CRACK,
    EVENT_SWITCH
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
    TILE2(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), /* empty */
    TILE2(0xff,0x00,0x81,0x7e,0x81,0x66,0x81,0x7e,0x81,0x7e,0xff,0x00,0xff,0xff,0xff,0xff), /* solid */
    TILE2(0xff,0x00,0x81,0x7e,0x99,0x5a,0x8d,0x6e,0x99,0x5a,0x81,0x7e,0xff,0xff,0xff,0xff), /* crack */
    TILE2(0x00,0x00,0x3c,0x3c,0x18,0x18,0x3c,0x3c,0x66,0x66,0x3c,0x3c,0x18,0x18,0x3c,0x3c), /* spring */
    TILE2(0x00,0x00,0x10,0x10,0x10,0x10,0x38,0x38,0x38,0x38,0x7c,0x7c,0x7c,0xfe,0xff,0xff), /* spikes */
    TILE2(0x00,0x00,0x18,0x18,0x24,0x24,0x42,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x00,0x00), /* coin */
    TILE2(0x3c,0x3c,0x66,0x66,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x66,0x66,0x3c,0x3c,0x00,0x00), /* battery */
    TILE2(0x7e,0xff,0x7e,0xc3,0x66,0xdb,0x66,0xdb,0x66,0xdb,0x66,0xdb,0x7e,0xc3,0x7e,0xff), /* exit */
    TILE2(0x00,0x00,0x3c,0x00,0x42,0x3c,0x5a,0x3c,0x5a,0x3c,0x24,0x18,0x7e,0x00,0x00,0x00), /* switch */
    TILE2(0xff,0xff,0xff,0x81,0xc3,0xbd,0xdb,0xa5,0xdb,0xa5,0xc3,0xbd,0xff,0x81,0xff,0xff), /* toggle */
    TILE2(0x10,0x00,0x30,0x10,0x7e,0x3c,0x7e,0xfe,0x7e,0xfe,0x7e,0x3c,0x30,0x10,0x10,0x00), /* fan left */
    TILE2(0x08,0x00,0x0c,0x08,0x7e,0x3c,0x7e,0x7f,0x7e,0x7f,0x7e,0x3c,0x0c,0x08,0x08,0x00), /* fan right */
    TILE2(0xff,0x00,0x81,0x7e,0xa5,0x66,0x89,0x4e,0xa5,0x66,0x81,0x7e,0x7e,0xff,0xff,0xff), /* conveyor left */
    TILE2(0xff,0x00,0x81,0x7e,0xa5,0x66,0x91,0x72,0xa5,0x66,0x81,0x7e,0x7e,0xff,0xff,0xff), /* conveyor right */
    TILE2(0x00,0x00,0x66,0x00,0x99,0x66,0x66,0x99,0x99,0x66,0x66,0x99,0x99,0x66,0x66,0x00), /* water */
    TILE2(0x00,0x00,0x3c,0x00,0x42,0x3c,0x99,0x66,0x99,0x66,0x42,0x3c,0x3c,0x00,0x00,0x00), /* bubble */
    TILE2(0xff,0x00,0x81,0x7e,0xbd,0x7e,0xa5,0x7e,0xa5,0x7e,0xbd,0x7e,0x81,0x7e,0xff,0x00), /* moving */
    TILE2(0x18,0x00,0x24,0x18,0x5a,0x3c,0xbd,0x7e,0x7e,0xff,0xbd,0x7e,0x5a,0x3c,0x24,0x18), /* rock */
    TILE2(0x81,0x81,0x00,0x00,0x3c,0x00,0x24,0x00,0x24,0x00,0x3c,0x00,0x00,0x00,0x81,0x81), /* toggle off */
    TILE2(0x18,0x18,0x18,0x18,0x18,0x18,0x7e,0x7e,0x3c,0x3c,0x18,0x18,0x00,0x00,0x00,0x00), /* arrow down */
    TILE2(0x10,0x10,0x18,0x18,0x1c,0x1c,0xfe,0xfe,0xfe,0xfe,0x1c,0x1c,0x18,0x18,0x10,0x10), /* arrow right */
    TILE2(0xff,0xff,0x81,0xff,0xbd,0xc3,0x81,0xff,0x81,0xff,0xbd,0xc3,0x81,0xff,0xff,0xff)  /* wall */
};

static const uint8_t sprite_tiles[] = {
    TILE2(0x38,0x38,0x38,0x38,0x7c,0x7c,0x38,0x38,0x28,0x28,0x10,0x10,0x38,0x38,0x10,0x10), /* player */
    TILE2(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), /* unused */
    TILE2(0x00,0x00,0x00,0x00,0x3c,0x3c,0x7e,0x7e,0xff,0xff,0xff,0xff,0x7e,0x7e,0x42,0x42), /* slime */
    TILE2(0x18,0x00,0x24,0x18,0x5a,0x3c,0xbd,0x7e,0x7e,0xff,0xbd,0x7e,0x5a,0x3c,0x24,0x18), /* rock */
    TILE2(0x00,0x00,0x3c,0x3c,0x66,0x66,0xc3,0xc3,0xc3,0xc3,0x66,0x66,0x3c,0x3c,0x00,0x00)  /* bubble ring */
};

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
    TILEF_SOLID     /* wall */
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
static uint8_t menu_tick = 0;
static uint8_t how_page = 0;
static uint8_t feedback_kind = FEEDBACK_NONE;
static uint8_t feedback_timer = 0;
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

static uint8_t music_track = MUSIC_NONE;
static uint8_t music_step = 0;
static uint8_t music_frame = 0;
static uint8_t music_paused = 0;
static uint8_t music_sfx_timer = 0;

static const uint8_t bass_wave[] = {
    0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98,
    0x76, 0x54, 0x32, 0x10, 0x01, 0x23, 0x45, 0x67
};

static const uint16_t title_lead[] = {
    NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6, NOTE_REST, NOTE_G5, NOTE_E5, NOTE_G5,
    NOTE_A5, NOTE_G5, NOTE_E5, NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5, NOTE_REST,
    NOTE_F5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_G5, NOTE_E5, NOTE_D5, NOTE_REST,
    NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5, NOTE_C6, NOTE_B5, NOTE_G5, NOTE_REST
};

static const uint16_t title_harmony[] = {
    NOTE_C4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_E4, NOTE_REST,
    NOTE_F4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST,
    NOTE_F4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST,
    NOTE_C4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_REST
};

static const uint16_t title_bass[] = {
    BASS_C3, NOTE_REST, NOTE_REST, BASS_G3, BASS_C3, NOTE_REST, BASS_G3, NOTE_REST,
    BASS_F3, NOTE_REST, NOTE_REST, BASS_C3, BASS_G3, NOTE_REST, BASS_D3, NOTE_REST,
    BASS_F3, NOTE_REST, NOTE_REST, BASS_C3, BASS_E3, NOTE_REST, BASS_G3, NOTE_REST,
    BASS_C3, NOTE_REST, BASS_G3, NOTE_REST, BASS_C3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t title_drums[] = {
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT
};

static const uint16_t play_lead[] = {
    NOTE_C5, NOTE_E5, NOTE_G5, NOTE_A5, NOTE_G5, NOTE_E5, NOTE_D5, NOTE_G5,
    NOTE_E5, NOTE_G5, NOTE_C6, NOTE_REST, NOTE_B5, NOTE_G5, NOTE_E5, NOTE_D5,
    NOTE_F5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_G5, NOTE_E5, NOTE_C5, NOTE_E5,
    NOTE_D5, NOTE_F5, NOTE_A5, NOTE_REST, NOTE_G5, NOTE_A5, NOTE_B5, NOTE_REST,
    NOTE_E5, NOTE_G5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_G5, NOTE_E5, NOTE_G5,
    NOTE_C5, NOTE_D5, NOTE_E5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_C5,
    NOTE_A4, NOTE_C5, NOTE_E5, NOTE_G5, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_D5,
    NOTE_E5, NOTE_G5, NOTE_C6, NOTE_B5, NOTE_A5, NOTE_G5, NOTE_E5, NOTE_REST
};

static const uint16_t play_harmony[] = {
    NOTE_C4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_E4, NOTE_REST,
    NOTE_C4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST,
    NOTE_F4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_A4, NOTE_REST,
    NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_D5, NOTE_REST, NOTE_B4, NOTE_REST,
    NOTE_C4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_G4, NOTE_REST,
    NOTE_F4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_E4, NOTE_REST,
    NOTE_F4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_F4, NOTE_REST,
    NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_G4, NOTE_REST
};

static const uint16_t play_bass[] = {
    BASS_C3, NOTE_REST, BASS_G3, NOTE_REST, BASS_A3, NOTE_REST, BASS_G3, NOTE_REST,
    BASS_C3, NOTE_REST, BASS_G3, NOTE_REST, BASS_E3, NOTE_REST, BASS_G3, NOTE_REST,
    BASS_F3, NOTE_REST, BASS_C3, NOTE_REST, BASS_A3, NOTE_REST, BASS_C3, NOTE_REST,
    BASS_G3, NOTE_REST, BASS_D3, NOTE_REST, BASS_G3, NOTE_REST, BASS_B3, NOTE_REST,
    BASS_C3, NOTE_REST, BASS_G3, NOTE_REST, BASS_A3, NOTE_REST, BASS_E3, NOTE_REST,
    BASS_F3, NOTE_REST, BASS_C3, NOTE_REST, BASS_G3, NOTE_REST, BASS_C3, NOTE_REST,
    BASS_F3, NOTE_REST, BASS_C3, NOTE_REST, BASS_A3, NOTE_REST, BASS_D3, NOTE_REST,
    BASS_G3, NOTE_REST, BASS_D3, NOTE_REST, BASS_C3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t play_drums[] = {
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_HAT
};

static const uint16_t switch_lead[] = {
    NOTE_D5, NOTE_F5, NOTE_A5, NOTE_F5, NOTE_D5, NOTE_REST, NOTE_F5, NOTE_REST,
    NOTE_A4, NOTE_D5, NOTE_F5, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_C5,
    NOTE_D5, NOTE_F5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_D5,
    NOTE_DS5, NOTE_F5, NOTE_G5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_F5, NOTE_REST
};

static const uint16_t switch_harmony[] = {
    NOTE_D4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_F4, NOTE_REST, NOTE_A4, NOTE_REST,
    NOTE_D4, NOTE_REST, NOTE_F4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_C5, NOTE_REST,
    NOTE_G4, NOTE_REST, NOTE_D5, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_G4, NOTE_REST,
    NOTE_D4, NOTE_REST, NOTE_F4, NOTE_REST, NOTE_A4, NOTE_C5, NOTE_A4, NOTE_REST
};

static const uint16_t switch_bass[] = {
    BASS_D3, NOTE_REST, BASS_A3, NOTE_REST, BASS_D3, NOTE_REST, BASS_A3, NOTE_REST,
    BASS_D3, NOTE_REST, BASS_F3, NOTE_REST, BASS_A3, NOTE_REST, BASS_C3, NOTE_REST,
    BASS_G3, NOTE_REST, BASS_D3, NOTE_REST, BASS_A3, NOTE_REST, BASS_G3, NOTE_REST,
    BASS_D3, NOTE_REST, BASS_A3, NOTE_REST, BASS_D3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t switch_drums[] = {
    DRUM_KICK, DRUM_TICK, DRUM_HAT, DRUM_TICK, DRUM_SNARE, DRUM_TICK, DRUM_HAT, DRUM_TICK,
    DRUM_KICK, DRUM_TICK, DRUM_KICK, DRUM_TICK, DRUM_SNARE, DRUM_TICK, DRUM_HAT, DRUM_TICK,
    DRUM_KICK, DRUM_TICK, DRUM_HAT, DRUM_TICK, DRUM_SNARE, DRUM_TICK, DRUM_HAT, DRUM_TICK,
    DRUM_KICK, DRUM_TICK, DRUM_SNARE, DRUM_TICK, DRUM_BOOM, DRUM_TICK, DRUM_HAT, DRUM_TICK
};

static const uint16_t water_lead[] = {
    NOTE_F5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_G5, NOTE_E5, NOTE_C5, NOTE_REST,
    NOTE_D5, NOTE_F5, NOTE_A5, NOTE_F5, NOTE_E5, NOTE_G5, NOTE_C6, NOTE_REST,
    NOTE_A4, NOTE_C5, NOTE_F5, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_D5, NOTE_REST,
    NOTE_E5, NOTE_G5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_G5, NOTE_F5, NOTE_REST
};

static const uint16_t water_harmony[] = {
    NOTE_F4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_F4, NOTE_REST,
    NOTE_D4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_C5, NOTE_REST,
    NOTE_F4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_A4, NOTE_REST,
    NOTE_G4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_F4, NOTE_REST
};

static const uint16_t water_bass[] = {
    BASS_F3, NOTE_REST, NOTE_REST, BASS_C3, BASS_F3, NOTE_REST, NOTE_REST, NOTE_REST,
    BASS_D3, NOTE_REST, NOTE_REST, BASS_A3, BASS_G3, NOTE_REST, NOTE_REST, NOTE_REST,
    BASS_F3, NOTE_REST, NOTE_REST, BASS_C3, BASS_A3, NOTE_REST, NOTE_REST, NOTE_REST,
    BASS_G3, NOTE_REST, NOTE_REST, BASS_C3, BASS_F3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t water_drums[] = {
    DRUM_KICK, DRUM_NONE, DRUM_HAT, DRUM_NONE, DRUM_TICK, DRUM_NONE, DRUM_HAT, DRUM_NONE,
    DRUM_NONE, DRUM_NONE, DRUM_HAT, DRUM_NONE, DRUM_SNARE, DRUM_NONE, DRUM_HAT, DRUM_NONE,
    DRUM_KICK, DRUM_NONE, DRUM_HAT, DRUM_NONE, DRUM_NONE, DRUM_NONE, DRUM_HAT, DRUM_NONE,
    DRUM_NONE, DRUM_NONE, DRUM_HAT, DRUM_NONE, DRUM_SNARE, DRUM_NONE, DRUM_TICK, DRUM_NONE
};

static const uint16_t motion_lead[] = {
    NOTE_G5, NOTE_A5, NOTE_B5, NOTE_G5, NOTE_D5, NOTE_E5, NOTE_G5, NOTE_B5,
    NOTE_A5, NOTE_G5, NOTE_E5, NOTE_D5, NOTE_G5, NOTE_REST, NOTE_B5, NOTE_REST,
    NOTE_C6, NOTE_B5, NOTE_A5, NOTE_G5, NOTE_E5, NOTE_G5, NOTE_A5, NOTE_C6,
    NOTE_B5, NOTE_G5, NOTE_E5, NOTE_D5, NOTE_G5, NOTE_A5, NOTE_B5, NOTE_REST
};

static const uint16_t motion_harmony[] = {
    NOTE_G4, NOTE_REST, NOTE_D5, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_D5, NOTE_REST,
    NOTE_C5, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST,
    NOTE_C5, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST,
    NOTE_D5, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_D5, NOTE_REST
};

static const uint16_t motion_bass[] = {
    BASS_G3, NOTE_REST, BASS_D3, BASS_G3, BASS_B3, NOTE_REST, BASS_D3, NOTE_REST,
    BASS_A3, NOTE_REST, BASS_E3, BASS_A3, BASS_G3, NOTE_REST, BASS_D3, NOTE_REST,
    BASS_C3, NOTE_REST, BASS_G3, BASS_C3, BASS_E3, NOTE_REST, BASS_G3, NOTE_REST,
    BASS_D3, NOTE_REST, BASS_A3, BASS_D3, BASS_G3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t motion_drums[] = {
    DRUM_KICK, DRUM_HAT, DRUM_TICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_TICK, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_TICK, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_TICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_BOOM, DRUM_HAT, DRUM_TICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT
};

static const uint16_t mixed_lead[] = {
    NOTE_A5, NOTE_C6, NOTE_B5, NOTE_G5, NOTE_E5, NOTE_G5, NOTE_A5, NOTE_REST,
    NOTE_F5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_GS5, NOTE_A5, NOTE_B5, NOTE_REST,
    NOTE_E5, NOTE_G5, NOTE_A5, NOTE_C6, NOTE_B5, NOTE_A5, NOTE_G5, NOTE_E5,
    NOTE_D5, NOTE_F5, NOTE_A5, NOTE_B5, NOTE_C6, NOTE_B5, NOTE_A5, NOTE_REST
};

static const uint16_t mixed_harmony[] = {
    NOTE_A4, NOTE_REST, NOTE_E5, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_E5, NOTE_REST,
    NOTE_F4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_C5, NOTE_REST,
    NOTE_E4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST,
    NOTE_D4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_F4, NOTE_A4, NOTE_C5, NOTE_REST
};

static const uint16_t mixed_bass[] = {
    BASS_A3, NOTE_REST, BASS_E3, NOTE_REST, BASS_A3, NOTE_REST, BASS_G3, NOTE_REST,
    BASS_F3, NOTE_REST, BASS_C3, NOTE_REST, BASS_F3, NOTE_REST, BASS_E3, NOTE_REST,
    BASS_E3, NOTE_REST, BASS_B3, NOTE_REST, BASS_G3, NOTE_REST, BASS_E3, NOTE_REST,
    BASS_D3, NOTE_REST, BASS_A3, NOTE_REST, BASS_F3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t mixed_drums[] = {
    DRUM_BOOM, DRUM_HAT, DRUM_HAT, DRUM_TICK, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_KICK, DRUM_TICK, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_TICK, DRUM_HAT,
    DRUM_BOOM, DRUM_HAT, DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_TICK, DRUM_HAT, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_TICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_BOOM, DRUM_HAT
};

static const uint16_t panic_lead[] = {
    NOTE_E5, NOTE_G5, NOTE_B5, NOTE_C6, NOTE_B5, NOTE_G5, NOTE_E5, NOTE_G5,
    NOTE_D5, NOTE_FS5, NOTE_A5, NOTE_B5, NOTE_A5, NOTE_FS5, NOTE_D5, NOTE_FS5,
    NOTE_E5, NOTE_G5, NOTE_B5, NOTE_E5, NOTE_C6, NOTE_B5, NOTE_A5, NOTE_G5,
    NOTE_FS5, NOTE_A5, NOTE_C6, NOTE_A5, NOTE_B5, NOTE_A5, NOTE_G5, NOTE_REST
};

static const uint16_t panic_harmony[] = {
    NOTE_E4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST,
    NOTE_D4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_FS4, NOTE_REST, NOTE_A4, NOTE_REST,
    NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_G4, NOTE_REST,
    NOTE_D4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_G4, NOTE_REST
};

static const uint16_t panic_bass[] = {
    BASS_E3, BASS_E3, BASS_B3, NOTE_REST, BASS_E3, BASS_E3, BASS_B3, NOTE_REST,
    BASS_D3, BASS_D3, BASS_A3, NOTE_REST, BASS_D3, BASS_D3, BASS_A3, NOTE_REST,
    BASS_E3, BASS_E3, BASS_B3, NOTE_REST, BASS_C3, BASS_C3, BASS_G3, NOTE_REST,
    BASS_D3, BASS_D3, BASS_A3, NOTE_REST, BASS_E3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t panic_drums[] = {
    DRUM_BOOM, DRUM_HAT, DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_BOOM, DRUM_HAT, DRUM_KICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_BOOM, DRUM_HAT,
    DRUM_KICK, DRUM_HAT, DRUM_TICK, DRUM_HAT, DRUM_SNARE, DRUM_HAT, DRUM_BOOM, DRUM_HAT
};

static const uint16_t clear_lead[] = {
    NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6, NOTE_REST, NOTE_G5, NOTE_A5, NOTE_C6,
    NOTE_B5, NOTE_A5, NOTE_G5, NOTE_REST, NOTE_C6, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint16_t clear_harmony[] = {
    NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_REST, NOTE_E4, NOTE_F4, NOTE_A4,
    NOTE_G4, NOTE_F4, NOTE_E4, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint16_t clear_bass[] = {
    BASS_C3, NOTE_REST, BASS_G3, NOTE_REST, BASS_C3, NOTE_REST, BASS_F3, NOTE_REST,
    BASS_G3, NOTE_REST, BASS_C3, NOTE_REST, BASS_C3, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t clear_drums[] = {
    DRUM_KICK, DRUM_HAT, DRUM_HAT, DRUM_SNARE, DRUM_NONE, DRUM_HAT, DRUM_KICK, DRUM_HAT,
    DRUM_SNARE, DRUM_HAT, DRUM_HAT, DRUM_NONE, DRUM_KICK, DRUM_NONE, DRUM_NONE, DRUM_NONE
};

static const uint16_t dead_lead[] = {
    NOTE_G4, NOTE_E4, NOTE_C4, NOTE_REST, NOTE_D4, NOTE_C4, NOTE_REST, NOTE_REST,
    NOTE_C4, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint16_t dead_harmony[] = {
    NOTE_E4, NOTE_C4, NOTE_REST, NOTE_REST, NOTE_C4, NOTE_REST, NOTE_REST, NOTE_REST,
    NOTE_REST, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint16_t dead_bass[] = {
    BASS_G3, NOTE_REST, BASS_C3, NOTE_REST, BASS_D3, NOTE_REST, BASS_C3, NOTE_REST,
    NOTE_REST, NOTE_REST, NOTE_REST, NOTE_REST
};

static const uint8_t dead_drums[] = {
    DRUM_SNARE, DRUM_NONE, DRUM_KICK, DRUM_NONE, DRUM_SNARE, DRUM_NONE, DRUM_KICK, DRUM_NONE,
    DRUM_NONE, DRUM_NONE, DRUM_NONE, DRUM_NONE
};

static void music_silence(void) {
    rAUD1ENV = 0;
    rAUD2ENV = 0;
    rAUD3LEVEL = 0;
    rAUD4ENV = 0;
}

static void music_play_square1(uint16_t note, uint8_t volume, uint8_t duty) {
    if (note == NOTE_REST) {
        rAUD1ENV = 0;
        return;
    }
    rAUD1SWEEP = 0;
    rAUD1LEN = (uint8_t)(duty | AUDLEN_LENGTH(36));
    rAUD1ENV = (uint8_t)(AUDENV_VOL(volume) | AUDENV_DOWN | AUDENV_LENGTH(1));
    rAUD1LOW = (uint8_t)note;
    rAUD1HIGH = (uint8_t)(((note >> 8) & 0x07u) | AUDHIGH_RESTART | AUDHIGH_LENGTH_ON);
}

static void music_play_square2(uint16_t note, uint8_t volume, uint8_t duty) {
    if (note == NOTE_REST) {
        rAUD2ENV = 0;
        return;
    }
    rAUD2LEN = (uint8_t)(duty | AUDLEN_LENGTH(34));
    rAUD2ENV = (uint8_t)(AUDENV_VOL(volume) | AUDENV_DOWN | AUDENV_LENGTH(1));
    rAUD2LOW = (uint8_t)note;
    rAUD2HIGH = (uint8_t)(((note >> 8) & 0x07u) | AUDHIGH_RESTART | AUDHIGH_LENGTH_ON);
}

static void music_play_wave(uint16_t note) {
    if (note == NOTE_REST) {
        rAUD3LEVEL = 0;
        return;
    }
    rAUD3ENA = 0x80u;
    rAUD3LEN = 180u;
    rAUD3LEVEL = 0x40u;
    rAUD3LOW = (uint8_t)note;
    rAUD3HIGH = (uint8_t)(((note >> 8) & 0x07u) | AUDHIGH_RESTART | AUDHIGH_LENGTH_OFF);
}

static void music_play_drum(uint8_t drum) {
    if (drum == DRUM_NONE) return;
    if (drum == DRUM_KICK) {
        rAUD4LEN = 28u;
        rAUD4ENV = (uint8_t)(AUDENV_VOL(9) | AUDENV_DOWN | AUDENV_LENGTH(2));
        rAUD4POLY = 0x65u;
    } else if (drum == DRUM_SNARE) {
        rAUD4LEN = 22u;
        rAUD4ENV = (uint8_t)(AUDENV_VOL(7) | AUDENV_DOWN | AUDENV_LENGTH(2));
        rAUD4POLY = 0x43u;
    } else if (drum == DRUM_TICK) {
        rAUD4LEN = 10u;
        rAUD4ENV = (uint8_t)(AUDENV_VOL(3) | AUDENV_DOWN | AUDENV_LENGTH(1));
        rAUD4POLY = (uint8_t)(0x12u | AUD4POLY_WIDTH_7BIT);
    } else if (drum == DRUM_BOOM) {
        rAUD4LEN = 34u;
        rAUD4ENV = (uint8_t)(AUDENV_VOL(11) | AUDENV_DOWN | AUDENV_LENGTH(3));
        rAUD4POLY = 0x76u;
    } else {
        rAUD4LEN = 18u;
        rAUD4ENV = (uint8_t)(AUDENV_VOL(4) | AUDENV_DOWN | AUDENV_LENGTH(1));
        rAUD4POLY = (uint8_t)(0x22u | AUD4POLY_WIDTH_7BIT);
    }
    rAUD4GO = (uint8_t)(AUDHIGH_RESTART | AUDHIGH_LENGTH_ON);
}

static uint8_t music_step_delay(void) {
    if (music_track == MUSIC_TITLE) return 9u;
    if (music_track == MUSIC_PLAY) return 8u;
    if (music_track == MUSIC_SWITCH) return 7u;
    if (music_track == MUSIC_WATER) return 10u;
    if (music_track == MUSIC_MOTION) return 6u;
    if (music_track == MUSIC_MIXED) return 7u;
    if (music_track == MUSIC_PANIC) return 5u;
    if (music_track == MUSIC_CLEAR) return 7u;
    if (music_track == MUSIC_DEAD) return 10u;
    return 8u;
}

static void music_run_pattern(const uint16_t *lead,
                              const uint16_t *harmony,
                              const uint16_t *bass,
                              const uint8_t *drums,
                              uint8_t length,
                              uint8_t loop,
                              uint8_t lead_volume,
                              uint8_t harmony_volume,
                              uint8_t lead_duty,
                              uint8_t harmony_duty) {
    uint8_t i;
    if (music_step >= length) {
        if (loop) music_step = 0;
        else {
            music_track = MUSIC_NONE;
            music_silence();
            return;
        }
    }

    i = music_step;
    music_play_square2(lead[i], lead_volume, lead_duty);
    if (!music_sfx_timer) music_play_square1(harmony[i], harmony_volume, harmony_duty);
    music_play_wave(bass[i]);
    music_play_drum(drums[i]);
    music_step++;
}

static void music_update(void) {
    if (music_sfx_timer) music_sfx_timer--;
    if (music_paused || (music_track == MUSIC_NONE)) return;
    if (music_frame) {
        music_frame--;
        return;
    }

    music_frame = (uint8_t)(music_step_delay() - 1u);
    if (music_track == MUSIC_TITLE) {
        music_run_pattern(title_lead, title_harmony, title_bass, title_drums, (uint8_t)(sizeof(title_lead) / sizeof(title_lead[0])), 1u,
                          8u, 4u, AUDLEN_DUTY_50, AUDLEN_DUTY_25);
    } else if (music_track == MUSIC_PLAY) {
        music_run_pattern(play_lead, play_harmony, play_bass, play_drums, (uint8_t)(sizeof(play_lead) / sizeof(play_lead[0])), 1u,
                          9u, 4u, AUDLEN_DUTY_50, AUDLEN_DUTY_25);
    } else if (music_track == MUSIC_SWITCH) {
        music_run_pattern(switch_lead, switch_harmony, switch_bass, switch_drums, (uint8_t)(sizeof(switch_lead) / sizeof(switch_lead[0])), 1u,
                          8u, 5u, AUDLEN_DUTY_12_5, AUDLEN_DUTY_25);
    } else if (music_track == MUSIC_WATER) {
        music_run_pattern(water_lead, water_harmony, water_bass, water_drums, (uint8_t)(sizeof(water_lead) / sizeof(water_lead[0])), 1u,
                          7u, 3u, AUDLEN_DUTY_75, AUDLEN_DUTY_50);
    } else if (music_track == MUSIC_MOTION) {
        music_run_pattern(motion_lead, motion_harmony, motion_bass, motion_drums, (uint8_t)(sizeof(motion_lead) / sizeof(motion_lead[0])), 1u,
                          10u, 5u, AUDLEN_DUTY_25, AUDLEN_DUTY_12_5);
    } else if (music_track == MUSIC_MIXED) {
        music_run_pattern(mixed_lead, mixed_harmony, mixed_bass, mixed_drums, (uint8_t)(sizeof(mixed_lead) / sizeof(mixed_lead[0])), 1u,
                          10u, 5u, AUDLEN_DUTY_50, AUDLEN_DUTY_75);
    } else if (music_track == MUSIC_PANIC) {
        music_run_pattern(panic_lead, panic_harmony, panic_bass, panic_drums, (uint8_t)(sizeof(panic_lead) / sizeof(panic_lead[0])), 1u,
                          12u, 6u, AUDLEN_DUTY_12_5, AUDLEN_DUTY_25);
    } else if (music_track == MUSIC_CLEAR) {
        music_run_pattern(clear_lead, clear_harmony, clear_bass, clear_drums, (uint8_t)(sizeof(clear_lead) / sizeof(clear_lead[0])), 0u,
                          10u, 5u, AUDLEN_DUTY_50, AUDLEN_DUTY_25);
    } else if (music_track == MUSIC_DEAD) {
        music_run_pattern(dead_lead, dead_harmony, dead_bass, dead_drums, (uint8_t)(sizeof(dead_lead) / sizeof(dead_lead[0])), 0u,
                          8u, 3u, AUDLEN_DUTY_75, AUDLEN_DUTY_25);
    }
}

static void music_start(uint8_t track) {
    music_track = track;
    music_step = 0;
    music_frame = 0;
    music_paused = 0;
    music_sfx_timer = 0;
    music_silence();
}

static void music_pause(uint8_t paused) {
    music_paused = paused;
    if (paused) music_silence();
}

static void music_sfx_event(uint8_t event) {
    if (music_track == MUSIC_DEAD) return;
    if (event == EVENT_COIN) {
        music_sfx_timer = 7u;
        music_play_square1(NOTE_C6, 10u, AUDLEN_DUTY_25);
    } else if (event == EVENT_BATTERY) {
        music_sfx_timer = 10u;
        music_play_square1(NOTE_G5, 12u, AUDLEN_DUTY_50);
        music_play_drum(DRUM_HAT);
    } else if (event == EVENT_BUBBLE) {
        music_sfx_timer = 9u;
        music_play_square1(NOTE_A5, 9u, AUDLEN_DUTY_75);
    } else if (event == EVENT_SHIELD) {
        music_sfx_timer = 12u;
        music_play_square1(NOTE_C5, 11u, AUDLEN_DUTY_12_5);
        music_play_drum(DRUM_SNARE);
    } else if (event == EVENT_STOMP) {
        music_play_drum(DRUM_SNARE);
    } else if (event == EVENT_CRACK) {
        music_play_drum(DRUM_KICK);
    } else if (event == EVENT_SWITCH) {
        music_sfx_timer = 8u;
        music_play_square1(NOTE_D5, 8u, AUDLEN_DUTY_25);
    }
}

static void init_sound(void) {
    uint8_t i;
    rAUDENA = AUDENA_ON;
    rAUDVOL = (uint8_t)(AUDVOL_VOL_LEFT(5) | AUDVOL_VOL_RIGHT(5));
    rAUDTERM = (uint8_t)(AUDTERM_1_LEFT | AUDTERM_2_LEFT | AUDTERM_3_LEFT | AUDTERM_4_LEFT |
                         AUDTERM_1_RIGHT | AUDTERM_2_RIGHT | AUDTERM_3_RIGHT | AUDTERM_4_RIGHT);
    rAUD1SWEEP = 0;
    rAUD1ENV = 0;
    rAUD2ENV = 0;
    rAUD3ENA = 0;
    for (i = 0; i < (uint8_t)(sizeof(bass_wave) / sizeof(bass_wave[0])); i++) {
        AUD3WAVE[i] = bass_wave[i];
    }
    rAUD3ENA = 0x80u;
    rAUD3LEVEL = 0;
    rAUD4ENV = 0;
}

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

static void draw_tile_icon(uint8_t x, uint8_t y, uint8_t tile) {
    set_bkg_tiles(x, y, 1, 1, &tile);
}

static void draw_tile_run(uint8_t x, uint8_t y, uint8_t w, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < w; i++) scratch[i] = tile;
    set_bkg_tiles(x, y, w, 1, scratch);
}

static void draw_menu_rule(uint8_t y) {
    draw_tile_run(1, y, 18, T_TOGGLE_OFF);
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

static uint8_t progress_level_limit(void) {
    if (game_mode == MODE_PANIC) return 0;
    if (game_mode == MODE_ADVENTURE) return save_data.unlocked;
    return NUM_ADVENTURE_LEVELS;
}

static void progress_prepare_selected_level(void) {
    if (game_mode == MODE_ADVENTURE) {
        if (selected_level >= save_data.unlocked) selected_level = (uint8_t)(save_data.unlocked - 1u);
        current_level = selected_level;
    } else if (game_mode == MODE_TRIAL) {
        current_level = selected_level;
    } else {
        panic_depth = 0;
    }
}

static void progress_record_clear(void) {
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
}

static uint8_t stage_tile(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return T_SOLID;
    return stage[y][x];
}

static uint8_t bg_for_tile(uint8_t t) {
    if (t == T_TOGGLE) return switch_on ? T_TOGGLE : T_TOGGLE_OFF;
    return t;
}

static uint8_t tile_has_flag(uint8_t t, uint8_t flag) {
    if (t >= TILE_COUNT) return 0;
    return (uint8_t)((tile_flags[t] & flag) != 0u);
}

static void put_stage_tile(uint8_t x, uint8_t y, uint8_t t) {
    uint8_t b;
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = t;
    b = bg_for_tile(t);
    set_bkg_tiles(x, y, 1, 1, &b);
}

static uint8_t is_solid_tile(uint8_t t) {
    if (t == T_TOGGLE) return switch_on;
    return tile_has_flag(t, TILEF_SOLID);
}

static uint8_t is_danger_tile(uint8_t t) {
    return tile_has_flag(t, TILEF_DANGER);
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

static void set_spawn_px(uint8_t x, uint8_t y) {
    player_x = FIX(x);
    player_y = FIX(y);
    player_vx = 0;
    player_vy = FIX(-5);
}

static void add_column(uint8_t x, uint8_t y, uint8_t h, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < h; i++) {
        if (((uint8_t)(y + i) < SCREEN_TILES_H) && (x < SCREEN_TILES_W)) stage[y + i][x] = tile;
    }
}

static void add_coin_line(uint8_t x, uint8_t y, uint8_t w) {
    uint8_t i;
    for (i = 0; i < w; i++) {
        if (((uint8_t)(x + i) < SCREEN_TILES_W) && (y < SCREEN_TILES_H) && (stage[y][x + i] == T_EMPTY)) {
            stage[y][x + i] = T_COIN;
        }
    }
}

static void add_hazard_line(uint8_t x, uint8_t y, uint8_t w, uint8_t tile) {
    uint8_t i;
    for (i = 0; i < w; i++) {
        if (((uint8_t)(x + i) < SCREEN_TILES_W) && (y < SCREEN_TILES_H) && (stage[y][x + i] == T_EMPTY)) {
            stage[y][x + i] = tile;
        }
    }
}

static void place_marker_down(uint8_t x, uint8_t y) {
    uint8_t my;
    if ((x >= SCREEN_TILES_W) || (y <= (uint8_t)(CEILING_Y + 1u))) return;
    my = (uint8_t)(y - 1u);
    if (stage[my][x] == T_EMPTY) stage[my][x] = T_ARROW_D;
}

static void place_exit(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = T_EXIT;
    place_marker_down(x, y);
}

static void place_battery(uint8_t x, uint8_t y) {
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = T_BATTERY;
    place_marker_down(x, y);
}

static void clear_if_danger(uint8_t x, uint8_t y) {
    if (is_danger_tile(stage[y][x])) stage[y][x] = T_EMPTY;
}

static void soften_exit(void) {
    clear_if_danger(15, 16);
    clear_if_danger(16, 16);
    clear_if_danger(17, 16);
    clear_if_danger(16, 15);
    clear_if_danger(17, 15);
    clear_if_danger(18, 15);
    place_marker_down(18, 16);
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
    feedback_kind = FEEDBACK_NONE;
    feedback_timer = 0;
    level_time = 0;
    set_spawn_px(18, 80);
}

static void begin_room(uint8_t world) {
    level_world = world;
    reset_common();
    place_exit(18, 16);
}

static void set_room_switch(uint8_t on) {
    switch_on = on;
}

static void add_moving_platform(uint8_t x, uint8_t y, uint8_t w, int8_t dir) {
    moving_active = 1;
    moving_x = x;
    moving_y = y;
    moving_w = w;
    moving_dir = dir;
    add_platform(moving_x, moving_y, moving_w, T_MOVING);
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

static void build_tutorial_room(uint8_t local) {
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

static void build_intro_room(uint8_t local) {
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

static void build_switch_room(uint8_t local) {
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

static void build_water_room(uint8_t local) {
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

static void build_motion_room(uint8_t local) {
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

static void build_mixed_room(uint8_t local) {
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

static void generate_adventure(uint8_t level) {
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
    soften_exit();
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
    begin_room(4);
    set_spawn_px(18, 92);
    set_room_switch((uint8_t)((depth & 1u) == 0u));
    add_platform(2, 14, 4, T_SOLID);

    for (i = 0; i < 4u; i++) {
        seed = rng_step(seed);
        x = (uint8_t)(5u + (seed % 10u));
        seed = rng_step(seed);
        y = (uint8_t)(6u + (seed % 8u));
        add_platform(x, y, (uint8_t)(3u + (seed % 4u)), (i == 2u) ? T_TOGGLE : ((depth & 3u) == i) ? T_CRACK : T_SOLID);
    }

    place_battery((uint8_t)(5u + (depth % 10u)), (uint8_t)(4u + (depth & 1u)));
    stage[13][4] = T_SWITCH;
    stage[8][15] = (depth & 1u) ? T_FAN_L : T_FAN_R;
    stage[12][6] = T_BUBBLE;
    sprinkle_coins((uint8_t)seed, (uint8_t)(4u + danger));

    for (i = 0; i < (uint8_t)(3u + danger); i++) {
        seed = rng_step(seed);
        x = (uint8_t)(6u + (seed % 10u));
        y = (uint8_t)(16u - (i & 1u));
        if (stage[y][x] == T_EMPTY) stage[y][x] = ((i + depth) & 1u) ? T_SPIKE : T_WATER;
    }
    for (i = 0; i < (uint8_t)(1u + (danger / 2u)); i++) {
        add_enemy((uint8_t)(7u + ((seed + i * 5u) % 8u)), 16, (i & 1u) ? 1 : -1);
    }
    if (danger >= 2u) {
        add_moving_platform((uint8_t)(7u + (depth & 3u)), 7, 3, 1);
    }
}

static uint8_t music_track_for_room(void) {
    if (game_mode == MODE_PANIC) return MUSIC_PANIC;
    if (level_world == 1u) return MUSIC_SWITCH;
    if (level_world == 2u) return MUSIC_WATER;
    if (level_world == 3u) return MUSIC_MOTION;
    if (level_world >= 4u) return MUSIC_MIXED;
    return MUSIC_PLAY;
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
    music_start(music_track_for_room());
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
    if (feedback_timer && (feedback_kind == FEEDBACK_SHIELD)) draw_text(6, 0, "SHIELD");
    else if (feedback_timer && (feedback_kind == FEEDBACK_STOMP)) draw_text(7, 0, "STOMP");
    else if (feedback_timer && (feedback_kind == FEEDBACK_CRACK)) draw_text(7, 0, "CRACK");
    if (game_mode == MODE_ADVENTURE) {
        draw_char(0, HUD_Y, 'A');
        draw_u8_2(1, HUD_Y, (uint8_t)(current_level + 1u));
    } else if (game_mode == MODE_TRIAL) {
        draw_char(0, HUD_Y, 'T');
        draw_u8_2(1, HUD_Y, (uint8_t)(current_level + 1u));
    } else {
        draw_char(0, HUD_Y, 'P');
        if (panic_depth > 99u) draw_u8_2(1, HUD_Y, 99);
        else draw_u8_2(1, HUD_Y, (uint8_t)panic_depth);
    }
    if (battery) draw_text(4, HUD_Y, "DOOR");
    else draw_text(4, HUD_Y, "BAT");
    draw_char(9, HUD_Y, 'C');
    draw_u8_2(10, HUD_Y, (coins > 99u) ? 99u : coins);
    draw_char(13, HUD_Y, 'T');
    draw_time(14, HUD_Y, level_time);
    if (bubble) draw_char(18, HUD_Y, 'B');
    else if (invuln) draw_char(18, HUD_Y, '!');
}

static void mark_hud_dirty(void) {
    hud_dirty = 1;
}

static void start_feedback(uint8_t kind, uint8_t timer) {
    feedback_kind = kind;
    feedback_timer = timer;
    hud_dirty = 1;
}

static void emit_game_event(uint8_t event) {
    music_sfx_event(event);
    if (event == EVENT_COIN) {
        coins++;
        mark_hud_dirty();
    } else if (event == EVENT_BATTERY) {
        if (!battery) battery = 1;
        mark_hud_dirty();
    } else if (event == EVENT_BUBBLE) {
        bubble = 1;
        mark_hud_dirty();
    } else if (event == EVENT_SHIELD) {
        bubble = 0;
        invuln = 70;
        player_vy = FIX(-5);
        start_feedback(FEEDBACK_SHIELD, 36);
    } else if (event == EVENT_STOMP) {
        coins++;
        mark_hud_dirty();
        if (!feedback_timer) start_feedback(FEEDBACK_STOMP, 18);
    } else if (event == EVENT_CRACK) {
        if (!feedback_timer) start_feedback(FEEDBACK_CRACK, 18);
    } else if (event == EVENT_SWITCH) {
        mark_hud_dirty();
    }
}

static void present_frame(void) {
    uint8_t second;
    vsync();
    music_update();
    if (state != STATE_PLAY) return;

    second = (uint8_t)(level_time / 60u);
    if (second != hud_second) {
        hud_second = second;
        hud_dirty = 1;
    }
    if (feedback_timer) hud_dirty = 1;
    if (hud_dirty) {
        draw_hud();
        hud_dirty = 0;
    }
    if (feedback_timer) {
        feedback_timer--;
        if (feedback_timer == 0u) {
            feedback_kind = FEEDBACK_NONE;
            hud_dirty = 1;
        }
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
    emit_game_event(EVENT_SWITCH);
}

static void hurt_player(void) {
    if (invuln) return;
    if (bubble) {
        emit_game_event(EVENT_SHIELD);
        return;
    }
    state = STATE_DEAD;
    music_start(MUSIC_DEAD);
    dead_wait = 24;
    hide_sprites();
    fill_screen(T_EMPTY);
    draw_text(5, 5, "POGO PANIC");
    draw_text(4, 8, "A TRY AGAIN");
    draw_text(5, 10, "B MENU");
}

static void complete_level(void) {
    progress_record_clear();
    state = STATE_CLEAR;
    music_start(MUSIC_CLEAR);
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

static uint8_t player_overlaps_exit(void) {
    return (uint8_t)(player_center_tile() == T_EXIT);
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
                emit_game_event(EVENT_COIN);
                put_stage_tile(x, y, T_EMPTY);
            } else if (t == T_BATTERY) {
                emit_game_event(EVENT_BATTERY);
                put_stage_tile(x, y, T_EMPTY);
            } else if (t == T_BUBBLE) {
                emit_game_event(EVENT_BUBBLE);
                put_stage_tile(x, y, T_EMPTY);
            } else if (is_danger_tile(t)) {
                hurt_player();
                return;
            }
        }
    }
    if (battery && player_overlaps_exit()) {
        complete_level();
        return;
    }
}

static void do_bounce(uint8_t tile, uint8_t joy) {
    if (tile == T_SWITCH) trigger_switch();
    if (tile == T_SPRING) {
        player_vy = (joy & J_B) ? BOUNCE_SPRING_SHORT : BOUNCE_SPRING;
    } else {
        player_vy = (joy & J_B) ? BOUNCE_SHORT : BOUNCE_NORMAL;
    }
    if (tile == T_CONV_L) player_vx -= FIX(1);
    if (tile == T_CONV_R) player_vx += FIX(1);
    if ((joy & J_B) && (joy & J_LEFT)) player_vx -= PLAYER_AIR_KICK;
    if ((joy & J_B) && (joy & J_RIGHT)) player_vx += PLAYER_AIR_KICK;
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
            emit_game_event(EVENT_CRACK);
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
                player_vy = BOUNCE_NORMAL;
                stomping = 0;
                emit_game_event(EVENT_STOMP);
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

static void apply_player_input(uint8_t joy) {
    if (joy & J_LEFT) player_vx -= (joy & J_B) ? PLAYER_FAST_ACCEL : PLAYER_ACCEL;
    if (joy & J_RIGHT) player_vx += (joy & J_B) ? PLAYER_FAST_ACCEL : PLAYER_ACCEL;
    if (!(joy & (J_LEFT | J_RIGHT))) {
        if (player_vx > 0) player_vx--;
        else if (player_vx < 0) player_vx++;
    }
    if (player_vx > PLAYER_MAX_VX) player_vx = PLAYER_MAX_VX;
    if (player_vx < -PLAYER_MAX_VX) player_vx = -PLAYER_MAX_VX;

    if ((joy & J_A) && (player_vy > FIX(-2))) {
        player_vy = FIX(5);
        stomping = 1;
    }
}

static void apply_player_world_forces(void) {
    uint8_t center_tile;
    int16_t gravity;
    center_tile = player_center_tile();
    if (center_tile == T_FAN_L) player_vx -= 3;
    if (center_tile == T_FAN_R) player_vx += 3;
    gravity = (level_world == 2u) ? 3 : 4;
    if (stomping) gravity = 6;
    player_vy = (int16_t)(player_vy + gravity);
    if (player_vy > FIX(6)) player_vy = FIX(6);
}

static void move_player_and_resolve(uint8_t joy) {
    player_x = (int16_t)(player_x + player_vx);
    resolve_horizontal();
    player_y = (int16_t)(player_y + player_vy);
    resolve_vertical(joy);
}

static void update_player(uint8_t joy) {
    if (invuln) invuln--;
    if (switch_cooldown) switch_cooldown--;

    apply_player_input(joy);
    apply_player_world_forces();
    move_player_and_resolve(joy);

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
        move_sprite(1, 0, 0);
    }
    if (bubble) move_sprite(10, px, py);
    else move_sprite(10, 0, 0);
}

static void start_selected(void) {
    progress_prepare_selected_level();
    load_level();
}

static void draw_title_demo(void) {
    uint8_t phase;
    uint8_t bounce;
    uint8_t px;
    uint8_t py;
    phase = (uint8_t)(menu_tick & 63u);
    if (phase < 32u) px = (uint8_t)(64u + phase);
    else px = (uint8_t)(127u - phase);
    bounce = (uint8_t)(phase & 15u);
    if (bounce > 7u) bounce = (uint8_t)(15u - bounce);
    py = (uint8_t)(78u - bounce);
    move_sprite(0, px, py);
    move_sprite(1, 0, 0);
}

static void show_title(void) {
    hide_sprites();
    music_start(MUSIC_TITLE);
    menu_tick = 0;
    fill_screen(T_EMPTY);
    draw_menu_rule(0);
    draw_menu_rule(17);
    draw_text(2, 2, "POCKET POGO");
    draw_text(5, 4, "PANIC");
    draw_text(2, 6, "STOMP");
    draw_tile_icon(8, 6, T_BATTERY);
    draw_tile_icon(10, 6, T_ARROW_R);
    draw_tile_icon(12, 6, T_EXIT);
    draw_text(14, 6, "DOOR");
    draw_tile_run(8, 10, 4, T_SOLID);
    draw_text(5, 12, "A START");
    draw_text(3, 14, "B HOW TO PLAY");
    draw_text(2, 16, "DMG ARCADE ROM");
    draw_title_demo();
}

static uint8_t level_limit_for_mode(void) {
    return progress_level_limit();
}

static void clamp_selected_level(void) {
    uint8_t limit;
    limit = level_limit_for_mode();
    if (limit == 0u) return;
    if (selected_level >= limit) selected_level = (uint8_t)(limit - 1u);
}

static void show_mode_select(void) {
    uint8_t world;
    hide_sprites();
    clamp_selected_level();
    fill_screen(T_EMPTY);
    draw_menu_rule(0);
    draw_menu_rule(17);
    draw_text(4, 1, "SELECT MODE");
    if (game_mode == MODE_ADVENTURE) draw_tile_icon(1, 4, T_ARROW_R);
    draw_text(3, 4, "ADVENTURE");
    if (game_mode == MODE_PANIC) draw_tile_icon(1, 6, T_ARROW_R);
    draw_text(3, 6, "PANIC");
    if (game_mode == MODE_TRIAL) draw_tile_icon(1, 8, T_ARROW_R);
    draw_text(3, 8, "TIME TRIAL");
    if (game_mode == MODE_ADVENTURE) {
        world = (uint8_t)((selected_level >> 4) + 1u);
        draw_text(2, 10, "GET BAT THEN DOOR");
        draw_text(2, 12, "LEVEL");
        draw_u8_2(9, 12, (uint8_t)(selected_level + 1u));
        draw_text(12, 12, "WORLD");
        draw_char(18, 12, (char)('0' + world));
        draw_text(2, 13, "OPEN");
        draw_u8_2(8, 13, save_data.unlocked);
    } else if (game_mode == MODE_PANIC) {
        draw_text(2, 10, "ENDLESS FAST ROOMS");
        draw_text(2, 12, "SAFE START MIX");
        draw_text(2, 13, "BEST DEPTH");
        if (save_data.panic_best > 99u) draw_u8_2(14, 13, 99);
        else draw_u8_2(14, 13, (uint8_t)save_data.panic_best);
    } else {
        world = (uint8_t)((selected_level >> 4) + 1u);
        draw_text(2, 10, "RACE ANY ROOM");
        draw_text(2, 12, "LEVEL");
        draw_u8_2(9, 12, (uint8_t)(selected_level + 1u));
        draw_text(12, 12, "WORLD");
        draw_char(18, 12, (char)('0' + world));
        draw_text(2, 13, "BEST");
        draw_time(8, 13, save_data.best_time[selected_level]);
    }
    if (game_mode != MODE_PANIC) draw_text(1, 14, "U/D MODE L/R LEVEL");
    else draw_text(5, 14, "U/D MODE");
    draw_text(2, 16, "A START B BACK");
}

static void show_howto(void) {
    hide_sprites();
    fill_screen(T_EMPTY);
    draw_menu_rule(0);
    draw_menu_rule(17);
    draw_text(4, 1, "HOW TO PLAY");
    draw_char(16, 1, (char)('1' + how_page));
    draw_char(17, 1, '/');
    draw_char(18, 1, '4');
    if (how_page == 0u) {
        draw_text(1, 4, "LEFT RIGHT STEER");
        draw_text(1, 6, "A STOMP DOWN");
        draw_text(1, 8, "B SHORT HOP");
        draw_text(1, 10, "START PAUSE");
    } else if (how_page == 1u) {
        draw_tile_icon(2, 4, T_BATTERY);
        draw_text(4, 4, "GET BATTERY");
        draw_tile_icon(2, 6, T_EXIT);
        draw_text(4, 6, "THEN ENTER DOOR");
        draw_text(4, 8, "CENTER ON DOOR");
        draw_tile_icon(2, 10, T_COIN);
        draw_text(4, 10, "COINS ARE BONUS");
    } else if (how_page == 2u) {
        draw_tile_icon(2, 4, T_BATTERY);
        draw_text(4, 4, "BATTERY GOAL");
        draw_tile_icon(2, 6, T_BUBBLE);
        draw_text(4, 6, "BUBBLE SHIELD");
        draw_tile_icon(2, 8, T_SPIKE);
        draw_text(4, 8, "SPIKES HURT");
        draw_tile_icon(2, 10, T_WATER);
        draw_text(4, 10, "WATER HURTS");
        move_sprite(4, 24, 112);
        draw_text(4, 12, "SLIME SIDE HURTS");
    } else {
        draw_tile_icon(2, 4, T_SPRING);
        draw_text(4, 4, "SPRINGS BOOST");
        draw_tile_icon(2, 6, T_CRACK);
        draw_text(4, 6, "STOMP CRACKS");
        draw_tile_icon(2, 8, T_SWITCH);
        draw_text(4, 8, "SWITCH PLATFORMS");
        draw_tile_icon(2, 10, T_FAN_R);
        draw_text(4, 10, "FANS PUSH");
        draw_tile_icon(2, 12, T_CONV_R);
        draw_text(4, 12, "CONVEYORS SLIDE");
    }
    if (how_page < 3u) draw_text(2, 16, "A NEXT B BACK");
    else draw_text(2, 16, "A MENU B BACK");
}

static void update_title(uint8_t pressed) {
    menu_tick++;
    draw_title_demo();
    if (pressed & J_B) {
        how_page = 0;
        state = STATE_HOWTO;
        show_howto();
    } else if (pressed & (J_A | J_START)) {
        state = STATE_MODE;
        show_mode_select();
    }
}

static void update_howto(uint8_t pressed) {
    if (pressed & J_B) {
        if (how_page == 0u) {
            state = STATE_TITLE;
            show_title();
        } else {
            how_page--;
            show_howto();
        }
    } else if (pressed & J_LEFT) {
        if (how_page > 0u) {
            how_page--;
            show_howto();
        }
    } else if (pressed & (J_A | J_START | J_RIGHT)) {
        if (how_page < 3u) {
            how_page++;
            show_howto();
        } else {
            state = STATE_MODE;
            show_mode_select();
        }
    }
}

static void update_mode_select(uint8_t pressed) {
    uint8_t limit;
    if (pressed & J_UP) {
        game_mode = (game_mode == 0u) ? MODE_TRIAL : (uint8_t)(game_mode - 1u);
        show_mode_select();
    } else if (pressed & J_DOWN) {
        game_mode = (uint8_t)((game_mode + 1u) % 3u);
        show_mode_select();
    } else if ((game_mode != MODE_PANIC) && (pressed & J_LEFT)) {
        if (selected_level > 0u) selected_level--;
        show_mode_select();
    } else if ((game_mode != MODE_PANIC) && (pressed & J_RIGHT)) {
        limit = level_limit_for_mode();
        if ((uint8_t)(selected_level + 1u) < limit) selected_level++;
        show_mode_select();
    } else if (pressed & J_B) {
        state = STATE_TITLE;
        show_title();
    } else if (pressed & (J_A | J_START)) {
        start_selected();
    }
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
    music_pause(1u);
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
    init_sound();
    save_read();
    show_title();
    while (1) {
        joy = joypad();
        pressed = (uint8_t)(joy & (uint8_t)(~last_joy));
        last_joy = joy;

        if (state == STATE_TITLE) {
            update_title(pressed);
        } else if (state == STATE_HOWTO) {
            update_howto(pressed);
        } else if (state == STATE_MODE) {
            update_mode_select(pressed);
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
                music_pause(0u);
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
