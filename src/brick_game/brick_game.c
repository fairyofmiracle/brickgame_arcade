#include "brick_game.h"

#include "arcanoid/arcanoid.h"
#include "gui/cli/interface.h"
#include "snake/snake.h"
#include "tetris/tetris.h"

int main(int argc, char *argv[]) {
  init_ncurses();
  init_colors();

  GameInfo_t game;

  if (argc > 1) {
    if (QString(argv[1]) == "tetris") {
      init_game();
      game_loop();
    } else if (QString(argv[1]) == "snake") {
      init_snake(game);
      playSnake(game);
    } else if (QString(argv[1]) == "race") {
      play_race_cli();
    } else if (QString(argv[1]) == "arcanoid") {
      play_arcanoid_cli();
    }
  } else {
    while (true) {
      print_start_menu();
      int choice = getch();
      if (choice == '1') {
        init_game();
        game_loop();
      } else if (choice == '2') {
        init_snake(game);
        playSnake(game);
      } else if (choice == '3') {
        play_race_cli();
      } else if (choice == '4') {
        play_arcanoid_cli();
      } else if (choice == '0') {
        break;
      }
    }
  }

  endwin();
  return 0;
}
