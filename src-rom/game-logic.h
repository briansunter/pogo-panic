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
#define TILEF_MECHANIC 0x04u
#define TILEF_PLATFORM 0x08u
#define TILEF_POCKET_SOLID 0x10u

#define PLAYER_ACCEL 2
#define PLAYER_FRICTION 2
#define PLAYER_MAX_VX 30
#define PLAYER_AIR_KICK 4

#define BOUNCE_NORMAL ((int16_t)-76)
#define BOUNCE_SHORT ((int16_t)-50)
#define BOUNCE_SPRING ((int16_t)-90)
#define BOUNCE_SPRING_SHORT ((int16_t)-64)

#define COIN_LIMIT 99u

#define FIX(v) ((int16_t)((v) * (1 << FP_SHIFT)))

#ifndef POGO_GAME_LOGIC_CONSTANTS_ONLY

/* ------------------------------------------------------------------ */
/* Enums and structs                                                  */
/* ------------------------------------------------------------------ */

/* Canonical tile vocabulary. The enum below, the JSON dump, and the JS
 * TILE map all derive from this list, so a tile is added or reordered in
 * exactly one place. The JSON shipped to the web side embeds these names,
 * so do not re-order existing entries — append new tiles to the end. */
#define TILE_LIST(X) \
    X(EMPTY)        \
    X(SOLID)        \
    X(CRACK)        \
    X(SPRING)       \
    X(SPIKE)        \
    X(COIN)         \
    X(KEY)          \
    X(EXIT)         \
    X(SWITCH)       \
    X(TOGGLE)       \
    X(FAN_L)        \
    X(FAN_R)        \
    X(CONV_L)       \
    X(CONV_R)       \
    X(WATER)        \
    X(BUBBLE)       \
    X(MOVING)       \
    X(ROCK)         \
    X(TOGGLE_OFF)   \
    X(ARROW_D)      \
    X(ARROW_R)      \
    X(WALL)         \
    X(TITLE_SHADOW) \
    X(TITLE_FILL)

enum TileType {
#define TILE_ENUM(name) T_##name,
    TILE_LIST(TILE_ENUM)
#undef TILE_ENUM
    TILE_COUNT
};

enum GameMode {
    MODE_ADVENTURE = 0,
    MODE_PANIC,
    MODE_TRIAL
};

enum FeedbackKind {
    FEEDBACK_NONE = 0,
    FEEDBACK_KEY,
    FEEDBACK_SHIELD,
    FEEDBACK_STOMP,
    FEEDBACK_CRACK
};

