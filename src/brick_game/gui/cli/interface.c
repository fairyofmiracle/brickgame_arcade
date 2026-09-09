#include "interface.h"

#include <stdlib.h>

void init_ncurses() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, 1);
  nodelay(stdscr, 1);
  scrollok(stdscr, 1);
  curs_set(0);
  mouseinterval(1);
}

void init_colors() {
  start_color();
  init_pair(1, COLOR_WHITE, COLOR_BLACK);
  init_pair(2, COLOR_WHITE, COLOR_GREEN);
  init_pair(3, COLOR_WHITE, COLOR_MAGENTA);
  init_pair(4, COLOR_WHITE, COLOR_RED);
  init_pair(5, COLOR_CYAN, COLOR_CYAN);
  init_pair(6, COLOR_GREEN, COLOR_BLACK);
  init_pair(7, COLOR_RED, COLOR_RED);
  init_pair(8, COLOR_YELLOW, COLOR_YELLOW);
  init_pair(9, COLOR_MAGENTA, COLOR_MAGENTA);
  init_pair(10, COLOR_GREEN, COLOR_GREEN);  // Цвет для змеи
  init_pair(11, COLOR_RED, COLOR_RED);      // Цвет для еды
  init_pair(12, COLOR_WHITE, COLOR_BLACK);
}

WINDOW *create_window(int height, int width, int start_y, int start_x,
                      int border_color) {
  WINDOW *win = newwin(height, width, start_y, start_x);
  if (win == NULL) {
    perror("Failed to create window");
    exit(EXIT_FAILURE);
  }
  box(win, 0, 0);
  wbkgd(win, COLOR_PAIR(border_color));
  return win;
}

void print_pause_menu() {
  int pause_y = (GAME_FIELD_N - PAUSE_MENU_N) / 2;
  int pause_x = (GAME_FIELD_M - PAUSE_MENU_M) / 2 + 1;

  WINDOW *pause_menu_window =
      newwin(PAUSE_MENU_N, PAUSE_MENU_M, pause_y, pause_x);
  box(pause_menu_window, 0, 0);
  wbkgd(pause_menu_window, COLOR_PAIR(3));

  mvwhline(pause_menu_window, 1, 2, ACS_HLINE, PAUSE_MENU_M - 4);
  mvwhline(pause_menu_window, PAUSE_MENU_N - 2, 2, ACS_HLINE, PAUSE_MENU_M - 4);
  mvwvline(pause_menu_window, 2, 2, ACS_VLINE, PAUSE_MENU_N - 4);
  mvwvline(pause_menu_window, 2, PAUSE_MENU_M - 3, ACS_VLINE, PAUSE_MENU_N - 4);

  int text_width = 6;
  int text_height = 1;

  mvwprintw(pause_menu_window, (PAUSE_MENU_N - text_height) / 2,
            (PAUSE_MENU_M - text_width) / 2, "Paused");

  wrefresh(pause_menu_window);
  delwin(pause_menu_window);
}

void print_win(GameInfo_t gi) {
  int win_y = (GAME_FIELD_N - FINISH_GAME_N) / 2;
  int win_x = (GAME_FIELD_M - FINISH_GAME_M) / 2 + 1;

  WINDOW *win_window =
      create_window(FINISH_GAME_N, FINISH_GAME_M, win_y, win_x, 2);

  mvwprintw(win_window, (FINISH_GAME_N - 2) / 2, (FINISH_GAME_M - 11) / 2,
            "You win!");
  mvwprintw(win_window, (FINISH_GAME_N - 2) / 2 + 2, (FINISH_GAME_M - 11) / 2,
            "Score %d", gi.score);

  wrefresh(win_window);
  delwin(win_window);
}

void print_game_over(GameInfo_t gi) {
  int game_over_y = (GAME_FIELD_N - FINISH_GAME_N) / 2;
  int game_over_x = (GAME_FIELD_M - FINISH_GAME_M) / 2 + 1;

  WINDOW *game_over_window =
      create_window(FINISH_GAME_N, FINISH_GAME_M, game_over_y, game_over_x, 4);

  mvwprintw(game_over_window, (FINISH_GAME_N - 4) / 2, (FINISH_GAME_M - 12) / 2,
            " GAME OVER ");
  mvwprintw(game_over_window, (FINISH_GAME_N - 4) / 2 + 2,
            (FINISH_GAME_M - 10) / 2, "Score %d", gi.score);

  wrefresh(game_over_window);
  delwin(game_over_window);
}

WINDOW *print_game_field(GameInfo_t gi) {
  WINDOW *game_window =
      newwin(GAME_FIELD_N, GAME_FIELD_M, TOP_MARGIN, LEFT_MARGIN + 1);

  box(game_window, 0, 0);
  keypad(game_window, 1);
  nodelay(game_window, 1);

  mvwprintw(game_window, 0, (GAME_FIELD_M - 6) / 2, "TETRIS");

  for (int i = 0; i < FIELD_N; i++) {
    for (int j = 0; j < FIELD_M; j++) {
      if (gi.field[i][j] == 1) {
        wattron(game_window, COLOR_PAIR(9));
        mvwprintw(game_window, i + 1, 3 * j + 1, "   ");
        wattroff(game_window, COLOR_PAIR(9));
      } else {
        wattron(game_window, COLOR_PAIR(5));
        mvwprintw(game_window, i + 1, 3 * j + 1, "   ");
        wattroff(game_window, COLOR_PAIR(5));
      }
    }
  }

  return game_window;
}

