#include <gb/gb.h>
#include <gb/hardware.h>
#include <stdint.h>
#include <string.h>
#include "game-logic.h"

#define FONT_BASE 24
#define SPRITE_BASE 96
#define FONT_COUNT 43
#define TITLE_FONT_BASE (FONT_BASE + FONT_COUNT)
#define HUD_Y 1
#define TITLE_PRESENTS_END 180u
#define TITLE_INTRO_DONE 216u

#define TILE8(a,b,c,d,e,f,g,h) a,a,b,b,c,c,d,d,e,e,f,f,g,g,h,h
#define TILE2(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p

/* ============================================================ */
/* Music engine                                                 */
/*                                                              */
/* Public surface (used by the rest of main.c):                 */
/*   music_start(track)   - select and start a track            */
/*   music_pause(p)       - pause/resume                        */
/*   music_sfx_event(ev)  - fire a one-shot SFX                 */
/*   music_update()       - call once per frame                 */
/*   init_sound()         - one-time hardware init              */
/*                                                              */
/* Everything else is private. The five state variables         */
/* (music_track, music_step, music_frame, music_paused,         */
/* music_sfx_timer) are static so the rest of main.c cannot     */
/* read or write them directly.                                 */
/* ============================================================ */

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

enum GameState {
    STATE_TITLE = 0,
    STATE_HOWTO,
    STATE_MODE,
    STATE_PLAY,
    STATE_PAUSE,
    STATE_CLEAR,
    STATE_DEAD
};

static const uint8_t bg_tiles[] = {
    TILE2(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), /* empty */
    TILE2(0xff,0x00,0x81,0x7e,0x81,0x66,0x81,0x7e,0x81,0x7e,0xff,0x00,0xff,0xff,0xff,0xff), /* solid */
    TILE2(0xff,0x00,0x81,0x7e,0x99,0x5a,0x8d,0x6e,0x99,0x5a,0x81,0x7e,0xff,0xff,0xff,0xff), /* crack */
    TILE2(0x00,0x00,0x3c,0x3c,0x18,0x18,0x3c,0x3c,0x66,0x66,0x3c,0x3c,0x18,0x18,0x3c,0x3c), /* spring */
    TILE2(0x00,0x00,0x10,0x10,0x10,0x10,0x38,0x38,0x38,0x38,0x7c,0x7c,0x7c,0xfe,0xff,0xff), /* spikes */
    TILE2(0x00,0x00,0x18,0x18,0x24,0x24,0x42,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0x00,0x00), /* coin */
    TILE2(0x70,0x70,0x88,0x88,0x88,0x88,0x70,0x70,0x20,0x20,0x20,0x20,0x38,0x38,0x20,0x20), /* key */
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
    TILE2(0xff,0xff,0x81,0xff,0xbd,0xc3,0x81,0xff,0x81,0xff,0xbd,0xc3,0x81,0xff,0xff,0xff), /* wall */
    TILE2(0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff), /* title shadow */
    TILE2(0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff)  /* title fill */
};

static const uint8_t sprite_tiles[] = {
    TILE2(0x38,0x38,0x38,0x38,0x7c,0x7c,0x38,0x38,0x28,0x28,0x10,0x10,0x38,0x38,0x10,0x10), /* player */
    TILE2(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), /* unused */
    TILE2(0x00,0x00,0x00,0x00,0x3c,0x3c,0x7e,0x7e,0xff,0xff,0xff,0xff,0x7e,0x7e,0x42,0x42), /* slime */
    TILE2(0x18,0x00,0x24,0x18,0x5a,0x3c,0xbd,0x7e,0x7e,0xff,0xbd,0x7e,0x5a,0x3c,0x24,0x18), /* rock */
    TILE2(0x00,0x00,0x3c,0x3c,0x66,0x66,0xc3,0xc3,0xc3,0xc3,0x66,0x66,0x3c,0x3c,0x00,0x00), /* bubble ring */
    TILE8(0x38,0x44,0x44,0x7c,0x44,0x44,0x44,0x00), /* prompt A */
    TILE8(0x3c,0x40,0x40,0x38,0x04,0x04,0x78,0x00), /* prompt S */
    TILE8(0x7c,0x10,0x10,0x10,0x10,0x10,0x10,0x00), /* prompt T */
    TILE8(0x78,0x44,0x44,0x78,0x50,0x48,0x44,0x00)  /* prompt R */
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
    TILE8(0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00), /* bang */
    TILE8(0x3c,0x42,0x9a,0xaa,0xae,0x80,0x7c,0x00)  /* at */
};

