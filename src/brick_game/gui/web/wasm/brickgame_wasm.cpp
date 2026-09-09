// WebAssembly bridge for BrickGame legacy libraries (Tetris C, Snake C++).
//
// Build idea (Emscripten):
// - Compile this file + legacy sources into a single wasm module.
// - Export a tiny C ABI for JS:
//     bg_init(gameId)
//     bg_input(actionId, hold)
//     bg_tick()
//     bg_field_ptr() -> uint8_t[200] (0/1)
//     bg_next_ptr()  -> uint8_t[16]  (0/1) + bg_next_size()
//     bg_colors_ptr() -> uint8_t[200] (snake only: 0/1/2; others 0)
//     bg_meta_ptr() -> int32_t[] (score, high, level, speed, pause, game_over, win)
//
// NOTE: This is "engine-only" and intentionally avoids ncurses/Qt.

#define BRICKGAME_WASM 1

#include <stdint.h>
#include <string.h>

// WASM bridge talks to the existing "native_bridge" API (C ABI) that already
// flattens game state and avoids C++ containers in exported interfaces.
#include "../../../brick_game.h"

extern "C" {
typedef struct {
  int field[FIELD_N][FIELD_M];
  int next[FIELD_N][FIELD_M];
  int score;
  int high_score;
  int level;
  int speed;
  int pause;
  int game_over;
} CState;

void tetris_init(void);
void tetris_user_input(int action_id, int hold);
void tetris_update_state(CState *out);

void snake_init(void);
void snake_user_input(int action_id, int hold);
void snake_update_state(CState *out);
}

namespace {

// GameId matches server/web:
// 1=Race (python), 2=Tetris (C), 3=Snake (C++), 4=Arcanoid (python)
static int g_game_id = 0;

// Flat buffers for UI (10x20).
static uint8_t g_field[FIELD_N * FIELD_M];
static uint8_t g_colors[FIELD_N * FIELD_M];
static uint8_t g_next[16];
static int32_t g_meta[8];  // score, high, level, speed, pause, game_over, win, reserved
static int g_next_size = 0;

static CState g_cstate;

static void clear_buffers() {
  memset(g_field, 0, sizeof(g_field));
  memset(g_colors, 0, sizeof(g_colors));
  memset(g_next, 0, sizeof(g_next));
  memset(g_meta, 0, sizeof(g_meta));
  g_next_size = 0;
}

static void fill_from_cstate(int game_id) {
  memset(g_field, 0, sizeof(g_field));
  memset(g_colors, 0, sizeof(g_colors));
  for (int r = 0; r < FIELD_N; r++) {
    for (int c = 0; c < FIELD_M; c++) {
      const int v = g_cstate.field[r][c];
      if (game_id == 3) {
        // Snake: 0 empty, 1 snake, 2 food.
        g_field[r * FIELD_M + c] = (uint8_t)(v ? 1 : 0);
        g_colors[r * FIELD_M + c] = (uint8_t)(v);
      } else {
        // Tetris: treat any non-zero as occupied.
        g_field[r * FIELD_M + c] = (uint8_t)(v ? 1 : 0);
        g_colors[r * FIELD_M + c] = (uint8_t)(v ? 1 : 0);
      }
    }
  }

  // next: expose top-left 4x4 for tetris, none for snake.
  memset(g_next, 0, sizeof(g_next));
  g_next_size = (game_id == 2) ? 4 : 0;
  if (game_id == 2) {
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 4; c++) {
        const int v = g_cstate.next[r][c];
        g_next[r * 4 + c] = (uint8_t)(v ? 1 : 0);
      }
    }
  }

  g_meta[0] = (int32_t)g_cstate.score;
  g_meta[1] = (int32_t)g_cstate.high_score;
  g_meta[2] = (int32_t)g_cstate.level;
  g_meta[3] = (int32_t)g_cstate.speed;
  g_meta[4] = (int32_t)g_cstate.pause;
  g_meta[5] = (int32_t)g_cstate.game_over;
  // win flag is not exposed by this bridge; keep 0.
  g_meta[6] = (int32_t)0;
  g_meta[7] = 0;
}

static void tetris_apply_action(int actionId, int hold) {
  tetris_user_input(actionId, hold);
}

static void snake_apply_action(int actionId, int hold) {
  snake_user_input(actionId, hold);
}

}  // namespace

extern "C" {

// Minimal exported ABI (Emscripten: add to EXPORTED_FUNCTIONS).
int bg_init(int gameId) {
  clear_buffers();
  g_game_id = (int)gameId;

  if (g_game_id == 2) {
    tetris_init();
    memset(&g_cstate, 0, sizeof(g_cstate));
    tetris_update_state(&g_cstate);
    fill_from_cstate(2);
    return 1;
  }
  if (g_game_id == 3) {
    snake_init();
    memset(&g_cstate, 0, sizeof(g_cstate));
    snake_update_state(&g_cstate);
    fill_from_cstate(3);
    return 1;
  }
  return 0;
}

int bg_input(int actionId, int hold) {
  if (g_game_id == 2) {
    tetris_apply_action(actionId, hold);
    return 1;
  }
  if (g_game_id == 3) {
    snake_apply_action(actionId, hold);
    return 1;
  }
  return 0;
}

int bg_tick(void) {
  if (g_game_id == 2) {
    memset(&g_cstate, 0, sizeof(g_cstate));
    tetris_update_state(&g_cstate);
    fill_from_cstate(2);
    return 1;
  }
  if (g_game_id == 3) {
    memset(&g_cstate, 0, sizeof(g_cstate));
    snake_update_state(&g_cstate);
    fill_from_cstate(3);
    return 1;
  }
  return 0;
}

const uint8_t *bg_field_ptr(void) { return g_field; }
const uint8_t *bg_colors_ptr(void) { return g_colors; }
const uint8_t *bg_next_ptr(void) { return g_next; }
int bg_next_size(void) { return g_next_size; }
const int32_t *bg_meta_ptr(void) { return g_meta; }

}  // extern "C"

