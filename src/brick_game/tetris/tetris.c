#include "tetris.h"

#include <stdio.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <direct.h>
#endif

#ifndef BRICKGAME_DATA_DIR
#define BRICKGAME_DATA_DIR "data"
#endif

static void brickgame_ensure_data_dir(void) {
#ifndef _WIN32
  mkdir(BRICKGAME_DATA_DIR, 0755);
#else
  _mkdir(BRICKGAME_DATA_DIR);
#endif
}

static void brickgame_high_score_path(char *buf, size_t buflen) {
  snprintf(buf, buflen, "%s/high_score.txt", BRICKGAME_DATA_DIR);
}

void init_game() {
  GameState_t *gs = get_game_state();

  // Инициализация состояния игры
  gs->x = START_X;
  gs->y = START_Y;
  // При входе в консольное тетрис сразу начинаем игру, чтобы не оставался экран
  // Initial. Внутреннее меню можно оставить только для GameOver/Win.
  gs->status = Spawn;
  gs->score = 0;
  gs->level = 1;
  gs->speed = 1000;
  gs->pause = false;
  gs->is_playing = true;
  gs->win = false;
  gs->time = get_time();

  // Создание игрового поля
  gs->field = create_matrix(FIELD_N, FIELD_M);
  if (!gs->field) {
    fprintf(stderr, "Failed to create game field!\n");
    return;  // Выход из функции, если создание матрицы не удалось
  }

  srand(get_time());

  // Создание следующей фигуры
  int size;
  gs->next_figure = create_figure(&size);
  if (!gs->next_figure) {
    fprintf(stderr, "Failed to create next figure!\n");
    free_matrix(FIELD_N, gs->field);  // Освобождение ресурсов
    return;  // Выход из функции, если создание фигуры не удалось
  }
  gs->next_figure_size = size;

  // Создание текущей фигуры
  gs->figure = create_matrix(size, size);
  if (!gs->figure) {
    fprintf(stderr, "Failed to create current figure!\n");
    free_matrix(FIELD_N, gs->field);  // Освобождение ресурсов
    free_matrix(gs->next_figure_size, gs->next_figure);
    return;  // Выход из функции, если создание фигуры не удалось
  }
  gs->figure_size = size;
  copy_matrix(size, size, gs->next_figure, gs->figure);

  brickgame_ensure_data_dir();
  char hs_path[128];
  brickgame_high_score_path(hs_path, sizeof(hs_path));
  // Чтение рекорда из файла
  FILE *file = fopen(hs_path, "r");
  if (file) {
    fscanf(file, "%d", &gs->high_score);
    fclose(file);
  } else {
    gs->high_score =
        0;  // Установите значение по умолчанию, если файл не найден
  }
}

#ifndef BRICKGAME_WASM
void game_loop() {
  init_game();  // Убедитесь, что игра инициализируется
  GameState_t *gs = get_game_state();

  while (gs->is_playing) {
    GameInfo_t gi = updateCurrentState(gs);
    int ch = getch();  // Получение ввода пользователя

    userInput(gs, get_user_action(ch));  // Обработка ввода
    render(gs->status, gs->win, gi);  // Отрисовка состояния игры
    free_game_info(&gi);  // Освобождение ресурсов

    // Условие для завершения игры
    if (gs->status == GameOver || gs->win) {
      while (ch != 'r' && ch != 'R' && ch != 'q' && ch != 'Q') ch = getch();
      if (ch == 'r' || ch == 'R') init_game();  // Перезапуск игры
    }

    timeout(10);  // Задержка между кадрами
  }
}
#endif

#ifndef BRICKGAME_WASM
void render(int status, bool win, GameInfo_t gi) {
  refresh();

  WINDOW *game_window = NULL;
  if (!gi.pause) {
    game_window = print_game_field(gi);
    wrefresh(game_window);
  }

  if (status == Initial)
    print_start_menu();
  else if (gi.pause)
    print_pause_menu();
  else if (win)
    print_win(gi);
  else if (status == GameOver)
    print_game_over(gi);

  WINDOW *info_window = print_game_info(gi);
  wrefresh(info_window);

  if (game_window != NULL) delwin(game_window);

  delwin(info_window);
}
#endif

int **create_figure(int *size) {
  int random_type = rand() % NUM_FIGURES;

  *size = (random_type == 0) ? 4 : (random_type == 3) ? 2 : 3;

  int **figure = create_matrix(*size, *size);

  if (figure != NULL) fill_figure(random_type, figure);

  return figure;
}