WINDOW *print_game_info(GameInfo_t gi) {
  WINDOW *info_window = newwin(GAME_FIELD_N, GAME_INFO_M - 3, TOP_MARGIN,
                               LEFT_MARGIN + GAME_FIELD_M + 1);
  box(info_window, 0, 0);

  mvwprintw(info_window, 0, (GAME_INFO_M - 14) / 2, "Game_Status");

  print_next_figure(info_window, gi);

  mvwprintw(info_window, 7, 2, "High score  %d", gi.high_score);
  mvwprintw(info_window, 11, 2, "Score       %d", gi.score);
  mvwprintw(info_window, 13, 2, "Level       %d", gi.level);
  mvwprintw(info_window, 15, 2, "Speed       %d", gi.speed);
  mvwprintw(info_window, 18, 2, "'P' to pause");
  mvwprintw(info_window, 20, 2, "'Q' to menu");

  return info_window;
}

WINDOW *print_snake_game_field(GameInfo_t game) {
  WINDOW *snake_window =
      newwin(GAME_FIELD_N, GAME_FIELD_M, TOP_MARGIN, LEFT_MARGIN + 1);

  box(snake_window, 0, 0);
  keypad(snake_window, 1);
  nodelay(snake_window, 1);

  mvwprintw(snake_window, 0, (GAME_FIELD_M - 6) / 2, "SNAKE");

  for (int i = 0; i < GAME_FIELD_N; i++) {
    for (int j = 0; j < GAME_FIELD_M; j++) {
      bool is_snake_part = false;
      for (const auto &part : game.snake) {
        if (part.first == i && part.second == j) {
          is_snake_part = true;
          break;
        }
      }

      if (is_snake_part) {
        wattron(snake_window, COLOR_PAIR(10));
        mvwprintw(snake_window, i + 1, 3 * j + 1, "   ");
        wattroff(snake_window, COLOR_PAIR(10));
      } else if (game.food.first == i && game.food.second == j) {
        wattron(snake_window, COLOR_PAIR(11));
        mvwprintw(snake_window, i + 1, 3 * j + 1, "   ");
        wattroff(snake_window, COLOR_PAIR(11));
      }
    }
  }

  mvwprintw(snake_window, GAME_FIELD_N + 1, 1, "Score: %d", game.score);
  wrefresh(snake_window);
  return snake_window;
}

WINDOW *print_snake_game_info(GameInfo_t game) {
  WINDOW *info_window = newwin(GAME_FIELD_N, GAME_INFO_M - 3, TOP_MARGIN,
                               LEFT_MARGIN + GAME_FIELD_M + 1);
  box(info_window, 0, 0);

  mvwprintw(info_window, 0, (GAME_INFO_M - 14) / 2, "Snake Game Info");
  mvwprintw(info_window, 2, 2, "Score: %d", game.score);
  mvwprintw(info_window, 4, 2, "Length: %zu", game.snake.size());
  mvwprintw(info_window, 9, 2, "'P' to pause");
  mvwprintw(info_window, 11, 2, "'Q' to menu");

  return info_window;
}

void print_next_figure(WINDOW *info_window, GameInfo_t gi) {
  mvwprintw(info_window, 2, 2, "Next");

  for (int i = 0; i < gi.next_size; i++) {
    for (int j = 0; j < gi.next_size; j++) {
      if (gi.next[i][j] == 1) {
        wattron(info_window, COLOR_PAIR(9));
        mvwprintw(info_window, i + 4, j * 3 + 4, "   ");
        wattroff(info_window, COLOR_PAIR(9));
      }
    }
  }
}

void print_start_menu() {
  int start_y = (GAME_FIELD_N - START_MENU_N) / 2;
  int start_x = (GAME_FIELD_M - START_MENU_M) / 2 + 1;

  WINDOW *start_menu_window =
      newwin(START_MENU_N, START_MENU_M, start_y, start_x);
  box(start_menu_window, 0, 0);

  mvwhline(start_menu_window, 1, 2, ACS_HLINE, START_MENU_M - 4);
  mvwhline(start_menu_window, START_MENU_N - 2, 2, ACS_HLINE, START_MENU_M - 4);
  mvwvline(start_menu_window, 2, 1, ACS_VLINE, START_MENU_N - 4);
  mvwvline(start_menu_window, 2, START_MENU_M - 2, ACS_VLINE, START_MENU_N - 4);

  int text_width = 20;
  int text_height = 3;

  wattron(start_menu_window, COLOR_PAIR(1));
  mvwprintw(start_menu_window, (START_MENU_N - text_height) / 2,
            (START_MENU_M - text_width) / 2 + 1, "Let's play Tetris!");
  mvwprintw(start_menu_window, (START_MENU_N - text_height) / 2 + 1,
            (START_MENU_M - text_width) / 2 + 2, "  BrickGame");
  wattroff(start_menu_window, COLOR_PAIR(1));

  wattron(start_menu_window, COLOR_PAIR(6));
  mvwprintw(start_menu_window, (START_MENU_N - text_height) / 2 + 3,
            (START_MENU_M - text_width) / 2 + 1, "Hit '1' for Tetris!");
  mvwprintw(start_menu_window, (START_MENU_N - text_height) / 2 + 4,
            (START_MENU_M - text_width) / 2 + 1, "Hit '2' for Snake!");
  mvwprintw(start_menu_window, (START_MENU_N - text_height) / 2 + 5,
            (START_MENU_M - text_width) / 2 + 1, "Hit '3' for Racing!");
  mvwprintw(start_menu_window, (START_MENU_N - text_height) / 2 + 6,
            (START_MENU_M - text_width) / 2 + 1, "Hit '4' for Arcanoid!");
  mvwprintw(start_menu_window, (START_MENU_N - text_height) / 2 + 7,
            (START_MENU_M - text_width) / 2 + 1, "Hit '0' to Exit!");
  wattroff(start_menu_window, COLOR_PAIR(6));

  wrefresh(start_menu_window);
  delwin(start_menu_window);
}
