#ifndef POGO_GAME_LOGIC_H
#define POGO_GAME_LOGIC_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define NUM_ADVENTURE_LEVELS 80
#define LEVELS_PER_WORLD 16
#define SCREEN_TILES_W 20
#define SCREEN_TILES_H 18
#define FP_SHIFT 4
#define TILE_PX 8
#define PLAYER_W 6
#define PLAYER_H 8
#define MAX_ENEMIES 5
#define CEILING_Y 2
#define SAVE_MAGIC 0x5047u
#define SAVE_VERSION 1u

#define TILEF_SOLID 0x01u
#define TILEF_DANGER 0x02u

#define PLAYER_ACCEL 3
#define PLAYER_FRICTION 2
#define PLAYER_MAX_VX 34
#define PLAYER_AIR_KICK 6

#define BOUNCE_NORMAL ((int16_t)-80)
#define BOUNCE_SHORT ((int16_t)-56)
#define BOUNCE_SPRING ((int16_t)-96)
#define BOUNCE_SPRING_SHORT ((int16_t)-72)

#define COIN_LIMIT 99u

#define FIX(v) ((int16_t)((v) * (1 << FP_SHIFT)))

#ifndef POGO_GAME_LOGIC_CONSTANTS_ONLY

/* ------------------------------------------------------------------ */
/* Enums and structs                                                  */
/* ------------------------------------------------------------------ */

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
    T_TITLE_SHADOW,
    T_TITLE_FILL,
    TILE_COUNT
};

enum GameMode {
    MODE_ADVENTURE = 0,
    MODE_PANIC,
    MODE_TRIAL
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

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */

extern uint8_t stage[SCREEN_TILES_H][SCREEN_TILES_W];
extern Enemy enemies[MAX_ENEMIES];
extern SaveData save_data;

extern uint8_t game_mode;
extern uint8_t selected_level;
extern uint8_t current_level;
extern uint16_t panic_depth;
extern uint8_t level_world;
extern uint8_t coins;
extern uint8_t battery;
extern uint8_t bubble;
extern uint8_t switch_on;
extern uint8_t switch_cooldown;
extern uint16_t level_time;
extern uint8_t enemy_count;

extern uint8_t moving_active;
extern uint8_t moving_x;
extern uint8_t moving_y;
extern uint8_t moving_w;
extern int8_t moving_dir;
extern uint8_t moving_tick;

extern int16_t player_x;
extern int16_t player_y;
extern int16_t player_vx;
extern int16_t player_vy;
extern uint8_t stomping;
extern uint8_t stomp_ready;
extern uint8_t stomp_chain;
extern uint8_t invuln;

extern uint8_t feedback_kind;
extern uint8_t feedback_timer;

/* ------------------------------------------------------------------ */
/* Function prototypes                                                */
/* ------------------------------------------------------------------ */

uint8_t tile_has_flag(uint8_t t, uint8_t flag);
uint8_t is_solid_tile(uint8_t t);
uint8_t is_danger_tile(uint8_t t);
uint8_t bg_for_tile(uint8_t t);
uint8_t stage_tile(uint8_t x, uint8_t y);
uint8_t tile_at_fixed(int16_t fx, int16_t fy);
uint8_t is_stomp_breakable(uint8_t tile);

void add_platform(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void add_enemy(uint8_t tx, uint8_t ty, int8_t vx);
void set_spawn_px(uint8_t x, uint8_t y);
void add_column(uint8_t x, uint8_t y, uint8_t h, uint8_t tile);
void add_coin_line(uint8_t x, uint8_t y, uint8_t w);
void add_hazard_line(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void add_tile_if_empty(uint8_t x, uint8_t y, uint8_t tile);
void add_platform_if_empty(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void add_stomp_route(uint8_t x, uint8_t y, uint8_t w);
void place_marker_down(uint8_t x, uint8_t y);
void place_exit(uint8_t x, uint8_t y);
void place_battery(uint8_t x, uint8_t y);
void clear_if_danger(uint8_t x, uint8_t y);
void soften_exit(void);
void set_room_switch(uint8_t on);
void add_moving_platform(uint8_t x, uint8_t y, uint8_t w, int8_t dir);
void sprinkle_coins(uint8_t seed, uint8_t count);

void reset_common(void);
void begin_room(uint8_t world);

uint16_t rng_step(uint16_t seed);

void enrich_adventure_room(uint8_t world, uint8_t local);
void build_tutorial_room(uint8_t local);
void build_intro_room(uint8_t local);
void build_switch_room(uint8_t local);
void build_water_room(uint8_t local);
void build_motion_room(uint8_t local);
void build_mixed_room(uint8_t local);
void generate_adventure(uint8_t level);
void generate_panic(uint16_t depth);

uint8_t checksum_save(const SaveData *data);
void save_defaults(void);

#endif /* POGO_GAME_LOGIC_CONSTANTS_ONLY */

#endif /* POGO_GAME_LOGIC_H */