void fill_figure(int random_type, int **figure) {
  switch (random_type) {
    case 0:
      figure[0][0] = 1;
      figure[0][1] = 1;
      figure[0][2] = 1;
      figure[0][3] = 1;
      break;
    case 1:
      figure[0][0] = 1;
      figure[1][0] = 1;
      figure[1][1] = 1;
      figure[1][2] = 1;
      break;
    case 2:
      figure[0][2] = 1;
      figure[1][0] = 1;
      figure[1][1] = 1;
      figure[1][2] = 1;
      break;
    case 3:
      figure[0][0] = 1;
      figure[0][1] = 1;
      figure[1][0] = 1;
      figure[1][1] = 1;
      break;
    case 4:
      figure[0][1] = 1;
      figure[0][2] = 1;
      figure[1][0] = 1;
      figure[1][1] = 1;
      break;
    case 5:
      figure[0][1] = 1;
      figure[1][0] = 1;
      figure[1][1] = 1;
      figure[1][2] = 1;
      break;
    case 6:
      figure[0][0] = 1;
      figure[0][1] = 1;
      figure[1][1] = 1;
      figure[1][2] = 1;
      break;
    default:
      break;
  }
}

GameInfo_t updateCurrentState(GameState_t *gs) {
  GameInfo_t gi;

  gi.score = gs->score;
  gi.level = gs->level;
  gi.speed = gs->speed;

  gi.field = create_matrix(FIELD_N, FIELD_M);
  copy_matrix(FIELD_N, FIELD_M, gs->field, gi.field);

  if (gi.field != NULL) update_field(gs, gi.field);

  int size = gs->next_figure_size;
  gi.next = create_matrix(size, size);
  copy_matrix(size, size, gs->next_figure, gi.next);
  gi.next_size = size;

  gi.high_score = gs->high_score;
  gi.pause = gs->pause;

  return gi;
}

void update_field(GameState_t *gs, int **field) {
  for (int i = 0; i < gs->figure_size; i++) {
    for (int j = 0; j < gs->figure_size; j++) {
      int x = gs->x + j;
      int y = gs->y + i;

      if (gs->figure[i][j] == 1 && y > -1 && y < FIELD_N && x > -1 &&
          x < FIELD_M)
        field[y][x] = 1;
    }
  }
}

#ifndef BRICKGAME_WASM
UserAction_t get_user_action(int ch) {
  UserAction_t action =
      (UserAction_t)(-1);  // Инициализация с использованием приведения типов

  if (ch == KEY_R || ch == 'R' || ch == 'r') {
    action = Start;
  } else if (ch == KEY_P || ch == 'P' || ch == 'p') {
    action = Pause;
  } else if (ch == KEY_Q || ch == 'Q' || ch == 'q') {
    action = Terminate;
  } else if (ch == KEY_LEFT) {
    action = Left;
  } else if (ch == KEY_RIGHT) {
    action = Right;
  } else if (ch == KEY_UP) {
    action = Up;
  } else if (ch == KEY_DOWN) {
    action = Down;
  } else if (ch == KEY_Z) {
    action = Action;
  }

  return action;
}
#endif

void userInput(GameState_t *gs, UserAction_t action) {
  // Pause должен работать вне зависимости от текущего состояния игры.
  // Это нужно, например, для REST/тестов: сразу после init может прийти Pause.
  if (action == Pause) {
    gs->pause = !gs->pause;
    return;
  }

  if (gs->status == Initial) {
    if (action == Terminate)
      finish_game(gs);
    else if (action == Start)
      gs->status = Spawn;

  } else if (gs->status == Spawn) {
    if (action == Terminate || gs->win)
      finish_game(gs);
    else
      spawn_figure(gs);

  } else if (gs->status == Moving) {
    move_figure(gs, action);
  } else if (gs->status == Shifting) {
    move_down(gs);
  } else if (gs->status == Attaching) {
    attach_figure(gs);
  } else if (gs->status == GameOver) {
    finish_game(gs);
  }
}

void finish_game(GameState_t *gs) {
  if (gs->status != GameOver && !gs->win) gs->is_playing = false;

  free_matrix(FIELD_N, gs->field);
  free_matrix(gs->figure_size, gs->figure);
  free_matrix(gs->next_figure_size, gs->next_figure);
}

