#ifndef INTERFACE_H
#define INTERFACE_H

#define GAME_FIELD_N (FIELD_N + 2)
#define GAME_FIELD_M (FIELD_M * 3 + 2)
#define START_MENU_N 12
#define START_MENU_M 24
#define PAUSE_MENU_N 5
#define PAUSE_MENU_M 24
#define FINISH_GAME_N 8
#define FINISH_GAME_M 20
#define GAME_INFO_M 25
#define CONTROLS_M 24
#define INSTRUCTION_MENU_N 20
#define INSTRUCTION_MENU_M 20

#define TOP_MARGIN 0
#define LEFT_MARGIN 0

// Убираем определение макроса scroll, если оно уже существует
#ifdef scroll
#undef scroll
#endif

#include <ncurses.h>

// `interface.h` должен видеть GameInfo_t и константы поля.
// Файл лежит в: src/brick_game/gui/cli/interface.h
#include "../../brick_game.h"

#ifdef __cplusplus
extern "C" {
#endif

// Объявление функций
WINDOW *create_window(int height, int width, int start_y, int start_x,
                      int border_color);
void init_ncurses();
void init_colors();
void print_instruction_menu();
WINDOW *print_game_field(GameInfo_t gi);
void print_pause_menu();
void print_win(GameInfo_t gi);
void print_game_over(GameInfo_t gi);
WINDOW *print_game_info(GameInfo_t gi);

void print_next_figure(WINDOW *info_window, GameInfo_t gi);
void print_start_menu();
WINDOW *print_snake_game_field(GameInfo_t game);
WINDOW *print_snake_game_info(GameInfo_t game);

#ifdef __cplusplus
}
#endif

// C++-функции змейки (реализованы в `snake.cpp`)
#ifdef __cplusplus
void render_snake(GameInfo_t &game);
void play_race_cli();
#endif

#endif
