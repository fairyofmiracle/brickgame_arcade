#include "../brick_game/brick_game.h"
#include "../brick_game/tetris/tetris.h"

#define FIELD_H FIELD_N
#define FIELD_W FIELD_M

#ifdef __cplusplus
extern "C" {
#endif

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

static void fill_cstate_from_tetris(const GameInfo_t *gi, const GameState_t *gs, CState *out) {
    // field: всегда FIELD_H x FIELD_W
    for (int r = 0; r < FIELD_H; ++r) {
        for (int c = 0; c < FIELD_W; ++c) {
            out->field[r][c] = (gi && gi->field) ? gi->field[r][c] : 0;
            out->next[r][c] = 0;
        }
    }

    // next: размер задаётся `next_size` (обычно 4x4), читаем только в пределах
    if (gi && gi->next && gi->next_size > 0) {
        int n = gi->next_size;
        if (n > FIELD_H) n = FIELD_H;
        if (n > FIELD_W) n = FIELD_W;
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                out->next[r][c] = gi->next[r][c];
            }
        }
    }
    out->score      = gi->score;
    out->high_score = gi->high_score;
    out->level      = gi->level;
    out->speed      = gi->speed;
    out->pause      = gi->pause;
    out->game_over  = (gs && (gs->status == GameOver || gs->win)) ? 1 : 0;
}

void tetris_init(void) {
    init_game();
}

// action_id должен соответствовать enum UserAction_t
void tetris_user_input(int action_id, int hold) {
    (void)hold;
    GameState_t *gs = get_game_state();
    UserAction_t action = (UserAction_t)action_id;
    userInput(gs, action);
}

void tetris_update_state(CState *out) {
    GameState_t *gs = get_game_state();
    // В оригинальном ncurses-цикле при GameOver/Win управление останавливается и ждёт R/Q.
    // В вебе мы должны сохранить поле/next для отрисовки "Игра завершена",
    // поэтому НЕ вызываем userInput() в этих состояниях (иначе finish_game() освободит матрицы).
    if (gs->status == GameOver || gs->win) {
        GameInfo_t gi = updateCurrentState(gs);
        fill_cstate_from_tetris(&gi, gs, out);
        free_game_info(&gi);
        return;
    }
    // В ncurses-версии основной цикл вызывает userInput() даже без нажатий клавиш,
    // чтобы работал таймер падения и проходили промежуточные состояния (Spawn/Attaching).
    // На вебе /state дергается периодически — эмулируем "пустое" действие.
    for (int step = 0; step < 4; ++step) {
        if (gs->status == GameOver || gs->win) break;
        userInput(gs, (UserAction_t)(-1));
        if (gs->status == Moving || gs->status == GameOver || gs->win) break;
    }
    GameInfo_t gi = updateCurrentState(gs);
    fill_cstate_from_tetris(&gi, gs, out);
    free_game_info(&gi);
}

#ifdef __cplusplus
}
#endif