void spawn_figure(GameState_t *gs) {
  free_matrix(gs->figure_size, gs->figure);
  gs->figure = gs->next_figure;
  gs->figure_size = gs->next_figure_size;

  int size;
  gs->next_figure = create_figure(&size);
  gs->next_figure_size = size;

  gs->x = (gs->figure_size == 2) ? 4 : 3;
  gs->y = 0;

  gs->time = get_time();
  gs->status = Moving;
}

void move_figure(GameState_t *gs, UserAction_t action) {
  if (action == Left && !gs->pause)
    move_left(gs);
  else if (action == Right && !gs->pause)
    move_right(gs);
  else if (action == Down && !gs->pause)
    move_down(gs);
  else if ((action == Action || action == Up) && !gs->pause)
    rotate(gs);
  else if (action == Pause)
    gs->pause = !gs->pause;
  else if (action == Terminate)
    finish_game(gs);

  if (check_timer(gs, gs->speed) && !gs->pause) gs->status = Shifting;
}

void move_left(GameState_t *gs) {
  if (gs->pause) return;

  bool can_move = true;

  for (int i = 0; i < gs->figure_size; i++) {
    for (int j = 0; j < gs->figure_size; j++) {
      if (gs->figure[i][j] == 1) {
        int x = gs->x + j - 1;
        int y = gs->y + i;
        if (x < 0 || (y < FIELD_N && gs->field[y][x] == 1)) {
          can_move = false;
          break;
        }
      }
    }
    if (!can_move) break;
  }

  if (can_move) gs->x--;
  gs->status = figure_is_attaching(gs) ? Attaching : Moving;
}

void move_right(GameState_t *gs) {
  bool can_move = true;

  for (int i = 0; i < gs->figure_size; i++) {
    for (int j = 0; j < gs->figure_size; j++) {
      int x = gs->x + j + 1;
      int y = gs->y + i;

      if (gs->figure[i][j] == 1 &&
          (x >= FIELD_M || x < 0 || (y > -1 && gs->field[y][x] == 1)))
        can_move = false;
    }
  }

  if (can_move) gs->x++;

  gs->status = figure_is_attaching(gs) ? Attaching : Moving;
}

void move_down(GameState_t *gs) {
  if (!figure_is_attaching(gs)) {
    gs->y++;
    gs->status = Moving;
  } else {
    gs->status = Attaching;
  }
}

void rotate(GameState_t *gs) {
  int **temp_figure = create_matrix(gs->figure_size, gs->figure_size);

  for (int i = 0; i < gs->figure_size; i++) {
    for (int j = 0; j < gs->figure_size; j++) {
      temp_figure[j][gs->figure_size - i - 1] = gs->figure[i][j];
    }
  }

  // Wall-kick: if rotation collides near wall, try small horizontal offsets.
  const int kicks[] = {0, -1, 1, -2, 2};
  bool applied = false;
  for (size_t ki = 0; ki < sizeof(kicks) / sizeof(kicks[0]); ++ki) {
    const int dx = kicks[ki];
    bool ok = true;
    for (int i = 0; i < gs->figure_size && ok; i++) {
      for (int j = 0; j < gs->figure_size; j++) {
        if (temp_figure[i][j] != 1) continue;
        int x = gs->x + j + dx;
        int y = gs->y + i;
        if (x < 0 || x >= FIELD_M || y >= FIELD_N) {
          ok = false;
          break;
        }
        if (y > -1 && gs->field[y][x] == 1) {
          ok = false;
          break;
        }
      }
    }
    if (!ok) continue;
    gs->x += dx;
    free_matrix(gs->figure_size, gs->figure);
    gs->figure = temp_figure;
    applied = true;
    break;
  }
  if (!applied) free_matrix(gs->figure_size, temp_figure);
}

void attach_figure(GameState_t *gs) {
  bool is_game_over = check_top_line(gs);

  for (int i = 0; i < gs->figure_size && !is_game_over; i++) {
    for (int j = 0; j < gs->figure_size; j++) {
      if (gs->figure[i][j] == 1) gs->field[gs->y + i][gs->x + j] = 1;
    }
  }

  if (is_game_over) {
    gs->status = GameOver;
  } else {
    process_full_lines(gs);
    gs->status = Spawn;
  }
}

bool check_top_line(GameState_t *gs) {
  bool is_filled = false;

  for (int j = 0; j < FIELD_M && !is_filled; j++) {
    is_filled = gs->field[0][j] == 1;
  }

  return is_filled;
}

