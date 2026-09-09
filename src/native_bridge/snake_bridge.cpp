#include "../brick_game/brick_game.h"
#include "../brick_game/snake/snake.h"

#include <cstdio>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <direct.h>
#endif

#define FIELD_H FIELD_N
#define FIELD_W FIELD_M

extern "C" {

typedef struct {
    int field[FIELD_H][FIELD_W];
    int next[FIELD_H][FIELD_W];
    int score;
    int high_score;
    int level;
    int speed;
    int pause;
    int game_over;
} CState;

static void fill_cstate_from_snake(const GameInfo_t &game, CState *out) {
    for (int r = 0; r < FIELD_H; ++r) {
        for (int c = 0; c < FIELD_W; ++c) {
            out->field[r][c] = 0;
            out->next[r][c] = 0;
        }
    }

    // В змейке поле строится из `snake` и `food` (game.field может не использоваться)
    for (const auto &part : game.snake) {
        int r = part.first;
        int c = part.second;
        if (r >= 0 && r < FIELD_H && c >= 0 && c < FIELD_W) {
            out->field[r][c] = 1; // snake body
        }
    }
    {
        int r = game.food.first;
        int c = game.food.second;
        if (r >= 0 && r < FIELD_H && c >= 0 && c < FIELD_W) {
            out->field[r][c] = 2; // food
        }
    }

    out->score      = game.score;
    out->high_score = game.high_score;
    out->level      = game.level;
    out->speed      = game.speed;
    // В snake.cpp используется флаг `paused`, а не поле `pause` структуры
    out->pause      = game.paused ? 1 : 0;
    out->game_over  = (game.game_over || game.win) ? 1 : 0;
}

static GameInfo_t g_snake_game;
static bool g_snake_accel = false;

static void ensure_scores_data_dir(void) {
#if defined(_WIN32)
    _mkdir("data");
#else
    mkdir("data", 0755);
#endif
}

static const char kSnakeHighScoreFile[] = "data/high_score_snake.txt";

static int load_snake_high_score() {
    FILE *file = fopen(kSnakeHighScoreFile, "r");
    int hs = 0;
    if (file) {
        fscanf(file, "%d", &hs);
        fclose(file);
    }
    return hs;
}

static void store_snake_high_score(int hs) {
    ensure_scores_data_dir();
    FILE *file = fopen(kSnakeHighScoreFile, "w");
    if (file) {
        fprintf(file, "%d", hs);
        fclose(file);
    }
}

void snake_init(void) {
    init_snake(g_snake_game);
    // Чтобы в веб/REST не было пустых значений
    g_snake_game.level = 1;
    g_snake_game.speed = 1;
    ensure_scores_data_dir();
    g_snake_game.high_score = load_snake_high_score();
}

// action_id: 0=Start,1=Pause,2=Terminate,3=Left,4=Right,5=Up,6=Down (как в REST)
void snake_user_input(int action_id, int hold) {
    if (action_id == 1) {
        // Пауза из веб/API (как клавиша 'p' в консоли)
        g_snake_game.paused = !g_snake_game.paused;
        return;
    }
    if (action_id == 7) {
        // Action = ускорение (hold = pressed)
        g_snake_accel = (hold != 0);
        return;
    }
    // Direction mapping for web/API.
#ifdef BRICKGAME_WASM
    // In WASM builds we do not depend on ncurses KEY_* constants.
    // Apply direction changes directly.
    const bool is_dir = (action_id == 3 || action_id == 4 || action_id == 5 || action_id == 6);
    if (is_dir && g_snake_game.waiting_for_input) {
        g_snake_game.waiting_for_input = false;
    }
    if (action_id == 5 && g_snake_game.dir != Direction::DOWN) g_snake_game.dir = Direction::UP;
    if (action_id == 6 && g_snake_game.dir != Direction::UP) g_snake_game.dir = Direction::DOWN;
    if (action_id == 3 && g_snake_game.dir != Direction::RIGHT) g_snake_game.dir = Direction::LEFT;
    if (action_id == 4 && g_snake_game.dir != Direction::LEFT) g_snake_game.dir = Direction::RIGHT;
#else
    int ch = 0;
    switch (action_id) {
        case 3: ch = KEY_LEFT;  break;
        case 4: ch = KEY_RIGHT; break;
        case 5: ch = KEY_UP;    break;
        case 6: ch = KEY_DOWN;  break;
        default: break;
    }
    if (ch != 0) {
        process_input(g_snake_game, ch);
    }
#endif
}

void snake_update_state(CState *out) {
    // Move multiple steps depending on level/speed (and acceleration key).
    int lvl = g_snake_game.level > 0 ? g_snake_game.level : 1;
    int steps = lvl;
    if (g_snake_accel) steps += lvl; // faster when holding action
    if (steps < 1) steps = 1;
    if (steps > 10) steps = 10;
    for (int i = 0; i < steps; ++i) {
        update_snake(g_snake_game);
        if (g_snake_game.game_over || g_snake_game.win) break;
    }

    // Level mechanic: +1 level per 5 score, max 10, speed mirrors level.
    int level = 1 + (g_snake_game.score / 5);
    if (level > 10) level = 10;
    if (level < 1) level = 1;
    g_snake_game.level = level;
    g_snake_game.speed = level;

    if (g_snake_game.score > g_snake_game.high_score) {
        g_snake_game.high_score = g_snake_game.score;
        store_snake_high_score(g_snake_game.high_score);
    }
    fill_cstate_from_snake(g_snake_game, out);
}

} // extern "C"

