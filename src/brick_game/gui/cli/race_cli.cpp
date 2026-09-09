#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../common/race_rest_client.h"
#include "interface.h"

namespace {

constexpr int RACE_GAME_ID = 1;
using State = race_rest::RaceStateDto;

enum ActionId {
  START = 0,
  PAUSE = 1,
  TERMINATE = 2,
  LEFT = 3,
  RIGHT = 4,
  UP = 5,
  DOWN = 6,
  ACTION = 7,
};

void draw_race_windows(const State &st) {
  WINDOW *game_window =
      newwin(GAME_FIELD_N, GAME_FIELD_M, TOP_MARGIN, LEFT_MARGIN + 1);
  box(game_window, 0, 0);
  mvwprintw(game_window, 0, (GAME_FIELD_M - 6) / 2, "RACING");
  for (int r = 0; r < race_rest::RACE_FIELD_HEIGHT; ++r) {
    for (int c = 0; c < race_rest::RACE_FIELD_WIDTH; ++c) {
      if (st.field[r][c]) {
        wattron(game_window, COLOR_PAIR(4));
        mvwprintw(game_window, r + 1, 3 * c + 1, "   ");
        wattroff(game_window, COLOR_PAIR(4));
      } else {
        wattron(game_window, COLOR_PAIR(5));
        mvwprintw(game_window, r + 1, 3 * c + 1, "   ");
        wattroff(game_window, COLOR_PAIR(5));
      }
    }
  }
  wrefresh(game_window);

  WINDOW *info_window = newwin(GAME_FIELD_N, GAME_INFO_M - 3, TOP_MARGIN,
                               LEFT_MARGIN + GAME_FIELD_M + 1);
  box(info_window, 0, 0);
  mvwprintw(info_window, 0, (GAME_INFO_M - 11) / 2, "Race Status");
  mvwprintw(info_window, 2, 2, "High score  %d", st.high_score);
  mvwprintw(info_window, 4, 2, "Score       %d", st.score);
  mvwprintw(info_window, 6, 2, "Level       %d", st.level);
  mvwprintw(info_window, 8, 2, "Speed       %d", st.speed);
  mvwprintw(info_window, 10, 2, "Lives       %d", st.lives);
  mvwprintw(info_window, 12, 2, "Nitro       %s", st.nitro ? "ON" : "OFF");
  mvwprintw(info_window, 15, 2, "'P' pause");
  mvwprintw(info_window, 16, 2, "'R' restart");
  mvwprintw(info_window, 17, 2, "'Q' menu");
  mvwprintw(info_window, 18, 2, "Arrows move");
  wrefresh(info_window);

  if (st.pause && !st.game_over) {
    print_pause_menu();
  } else if (st.game_over) {
    GameInfo_t gi{};
    gi.score = st.score;
    print_game_over(gi);
  }

  delwin(game_window);
  delwin(info_window);
}

}  // namespace

void play_race_cli() {
  State st{};
  race_rest::RaceRestClient client;

  if (!client.select_game(RACE_GAME_ID)) {
    WINDOW *err =
        newwin(5, 40, (GAME_FIELD_N - 5) / 2, (GAME_FIELD_M - 40) / 2);
    box(err, 0, 0);
    mvwprintw(err, 2, 2, "Race server unavailable");
    wrefresh(err);
    timeout(1200);
    getch();
    timeout(100);
    delwin(err);
    return;
  }

  timeout(100);
  long long last_fetch_ms = 0;
  long long last_up_ms = 0;
  bool nitro_hold = false;

  while (true) {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();

    if (now - last_fetch_ms >= 180) {
      if (client.fetch_state(st)) draw_race_windows(st);
      last_fetch_ms = now;
    }

    int ch = getch();
    if (ch != ERR) {
      if (ch == 'q' || ch == 'Q' || ch == 27) {
        (void)client.send_action(TERMINATE, false);
        break;
      } else if (ch == 'p' || ch == 'P') {
        (void)client.send_action(PAUSE, false);
      } else if (ch == 'r' || ch == 'R') {
        (void)client.select_game(RACE_GAME_ID);
      } else if (ch == KEY_LEFT) {
        (void)client.send_action(LEFT, false);
      } else if (ch == KEY_RIGHT) {
        (void)client.send_action(RIGHT, false);
      } else if (ch == KEY_UP) {
        last_up_ms = now;
        if (!nitro_hold) {
          nitro_hold = true;
          (void)client.send_action(UP, true);
        }
      }
    }

    if (nitro_hold && (now - last_up_ms > 130)) {
      nitro_hold = false;
      (void)client.send_action(UP, false);
    }
  }
}