void process_full_lines(GameState_t *gs) {
  int num_full_lines = 0;

  for (int i = FIELD_N - 1; i >= 0; i--) {
    bool is_full = true;

    for (int j = 0; j < FIELD_M && is_full; j++) {
      if (gs->field[i][j] == 0) {
        is_full = false;
      }
    }

    if (is_full) {
      shift_lines(gs, i);
      fill_top_line(gs);
      num_full_lines++;
      i++;
    }
  }

  update_score_and_level(gs, num_full_lines);
}

void shift_lines(GameState_t *gs, int i) {
  for (int j = i; j > 0; j--) {
    for (int k = 0; k < FIELD_M; k++) {
      gs->field[j][k] = gs->field[j - 1][k];
    }
  }
}

void fill_top_line(GameState_t *gs) {
  for (int j = 0; j < FIELD_M; j++) {
    gs->field[0][j] = 0;
  }
}

void update_score_and_level(GameState_t *gs, int num_full_lines) {
  switch (num_full_lines) {
    case 1:
      gs->score += 100;
      break;
    case 2:
      gs->score += 300;
      break;
    case 3:
      gs->score += 700;
      break;
    case 4:
      gs->score += 1500;
      break;
    default:
      break;
  }

  if (gs->score >= SCORE_PER_LEVEL) {
    gs->level = gs->score / SCORE_PER_LEVEL + 1;
    if (gs->level > MAX_LEVEL) gs->level = MAX_LEVEL;
    // Speed should grow with level; level=1 => base delay.
    const double k = pow(0.8, (double)(gs->level - 1));
    const int delay = (int)round((double)DELAY_MS * k);
    gs->speed = delay < 60 ? 60 : delay;
  }

  if (gs->level >= MAX_LEVEL) gs->win = true;

  if (gs->score > gs->high_score) {
    gs->high_score = gs->score;
    char hs_path[128];
    brickgame_high_score_path(hs_path, sizeof(hs_path));
    FILE *file = fopen(hs_path, "w");

    if (file) {
      fprintf(file, "%d", gs->high_score);
      fclose(file);
    }
  }
}

GameState_t *get_game_state() {
  static GameState_t gs;
  return &gs;
}

int **create_matrix(int N, int M) {
  int **result = (int **)calloc(N, sizeof(int *));

  if (result != NULL) {
    for (int i = 0; i < N; i++) {
      result[i] = (int *)calloc(M, sizeof(int));

      if (result[i] == NULL) {
        free_matrix(i, result);
        break;
      }
    }
  }

  return result;
}

void copy_matrix(int N, int M, int **src_matrix, int **dest_matrix) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < M; j++) {
      dest_matrix[i][j] = src_matrix[i][j];
    }
  }
}

void free_matrix(int N, int **matrix) {
  if (matrix != NULL) {
    for (int i = 0; i < N; i++) {
      free(matrix[i]);
    }
    free(matrix);
  }
}

bool figure_is_attaching(GameState_t *gs) {
  bool is_attaching = false;

  for (int i = 0; i < gs->figure_size && !is_attaching; i++) {
    for (int j = 0; j < gs->figure_size && !is_attaching; j++) {
      int x = gs->x + j;
      int y = gs->y + i + 1;

      if (gs->figure[i][j] == 1 &&
          (y > FIELD_N - 1 || (y > -1 && gs->field[y][x] == 1)))
        is_attaching = true;
    }
  }

  return is_attaching;
}

long long get_time() {
  struct timeval t;
  gettimeofday(&t, NULL);

  return (long long)t.tv_sec * 1000 + t.tv_usec / 1000;
}

bool check_timer(GameState_t *gs, int delay) {
  bool result = false;
  long long time = get_time();

  if (time - gs->time >= delay) {
    gs->time = time;
    result = true;
  }

  return result;
}

void free_game_info(GameInfo_t *gi) {
  free_matrix(FIELD_N, gi->field);
  free_matrix(gi->next_size, gi->next);
}

void initialize_game_state(GameState_t *gs) {
  gs->x = START_X;
  gs->y = START_Y;
  gs->status = Initial;
  gs->score = 0;
  gs->level = 1;
  gs->speed = 1000;
  gs->pause = false;
  gs->is_playing = true;
  gs->win = false;
  gs->time = get_time();
  gs->field = NULL;  // Установите в NULL, если не инициализировано
  gs->figure = NULL;
  gs->next_figure = NULL;
  gs->high_score = 0;
}
