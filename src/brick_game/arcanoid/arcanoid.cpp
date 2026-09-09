#include "arcanoid.h"

#ifndef BRICKGAME_WASM

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <utility>
#ifndef _WIN32
#include <sys/time.h>
#else
#include <windows.h>
#endif

#include "../gui/cli/interface.h"

namespace {

constexpr int PADDLE_WIDTH = 3;
constexpr int BRICKS_TOP = 2;
constexpr int BRICKS_ROWS = 4;
constexpr int POINTS_PER_LEVEL = 5;
constexpr int MAX_LEVEL = 10;

struct ArcState {
  bool pause = false;
  bool game_over = false;
  bool win = false;

  int score = 0;
  int high_score = 0;
  int level = 1;
  int speed = 1;

  int paddle_col = FIELD_M / 2;
  int ball_row = FIELD_N - 2;
  int ball_col = FIELD_M / 2;
  int ball_dr = -1;
  int ball_dc = 1;
  bool ball_released = false;

  std::set<std::pair<int, int>> bricks;
};

static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static long long now_ms() {
#ifndef _WIN32
  timeval tv{};
  gettimeofday(&tv, nullptr);
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#else
  return (long long)GetTickCount64();
#endif
}

static int min_paddle_center() { return PADDLE_WIDTH / 2; }
static int max_paddle_center() { return FIELD_M - 1 - PADDLE_WIDTH / 2; }

static void ensure_data_dir() {
  // Best-effort: consistent with tetris (creates ./data).
#ifndef _WIN32
  (void)system("mkdir -p data >/dev/null 2>&1");
#else
  (void)system("mkdir data >nul 2>nul");
#endif
}

static int load_high_score() {
  std::ifstream in("data/high_score_arcanoid.txt");
  int v = 0;
  if (in.is_open()) in >> v;
  return v;
}

static void store_high_score(int v) {
  ensure_data_dir();
  std::ofstream out("data/high_score_arcanoid.txt", std::ios::out | std::ios::trunc);
  if (out.is_open()) out << v;
}

static void reset_round(ArcState &st) {
  st.pause = false;
  st.game_over = false;
  st.win = false;

  st.score = 0;
  st.level = 1;
  st.speed = 1;

  st.paddle_col = FIELD_M / 2;
  st.ball_row = FIELD_N - 2;
  st.ball_col = st.paddle_col;
  st.ball_dr = -1;
  st.ball_dc = 1;
  st.ball_released = false;

  st.bricks.clear();
  const int end_row = std::min(FIELD_N - 3, BRICKS_TOP + BRICKS_ROWS);
  for (int r = BRICKS_TOP; r < end_row; ++r) {
    for (int c = 0; c < FIELD_M; ++c) st.bricks.insert({r, c});
  }
}

static void update_meta(ArcState &st) {
  st.level = std::min(MAX_LEVEL, 1 + st.score / POINTS_PER_LEVEL);
  st.speed = st.level;
  if (st.score > st.high_score) {
    st.high_score = st.score;
    store_high_score(st.high_score);
  }
}

static void step_ball(ArcState &st) {
  if (!st.ball_released) {
    st.ball_col = st.paddle_col;
    st.ball_row = FIELD_N - 2;
    return;
  }

  int nr = st.ball_row + st.ball_dr;
  int nc = st.ball_col + st.ball_dc;

  if (nc < 0 || nc >= FIELD_M) {
    st.ball_dc *= -1;
    nc = st.ball_col + st.ball_dc;
  }
  if (nr < 0) {
    st.ball_dr *= -1;
    nr = st.ball_row + st.ball_dr;
  }

  bool hit_any = false;
  const int curr_r = st.ball_row;
  const int curr_c = st.ball_col;

  auto vert_hit = std::make_pair(nr, curr_c);
  if (st.bricks.count(vert_hit)) {
    st.bricks.erase(vert_hit);
    st.ball_dr *= -1;
    nr = curr_r + st.ball_dr;
    hit_any = true;
  }

  auto horiz_hit = std::make_pair(curr_r, nc);
  if (st.bricks.count(horiz_hit)) {
    st.bricks.erase(horiz_hit);
    st.ball_dc *= -1;
    nc = curr_c + st.ball_dc;
    hit_any = true;
  }

  auto diag_hit = std::make_pair(nr, nc);
  if (!hit_any && st.bricks.count(diag_hit)) {
    st.bricks.erase(diag_hit);
    st.ball_dr *= -1;
    nr = curr_r + st.ball_dr;
    hit_any = true;
  }

  if (hit_any) {
    st.score += 1;
    update_meta(st);
  }

  const int paddle_row = FIELD_N - 1;
  if (nr == paddle_row) {
    const int half = PADDLE_WIDTH / 2;
    const int min_c = st.paddle_col - half;
    const int max_c = st.paddle_col + half;
    const bool inside = (nc >= min_c && nc <= max_c);
    const bool forgive = (nc == min_c - 1 || nc == max_c + 1);
    if (inside || forgive) {
      st.ball_dr = -1;
      if (nc < st.paddle_col)
        st.ball_dc = -1;
      else if (nc > st.paddle_col)
        st.ball_dc = 1;
      nc = clamp(nc, min_c, max_c);
      nr = st.ball_row + st.ball_dr;
    } else {
      st.game_over = true;
      st.ball_released = false;
      return;
    }
  }

  st.ball_row = clamp(nr, 0, FIELD_N - 1);
  st.ball_col = clamp(nc, 0, FIELD_M - 1);

  if (st.bricks.empty()) {
    st.win = true;
    st.game_over = true;
    st.ball_released = false;
  }
}

static void build_field(const ArcState &st, int out[FIELD_N][FIELD_M]) {
  for (int r = 0; r < FIELD_N; ++r)
    for (int c = 0; c < FIELD_M; ++c) out[r][c] = 0;

  for (const auto &bc : st.bricks) {
    if (bc.first >= 0 && bc.first < FIELD_N && bc.second >= 0 && bc.second < FIELD_M)
      out[bc.first][bc.second] = 1;
  }

  const int paddle_row = FIELD_N - 1;
  const int half = PADDLE_WIDTH / 2;
  for (int c = st.paddle_col - half; c <= st.paddle_col + half; ++c) {
    if (c >= 0 && c < FIELD_M) out[paddle_row][c] = 1;
  }

  if (st.ball_row >= 0 && st.ball_row < FIELD_N && st.ball_col >= 0 && st.ball_col < FIELD_M)
    out[st.ball_row][st.ball_col] = 1;
}

static void draw_arcanoid_windows(const ArcState &st) {
  int field[FIELD_N][FIELD_M];
  build_field(st, field);

  WINDOW *game_window = newwin(GAME_FIELD_N, GAME_FIELD_M, TOP_MARGIN, LEFT_MARGIN + 1);
  box(game_window, 0, 0);
  mvwprintw(game_window, 0, (GAME_FIELD_M - 8) / 2, "ARCANOID");

  for (int r = 0; r < FIELD_N; ++r) {
    for (int c = 0; c < FIELD_M; ++c) {
      if (field[r][c]) {
        wattron(game_window, COLOR_PAIR(9));
        mvwprintw(game_window, r + 1, 3 * c + 1, "   ");
        wattroff(game_window, COLOR_PAIR(9));
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
  mvwprintw(info_window, 0, (GAME_INFO_M - 14) / 2, "Arcanoid Status");
  mvwprintw(info_window, 2, 2, "High score  %d", st.high_score);
  mvwprintw(info_window, 4, 2, "Score       %d", st.score);
  mvwprintw(info_window, 6, 2, "Level       %d", st.level);
  mvwprintw(info_window, 8, 2, "Speed       %d", st.speed);
  mvwprintw(info_window, 12, 2, "Space/Up: start");
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
    if (st.win)
      print_win(gi);
    else
      print_game_over(gi);
  }

  delwin(game_window);
  delwin(info_window);
}

}  // namespace

void play_arcanoid_cli() {
  ArcState st;
  st.high_score = load_high_score();
  reset_round(st);

  timeout(60);
  long long last_tick = 0;

  while (true) {
    const long long now = now_ms();
    const int tick_ms = std::max(40, 220 - (st.level - 1) * 18);
    if (!st.pause && !st.game_over && (now - last_tick >= tick_ms)) {
      step_ball(st);
      last_tick = now;
    }

    draw_arcanoid_windows(st);

    int ch = getch();
    if (ch != ERR) {
      if (ch == 'q' || ch == 'Q' || ch == 27) {
        break;
      } else if (ch == 'p' || ch == 'P') {
        st.pause = !st.pause;
      } else if (ch == 'r' || ch == 'R') {
        // keep high score
        int hs = st.high_score;
        reset_round(st);
        st.high_score = hs;
      } else if (ch == KEY_LEFT) {
        st.paddle_col = clamp(st.paddle_col - 1, min_paddle_center(), max_paddle_center());
        if (!st.ball_released) st.ball_col = st.paddle_col;
      } else if (ch == KEY_RIGHT) {
        st.paddle_col = clamp(st.paddle_col + 1, min_paddle_center(), max_paddle_center());
        if (!st.ball_released) st.ball_col = st.paddle_col;
      } else if (ch == KEY_UP || ch == ' ') {
        if (!st.game_over && !st.pause) st.ball_released = true;
      }
    }

    if (st.game_over) {
      // When game ends, just wait for R/Q with a small delay.
      timeout(120);
    } else {
      timeout(60);
    }
  }
}

#endif  // !BRICKGAME_WASM