enum GameEvent {
    EVENT_COIN = 0,
    EVENT_KEY,
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

/* Per-tile behavior table. One row per TileType, indexed by enum value.
 * conveyor_dx units: raw fixed-point delta added to player_vx (same units as
 * the original literals: fan tiles used +/-3, conveyor tiles used +/-FIX(1)=+/-16).
 * conveyor_dx is consumed in two sites with different cadences:
 *   - apply_player_world_forces() applies it per-frame when player center
 *     is on the tile (this is how fans pushed the player every frame).
 *   - do_bounce() applies it once on landing (this is how conveyors gave
 *     a one-shot horizontal kick on bounce). The two effects do not overlap
 *     because conveyors are solid (player center is above, not in them) and
 *     fans are non-solid (do_bounce is never called with a fan tile). */
typedef struct TileBehavior {
    uint8_t flags;              /* TILEF_SOLID | TILEF_DANGER */
    int16_t bounce_vy;          /* normal bounce vy on landing; 0 if non-bouncy */
    int16_t bounce_vy_short;    /* short-bounce variant when J_B is held; 0 if non-bouncy */
    int8_t  conveyor_dx;        /* horizontal nudge in raw fixed-point; 0 if none */
    uint8_t pickup_event;       /* GameEvent value + 1 fired on player overlap; 0 = none.
                                   The +1 offset is required because EVENT_COIN==0 collides
                                   with the "no event" sentinel. Decode at use site as
                                   (GameEvent)(pickup_event - 1). */
    uint8_t consumed_on_pickup; /* 1 if tile clears to T_EMPTY after pickup */
    uint8_t breakable_by_stomp; /* 1 if a stomp from above breaks this tile */
} TileBehavior;

extern const TileBehavior tile_table[TILE_COUNT];

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */

extern uint8_t stage[SCREEN_TILES_H][SCREEN_TILES_W];
extern Enemy enemies[MAX_ENEMIES];

extern uint8_t game_mode;
extern uint8_t selected_level;
extern uint8_t current_level;
extern uint16_t panic_depth;
extern uint8_t level_world;
extern uint8_t coins;
extern uint8_t key;
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

/* HUD feedback module. Owns feedback_kind and feedback_timer privately.
 *
 * The Module has two non-trivial rules: SHIELD always supersedes; STOMP and
 * CRACK only register if no feedback is currently active. Durations are
 * named below so callers and tests don't repeat magic frame counts. */
#define FEEDBACK_DURATION_SHIELD 36u   /* ~0.6s @ 60fps; longer to read the shield message */
#define FEEDBACK_DURATION_BREAK  18u   /* ~0.3s @ 60fps; brief flash on STOMP/CRACK */

void hud_feedback_on_event(uint8_t event);  /* maps GameEvent to feedback; no-op for events without UI feedback */
void hud_feedback_tick(void);                /* call once per frame; decrements timer, clears kind on expiry */
uint8_t hud_feedback_active(void);           /* 1 if feedback is currently displayed */
uint8_t hud_feedback_kind(void);             /* returns current FeedbackKind, or FEEDBACK_NONE if inactive */
uint8_t hud_feedback_expired_this_tick(void); /* 1 only on the tick that timer reached 0 */

/* ------------------------------------------------------------------ */
/* Function prototypes                                                */
/* ------------------------------------------------------------------ */

uint8_t tile_has_flag(uint8_t t, uint8_t flag);
uint8_t is_solid_tile(uint8_t t);
uint8_t is_danger_tile(uint8_t t);
uint8_t tile_is_mechanic(uint8_t t);
uint8_t tile_is_platform(uint8_t t);
uint8_t tile_is_pocket_solid(uint8_t t);
uint8_t bg_for_tile(uint8_t t);
uint8_t stage_tile(uint8_t x, uint8_t y);
uint8_t tile_at_fixed(int16_t fx, int16_t fy);
uint8_t is_stomp_breakable(uint8_t tile);

/* Pure physics primitives. These convert player/world coords to tile lookups
 * and run AABB checks — no hardware side effects, so the test surface can
 * exercise them directly. The richer step()-shaped Game Simulation seam
 * (move physics + bounce dispatch out of main.c) is left for a future pass;
 * until then main.c's update_player remains the orchestration site. */
uint8_t point_tile_x(int16_t fx);
uint8_t point_tile_y(int16_t fy);
uint8_t player_center_tile(void);
uint8_t player_overlaps_exit(void);
void clamp_player_vx(void);
uint8_t rect_overlap(int16_t ax, int16_t ay, uint8_t aw, uint8_t ah,
                     int16_t bx, int16_t by, uint8_t bw, uint8_t bh);

void add_platform(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void add_enemy(uint8_t tx, uint8_t ty, int8_t vx);
void set_spawn_px(uint8_t x, uint8_t y);
void add_coin_line(uint8_t x, uint8_t y, uint8_t w);
void add_hazard_line(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void add_tile_if_empty(uint8_t x, uint8_t y, uint8_t tile);
void add_platform_if_empty(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void add_stomp_route(uint8_t x, uint8_t y, uint8_t w);
void add_route_step(uint8_t x, uint8_t y, uint8_t w, uint8_t tile);
void place_marker_down(uint8_t x, uint8_t y);
void place_exit(uint8_t x, uint8_t y);
void place_key(uint8_t x, uint8_t y);
void clear_if_danger(uint8_t x, uint8_t y);
void soften_exit(void);
void unpin_goal_tiles(void);
void fill_one_tile_pockets(void);
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

/* Save data API. The SaveData global lives inside game-logic.c; callers must
 * use these functions rather than reaching for it directly. Each mutation
 * recomputes the checksum so callers can't forget. */
uint8_t  save_unlocked_count(void);
uint16_t save_best_time(uint8_t level);    /* returns 0xffffu for invalid level */
uint16_t save_panic_best(void);

void save_record_clear(uint8_t level, uint16_t time);  /* monotonic: only overwrites if time < current best */
void save_unlock_through(uint8_t level);                /* unlocks level+1 if not already unlocked (no-op past last level) */
void save_record_panic(uint16_t depth);                /* monotonic: only overwrites if depth > current best */

uint8_t save_validate(void); /* 1 if magic/version/checksum/unlocked all OK; 0 otherwise. */

/* Raw blob access -- ONLY for SRAM persistence in main.c. No other caller
 * should touch the bytes directly. */
void save_data_blob(uint8_t **out_ptr, uint16_t *out_len);

#endif /* POGO_GAME_LOGIC_CONSTANTS_ONLY */

#endif /* POGO_GAME_LOGIC_H */
