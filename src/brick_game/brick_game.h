#ifndef BRICK_GAME_H
#define BRICK_GAME_H

#define FIELD_N 20
#define FIELD_M 10

#define KEY_R 114
#define KEY_P 112
#define KEY_Q 113
#define KEY_Z 122
#define KEY_I 105

#ifdef scroll
#undef scroll
#endif

#include <stdbool.h>
#include <sys/time.h>

// `GameInfo_t` must be consumable from both C (tetris.c) and C++ (snake.cpp).
//
// - In C builds we expose only the fields needed by the C tetris engine.
// - In C++ builds we extend the struct with snake-specific fields.
#ifdef __cplusplus

#include <deque>
#include <utility>

// For non-WASM builds we may depend on Qt headers (desktop legacy UI).
#ifndef BRICKGAME_WASM
#include <QString>
#endif

enum class Direction { UP, DOWN, LEFT, RIGHT };

struct GameInfo_t {
  int **field;
  int **next;
  int next_size;
  int score;
  int high_score;
  int level;
  int speed;
  int pause;

  // snake-only
  std::deque<std::pair<int, int>> snake;
  std::pair<int, int> food;
  Direction dir;
  bool game_over;
  bool win;
  bool paused;
  bool waiting_for_input;
};

#else  // C

typedef struct GameInfo_t {
  int **field;
  int **next;
  int next_size;
  int score;
  int high_score;
  int level;
  int speed;
  int pause;
} GameInfo_t;

#endif

// Do NOT include game headers here.
// Each game (tetris/snake/...) must include its own header explicitly.

#endif
