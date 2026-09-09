// game_structures.h (for legacy desktop build)
#ifndef GAME_STRUCTURES_H
#define GAME_STRUCTURES_H

// Структура для части змейки
struct SnakePart {
  int x;  // Координата по оси X
  int y;  // Координата по оси Y
};

// Структура для точки (например, для еды в змейке)
struct Point {
  int x;  // Координата по оси X
  int y;  // Координата по оси Y
};

// Desktop UI has its own lightweight Tetris state.
// Name must not conflict with core `tetris/tetris.h`'s `GameState_t`.
struct DesktopGameState_t {
  int **field;
  int **figure;
  int figure_size;
  int **next_figure;
  int next_figure_size;
  int x;
  int y;
  int score;
  int high_score;
  int level;
  int speed;
  bool pause;
  bool is_playing;
  bool win;
};

#endif  // GAME_STRUCTURES_H