static const uint8_t title_font_tiles[] = {
    TILE2(0x00,0x00,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x38,0x3f,0x38,0x3c,0x3f,0x3f,0x3f,0x3f), /* title P TL */
    TILE2(0x00,0x00,0xf8,0xf8,0xfc,0xfc,0xfc,0xfe,0x1c,0xfe,0x1c,0x1e,0xfc,0xfe,0xfc,0xfe), /* title P TR */
    TILE2(0x3f,0x3f,0x38,0x3f,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x00,0x1c), /* title P BL */
    TILE2(0xf8,0xfe,0x00,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00), /* title P BR */
    TILE2(0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x3f,0x3f,0x38,0x3f,0x38,0x3c,0x38,0x3c,0x38,0x3c), /* title O TL */
    TILE2(0x00,0x00,0xf0,0xf0,0xf0,0xf8,0xfc,0xfc,0x1c,0xfe,0x1c,0x1e,0x1c,0x1e,0x1c,0x1e), /* title O TR */
    TILE2(0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x3f,0x3f,0x0f,0x1f,0x0f,0x0f,0x00,0x07), /* title O BL */
    TILE2(0x1c,0x1e,0x1c,0x1e,0x1c,0x1e,0x1c,0x1e,0xfc,0xfe,0xf0,0xfe,0xf0,0xf8,0x00,0xf8), /* title O BR */
    TILE2(0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x3f,0x3f,0x38,0x3f,0x38,0x3c,0x38,0x3c,0x38,0x3c), /* title G TL */
    TILE2(0x00,0x00,0xfc,0xfc,0xfc,0xfe,0xfc,0xfe,0x00,0xfe,0x00,0x00,0x00,0x00,0xfc,0xfc), /* title G TR */
    TILE2(0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x3f,0x3f,0x0f,0x1f,0x0f,0x0f,0x00,0x07), /* title G BL */
    TILE2(0xfc,0xfe,0xfc,0xfe,0x1c,0x7e,0x1c,0x1e,0xfc,0xfe,0xfc,0xfe,0xfc,0xfe,0x00,0xfe), /* title G BR */
    TILE2(0x00,0x00,0x07,0x07,0x0f,0x0f,0x0f,0x0f,0x3c,0x3f,0x38,0x3e,0x38,0x3c,0x3f,0x3f), /* title A TL */
    TILE2(0x00,0x00,0xe0,0xe0,0xf0,0xf0,0xf0,0xf8,0x3c,0xfc,0x1c,0x1e,0x1c,0x1e,0xfc,0xfe), /* title A TR */
    TILE2(0x3f,0x3f,0x3f,0x3f,0x38,0x3f,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x00,0x1c), /* title A BL */
    TILE2(0xfc,0xfe,0xfc,0xfe,0x1c,0xfe,0x1c,0x1e,0x1c,0x1e,0x1c,0x1e,0x1c,0x1e,0x00,0x0e), /* title A BR */
    TILE2(0x00,0x00,0x38,0x38,0x38,0x3c,0x3e,0x3e,0x3e,0x3f,0x3f,0x3f,0x3b,0x3f,0x3b,0x3f), /* title N TL */
    TILE2(0x00,0x00,0x1c,0x1c,0x1c,0x1e,0x1c,0x1e,0x1c,0x1e,0x9c,0x9e,0x9c,0xde,0x9c,0xde), /* title N TR */
    TILE2(0x3b,0x3f,0x38,0x3d,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x00,0x1c), /* title N BL */
    TILE2(0xfc,0xfe,0xfc,0xfe,0xfc,0xfe,0xfc,0xfe,0x7c,0x7e,0x7c,0x7e,0x1c,0x3e,0x00,0x0e), /* title N BR */
    TILE2(0x00,0x00,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x03,0x0f,0x03,0x03,0x03,0x03,0x03,0x03), /* title I TL */
    TILE2(0x00,0x00,0xf8,0xf8,0xf8,0xfc,0xf8,0xfc,0xc0,0xfc,0xc0,0xe0,0xc0,0xe0,0xc0,0xe0), /* title I TR */
    TILE2(0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x00,0x0f), /* title I BL */
    TILE2(0xc0,0xe0,0xc0,0xe0,0xc0,0xe0,0xc0,0xe0,0xf8,0xf8,0xf8,0xfc,0xf8,0xfc,0x00,0xfc), /* title I BR */
    TILE2(0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x3f,0x3f,0x38,0x3f,0x38,0x3c,0x38,0x3c,0x38,0x3c), /* title C TL */
    TILE2(0x00,0x00,0xfc,0xfc,0xfc,0xfe,0xfc,0xfe,0x00,0xfe,0x00,0x00,0x00,0x00,0x00,0x00), /* title C TR */
    TILE2(0x38,0x3c,0x38,0x3c,0x38,0x3c,0x38,0x3c,0x3f,0x3f,0x0f,0x1f,0x0f,0x0f,0x00,0x07), /* title C BL */
    TILE2(0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0xfc,0xfc,0xfe,0xfc,0xfe,0x00,0xfe)  /* title C BR */
};

static uint8_t scratch[SCREEN_TILES_W];

static uint8_t state = STATE_TITLE;
static uint8_t clear_wait = 0;
static uint8_t dead_wait = 0;
static uint8_t last_joy = 0;
static uint8_t hud_dirty = 0;
static uint8_t hud_second = 0xffu;
static uint8_t menu_tick = 0;
static uint8_t title_boot_done = 0;
static uint8_t title_scene = 0xffu;
static uint8_t how_page = 0;

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
    } else if (event == EVENT_KEY) {
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

/* End music engine */

static uint8_t glyph_for(char c) {
    if ((c >= 'A') && (c <= 'Z')) return (uint8_t)(c - 'A');
    if ((c >= 'a') && (c <= 'z')) return (uint8_t)(c - 'a');
    if ((c >= '0') && (c <= '9')) return (uint8_t)(26 + (c - '0'));
    if (c == ':') return 37;
    if (c == '-') return 38;
    if (c == '.') return 39;
    if (c == '/') return 40;
    if (c == '!') return 41;
    if (c == '@') return 42;
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

static void save_write(void) {
    uint8_t *blob;
    uint16_t len;
    save_data_blob(&blob, &len);
    ENABLE_RAM_MBC1;
    memcpy((uint8_t *)0xa000u, blob, len);
    DISABLE_RAM_MBC1;
}

static void save_read(void) {
    uint8_t *blob;
    uint16_t len;
    save_data_blob(&blob, &len);
    ENABLE_RAM_MBC1;
    memcpy(blob, (const uint8_t *)0xa000u, len);
    DISABLE_RAM_MBC1;
    if (!save_validate()) {
        save_defaults();
        save_write();
    }
}

static uint8_t progress_level_limit(void) {
    if (game_mode == MODE_PANIC) return 0;
    if (game_mode == MODE_ADVENTURE) return save_unlocked_count();
    return NUM_ADVENTURE_LEVELS;
}

static void progress_prepare_selected_level(void) {
    if (game_mode == MODE_ADVENTURE) {
        if (selected_level >= save_unlocked_count()) selected_level = (uint8_t)(save_unlocked_count() - 1u);
        current_level = selected_level;
    } else if (game_mode == MODE_TRIAL) {
        current_level = selected_level;
    } else {
        panic_depth = 0;
    }
}

static void progress_record_clear(void) {
    if (game_mode == MODE_PANIC) {
        panic_depth++;
        save_record_panic(panic_depth);
    } else {
        save_record_clear(current_level, level_time);
        if (game_mode == MODE_ADVENTURE) save_unlock_through(current_level);
    }
    save_write();
}

static void put_stage_tile(uint8_t x, uint8_t y, uint8_t t) {
    uint8_t b;
    if ((x >= SCREEN_TILES_W) || (y >= SCREEN_TILES_H)) return;
    stage[y][x] = t;
    b = bg_for_tile(t);
    set_bkg_tiles(x, y, 1, 1, &b);
}

static void redraw_stage(void) {
    uint8_t x;
    uint8_t y;
    for (y = 0; y < SCREEN_TILES_H; y++) {
        for (x = 0; x < SCREEN_TILES_W; x++) scratch[x] = bg_for_tile(stage[y][x]);
        set_bkg_tiles(0, y, SCREEN_TILES_W, 1, scratch);
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

static void reset_video_fx(void) {
    SCX_REG = 0;
    SCY_REG = 0;
    BGP_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
    OBP0_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
}

static void title_dark_palette(void) {
    BGP_REG = DMG_PALETTE(DMG_BLACK, DMG_DARK_GRAY, DMG_LITE_GRAY, DMG_WHITE);
    OBP0_REG = DMG_PALETTE(DMG_BLACK, DMG_DARK_GRAY, DMG_LITE_GRAY, DMG_WHITE);
}

static void load_level(void) {
    reset_video_fx();
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
    if (hud_feedback_active()) {
        uint8_t k = hud_feedback_kind();
        if (k == FEEDBACK_SHIELD) draw_text(6, 0, "SHIELD");
        else if (k == FEEDBACK_STOMP) draw_text(7, 0, "STOMP");
        else if (k == FEEDBACK_CRACK) draw_text(7, 0, "CRACK");
    }
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
    if (key) draw_tile_icon(4, HUD_Y, T_KEY);
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

static void add_coin_bonus(uint8_t amount) {
    if (amount == 0u) return;
    if (coins >= COIN_LIMIT) {
        coins = COIN_LIMIT;
    } else if (amount > (uint8_t)(COIN_LIMIT - coins)) {
        coins = COIN_LIMIT;
    } else {
        coins = (uint8_t)(coins + amount);
    }
    mark_hud_dirty();
}

static void emit_game_event(uint8_t event) {
    music_sfx_event(event);
    hud_feedback_on_event(event);
    if (event == EVENT_COIN) {
        add_coin_bonus(1u);
    } else if (event == EVENT_KEY) {
        if (!key) key = 1;
        mark_hud_dirty();
    } else if (event == EVENT_BUBBLE) {
        bubble = 1;
        mark_hud_dirty();
    } else if (event == EVENT_SHIELD) {
        bubble = 0;
        invuln = 70;
        player_vy = FIX(-5);
        mark_hud_dirty();
    } else if (event == EVENT_STOMP) {
        add_coin_bonus(stomp_chain ? stomp_chain : 1u);
        mark_hud_dirty();
    } else if (event == EVENT_CRACK) {
        mark_hud_dirty();
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
    if (hud_feedback_active()) hud_dirty = 1;
    if (hud_dirty) {
        draw_hud();
        hud_dirty = 0;
    }
    hud_feedback_tick();
    if (hud_feedback_expired_this_tick()) hud_dirty = 1;
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
    stomp_chain = 0;
    if (bubble) {
        emit_game_event(EVENT_SHIELD);
        return;
    }
    state = STATE_DEAD;
    music_start(MUSIC_DEAD);
    dead_wait = 24;
    reset_video_fx();
    hide_sprites();
    fill_screen(T_EMPTY);
    draw_text(5, 5, "POGO PANIC");
    draw_text(4, 8, "A TRY AGAIN");
    draw_text(5, 10, "B MENU");
}

static void complete_level(void) {
    progress_record_clear();
    stomp_chain = 0;
    state = STATE_CLEAR;
    music_start(MUSIC_CLEAR);
    clear_wait = 24;
    reset_video_fx();
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
        draw_time(10, 10, save_best_time(current_level));
    }
    draw_text(7, 13, "A NEXT");
}

static uint8_t player_center_tile(void) {
    return tile_at_fixed((int16_t)(player_x + FIX(PLAYER_W / 2)), (int16_t)(player_y + FIX(PLAYER_H / 2)));
}

static uint8_t player_overlaps_exit(void) {
    return (uint8_t)(player_center_tile() == T_EXIT);
}

static void clamp_player_vx(void) {
    if (player_vx > PLAYER_MAX_VX) player_vx = PLAYER_MAX_VX;
    if (player_vx < -PLAYER_MAX_VX) player_vx = -PLAYER_MAX_VX;
}

static void collect_tiles(void) {
    uint8_t tx1;
    uint8_t tx2;
    uint8_t ty1;
    uint8_t ty2;
    uint8_t x;
    uint8_t y;
    uint8_t t;
    const TileBehavior *b;
    tx1 = point_tile_x(player_x);
    tx2 = point_tile_x((int16_t)(player_x + FIX(PLAYER_W - 1)));
    ty1 = point_tile_y(player_y);
    ty2 = point_tile_y((int16_t)(player_y + FIX(PLAYER_H - 1)));
    for (y = ty1; y <= ty2; y++) {
        for (x = tx1; x <= tx2; x++) {
            t = stage_tile(x, y);
            b = &tile_table[t];
            if (b->pickup_event) {
                emit_game_event((uint8_t)(b->pickup_event - 1u));
                if (b->consumed_on_pickup) put_stage_tile(x, y, T_EMPTY);
            } else if (is_danger_tile(t)) {
                hurt_player();
                return;
            }
        }
    }
    if (key && player_overlaps_exit()) {
        complete_level();
        return;
    }
}

static void do_bounce(uint8_t tile, uint8_t joy) {
    const TileBehavior *b = &tile_table[tile];
    int16_t vy;
    /* T_SWITCH is the one tile whose landing fires a side effect that the
     * table cannot represent (it mutates global switch state). Keep it
     * explicit; encoding it as a pickup_event would change semantics
     * because trigger_switch is a switch-tile-only side effect, not an
     * overlap-style pickup. */
    if (tile == T_SWITCH) trigger_switch();
    if (b->bounce_vy == 0) return;
    vy = (joy & J_B) ? b->bounce_vy_short : b->bounce_vy;
    player_vy = vy;
    /* Conveyor tiles encode their per-bounce horizontal kick in conveyor_dx
     * (FIX(1) magnitude). Non-conveyor solid tiles have conveyor_dx == 0. */
    player_vx += b->conveyor_dx;
    if (joy & J_LEFT) player_vx -= PLAYER_AIR_KICK;
    if (joy & J_RIGHT) player_vx += PLAYER_AIR_KICK;
    clamp_player_vx();
    stomping = 0;
    stomp_ready = 1;
    stomp_chain = 0;
}

static void break_stomp_block_at(uint8_t tx, uint8_t ty) {
    if (is_stomp_breakable(stage_tile(tx, ty))) put_stage_tile(tx, ty, T_EMPTY);
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
        if (stomping && is_stomp_breakable(tile_l)) {
            break_stomp_block_at(tx_l, ty);
            broke = 1;
        }
        if (stomping && is_stomp_breakable(tile_r)) {
            break_stomp_block_at(tx_r, ty);
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
                if (stomp_chain < 9u) stomp_chain++;
                player_vy = (stomp_chain >= 2u) ? BOUNCE_SPRING : BOUNCE_NORMAL;
                stomping = 0;
                stomp_ready = 1;
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

static void apply_player_input(uint8_t joy, uint8_t pressed) {
    if (joy & J_LEFT) player_vx -= PLAYER_ACCEL;
    if (joy & J_RIGHT) player_vx += PLAYER_ACCEL;
    if (!(joy & (J_LEFT | J_RIGHT))) {
        if (player_vx > PLAYER_FRICTION) player_vx -= PLAYER_FRICTION;
        else if (player_vx < -PLAYER_FRICTION) player_vx += PLAYER_FRICTION;
        else player_vx = 0;
    }
    clamp_player_vx();

    if ((pressed & J_A) && stomp_ready && (player_vy > FIX(-2))) {
        player_vy = FIX(5);
        stomping = 1;
        stomp_ready = 0;
    }
}

static void apply_player_world_forces(void) {
    uint8_t center_tile;
    int16_t gravity;
    center_tile = player_center_tile();
    /* Per-frame horizontal nudge from the tile the player center is in.
     * In practice this fires for fan tiles (non-solid, so the player can
     * occupy them) with magnitude +/-3. Conveyors are solid, so the player
     * center is never inside one — their conveyor_dx is consumed by
     * do_bounce instead. See TileBehavior in game-logic.h. */
    player_vx += tile_table[center_tile].conveyor_dx;
    clamp_player_vx();
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

static void update_player(uint8_t joy, uint8_t pressed) {
    if (invuln) invuln--;
    if (switch_cooldown) switch_cooldown--;

    apply_player_input(joy, pressed);
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
        move_sprite(1, px, (uint8_t)(py + 8u));
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

static void draw_big_title_char(uint8_t x, uint8_t y, char c, uint8_t tile) {
    const char *rows;
    uint8_t row;
    uint8_t col;
    rows = "   "
           "   "
           "   "
           "   ";
    if (c == 'P') {
        rows = "## "
               "# #"
               "## "
               "#  ";
    } else if (c == 'O') {
        rows = "###"
               "# #"
               "# #"
               "###";
    } else if (c == 'G') {
        rows = "###"
               "#  "
               "# #"
               "###";
    } else if (c == 'A') {
        rows = " # "
               "###"
               "# #"
               "# #";
    } else if (c == 'N') {
        rows = "# #"
               "###"
               "###"
               "# #";
    } else if (c == 'I') {
        rows = "###"
               " # "
               " # "
               "###";
    } else if (c == 'C') {
        rows = "###"
               "#  "
               "#  "
               "###";
    }

    for (row = 0; row < 4u; row++) {
        for (col = 0; col < 3u; col++) {
            if (rows[(uint8_t)(row * 3u + col)] != ' ') draw_tile_icon((uint8_t)(x + col), (uint8_t)(y + row), tile);
        }
    }
}

static void draw_big_title_word(uint8_t x, uint8_t y, const char *text, uint8_t tile) {
    while ((*text) && (x < SCREEN_TILES_W)) {
        if (*text == ' ') x = (uint8_t)(x + 2u);
        else {
            draw_big_title_char(x, y, *text, tile);
            x = (uint8_t)(x + 4u);
        }
        text++;
    }
}

static void draw_big_title_word_tight(uint8_t x, uint8_t y, const char *text, uint8_t tile) {
    while ((*text) && (x < SCREEN_TILES_W)) {
        if (*text == ' ') x = (uint8_t)(x + 1u);
        else {
            draw_big_title_char(x, y, *text, tile);
            x = (uint8_t)(x + 3u);
        }
        text++;
    }
}

static void draw_title_sign_frame(void) {
    uint8_t y;
    draw_tile_run(1, 2, 18, T_TITLE_FILL);
    draw_tile_run(1, 10, 18, T_TITLE_FILL);
    for (y = 3; y < 10u; y++) {
        draw_tile_icon(1, y, T_TITLE_FILL);
        draw_tile_icon(18, y, T_TITLE_FILL);
    }
}

static void draw_title_mascot(void) {
    uint8_t bob;
    bob = (menu_tick & 16u) ? 0u : 2u;
    move_sprite(0, 31, (uint8_t)(36u - bob));
    move_sprite(1, 0, 0);
}

static void hide_title_prompt(void) {
    move_sprite(2, 0, 0);
    move_sprite(3, 0, 0);
    move_sprite(9, 0, 0);
    move_sprite(11, 0, 0);
    move_sprite(12, 0, 0);
    move_sprite(13, 0, 0);
}

static uint8_t title_prompt_tile(char c) {
    if (c == 'A') return (uint8_t)(SPRITE_BASE + 5u);
    if (c == 'S') return (uint8_t)(SPRITE_BASE + 6u);
    if (c == 'T') return (uint8_t)(SPRITE_BASE + 7u);
    if (c == 'R') return (uint8_t)(SPRITE_BASE + 8u);
    return (uint8_t)(SPRITE_BASE + 5u);
}

static void draw_title_prompt_sprite(uint8_t sprite, uint8_t tile_x, char c) {
    set_sprite_tile(sprite, title_prompt_tile(c));
    move_sprite(sprite, (uint8_t)((tile_x << 3u) + 8u), 116);
}

static void draw_title_prompt(void) {
    draw_title_prompt_sprite(2, 6, 'A');
    draw_title_prompt_sprite(3, 8, 'S');
    draw_title_prompt_sprite(9, 9, 'T');
    draw_title_prompt_sprite(11, 10, 'A');
    draw_title_prompt_sprite(12, 11, 'R');
    draw_title_prompt_sprite(13, 12, 'T');
}

static uint8_t title_glyph_index(char c) {
    if (c == 'P') return 0;
    if (c == 'O') return 1;
    if (c == 'G') return 2;
    if (c == 'A') return 3;
    if (c == 'N') return 4;
    if (c == 'I') return 5;
    if (c == 'C') return 6;
    return 0;
}

static void draw_title_glyph(uint8_t x, uint8_t y, char c) {
    uint8_t tile;
    tile = (uint8_t)(TITLE_FONT_BASE + (uint8_t)(title_glyph_index(c) * 4u));
    scratch[0] = tile;
    scratch[1] = (uint8_t)(tile + 1u);
    set_bkg_tiles(x, y, 2, 1, scratch);
    scratch[0] = (uint8_t)(tile + 2u);
    scratch[1] = (uint8_t)(tile + 3u);
    set_bkg_tiles(x, (uint8_t)(y + 1u), 2, 1, scratch);
}

static void draw_title_word(uint8_t x, uint8_t y, const char *text) {
    while ((*text) && (x < SCREEN_TILES_W)) {
        if (*text == ' ') x = (uint8_t)(x + 1u);
        else {
            draw_title_glyph(x, y, *text);
            x = (uint8_t)(x + 3u);
        }
        text++;
    }
}

static void draw_title_ready(void);

static void draw_title_intro_scene(uint8_t scene) {
    SCX_REG = 0;
    SCY_REG = 0;
    hide_sprites();
    fill_screen(T_EMPTY);

    if (scene == 0u) {
        title_dark_palette();
        draw_tile_run(4, 6, 12, T_TOGGLE_OFF);
        draw_text(1, 8, "@bsunter PRESENTS");
        draw_tile_run(4, 10, 12, T_TOGGLE_OFF);
    } else {
        reset_video_fx();
        draw_title_ready();
        hide_title_prompt();
    }
}

static void draw_title_intro(void) {
    uint8_t scene;
    uint8_t phase;
    uint8_t bounce;
    uint8_t py;
    uint8_t shine;

    if (menu_tick < TITLE_PRESENTS_END) scene = 0u;
    else scene = 1u;

    if (scene != title_scene) {
        title_scene = scene;
        draw_title_intro_scene(scene);
    }

    if (scene == 0u) {
        phase = menu_tick;
        shine = (uint8_t)((phase >> 3u) % 12u);
        draw_tile_run(4, 6, 12, T_TOGGLE_OFF);
        draw_tile_run(4, 10, 12, T_TOGGLE_OFF);
        draw_tile_icon((uint8_t)(4u + shine), 6, T_TITLE_FILL);
        draw_tile_icon((uint8_t)(15u - shine), 10, T_TITLE_FILL);
        bounce = (uint8_t)(phase & 31u);
        if (bounce > 15u) bounce = (uint8_t)(31u - bounce);
        py = (uint8_t)(114u - (uint8_t)(bounce >> 2u));
        set_sprite_tile(0, SPRITE_BASE);
        move_sprite(0, 84, py);
        move_sprite(1, 0, 0);
    } else {
        reset_video_fx();
        hide_title_prompt();
    }
}

static void draw_title_ready(void) {
    reset_video_fx();
    fill_screen(T_EMPTY);
    draw_menu_rule(0);
    draw_menu_rule(17);

    draw_title_sign_frame();
    draw_title_word(4, 4, "POGO");
    draw_title_word(3, 7, "PANIC");
    draw_tile_run(0, 11, 20, T_TOGGLE_OFF);

    draw_text(3, 14, "B HOW TO PLAY");
    draw_text(2, 16, "MADE BY @bsunter");
    hide_title_prompt();
}

static void draw_title_ready_anim(void) {
    if (menu_tick & 32u) draw_title_prompt();
    else hide_title_prompt();
}

static void show_title(void) {
    hide_sprites();
    music_start(MUSIC_TITLE);
    menu_tick = 0;
    title_boot_done = 0;
    title_scene = 0xffu;
    draw_title_intro();
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
    reset_video_fx();
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
        draw_text(2, 10, "GET KEY THEN DOOR");
        draw_text(2, 12, "LEVEL");
        draw_u8_2(9, 12, (uint8_t)(selected_level + 1u));
        draw_text(12, 12, "WORLD");
        draw_char(18, 12, (char)('0' + world));
        draw_text(2, 13, "OPEN");
        draw_u8_2(8, 13, save_unlocked_count());
    } else if (game_mode == MODE_PANIC) {
        draw_text(2, 10, "ENDLESS FAST ROOMS");
        draw_text(2, 12, "SAFE START MIX");
        draw_text(2, 13, "BEST DEPTH");
        if (save_panic_best() > 99u) draw_u8_2(14, 13, 99);
        else draw_u8_2(14, 13, (uint8_t)save_panic_best());
    } else {
        world = (uint8_t)((selected_level >> 4) + 1u);
        draw_text(2, 10, "RACE ANY ROOM");
        draw_text(2, 12, "LEVEL");
        draw_u8_2(9, 12, (uint8_t)(selected_level + 1u));
        draw_text(12, 12, "WORLD");
        draw_char(18, 12, (char)('0' + world));
        draw_text(2, 13, "BEST");
        draw_time(8, 13, save_best_time(selected_level));
    }
    if (game_mode != MODE_PANIC) draw_text(1, 14, "U/D MODE L/R LEVEL");
    else draw_text(5, 14, "U/D MODE");
    draw_text(2, 16, "A START B BACK");
}

static void show_howto(void) {
    reset_video_fx();
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
        draw_tile_icon(2, 4, T_KEY);
        draw_text(4, 4, "GET KEY");
        draw_tile_icon(2, 6, T_EXIT);
        draw_text(4, 6, "THEN ENTER DOOR");
        draw_text(4, 8, "CENTER ON DOOR");
        draw_tile_icon(2, 10, T_COIN);
        draw_text(4, 10, "COINS ARE BONUS");
    } else if (how_page == 2u) {
        draw_tile_icon(2, 4, T_KEY);
        draw_text(4, 4, "KEY GOAL");
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
        draw_text(4, 6, "STOMP BREAKS");
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
    if (!title_boot_done) {
        draw_title_intro();
        if (menu_tick >= TITLE_INTRO_DONE) {
            title_boot_done = 1;
            menu_tick = 0;
            title_scene = 0xffu;
            draw_title_ready();
        }
    } else {
        draw_title_ready_anim();
    }
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
    if (pressed & (J_A | J_START)) {
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
    reset_video_fx();
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
    set_bkg_data(TITLE_FONT_BASE, (uint8_t)(sizeof(title_font_tiles) / 16u), title_font_tiles);
    set_sprite_data(SPRITE_BASE, (uint8_t)(sizeof(sprite_tiles) / 16u), sprite_tiles);
    set_sprite_tile(0, SPRITE_BASE);
    set_sprite_tile(1, (uint8_t)(SPRITE_BASE + 1u));
    set_sprite_tile(4, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(5, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(6, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(7, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(8, (uint8_t)(SPRITE_BASE + 2u));
    set_sprite_tile(10, (uint8_t)(SPRITE_BASE + 4u));
    reset_video_fx();
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
                update_player(joy, pressed);
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
