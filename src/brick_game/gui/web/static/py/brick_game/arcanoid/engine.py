from __future__ import annotations

from typing import Set, Tuple

from core import (
    Action,
    GameProtocol,
    Matrix,
    SCORES_DIR,
    State,
    ensure_scores_dir,
)
from brick_game.arcanoid.models import (
    ArcanoidConfig,
    ArcanoidState,
    ArcanoidStatus,
    Cell,
)

_ARCANOID_HIGH_SCORE_FILE = SCORES_DIR / "high_score_arcanoid.txt"


def _empty_matrix(height: int, width: int) -> Matrix:
    return [[False for _ in range(width)] for _ in range(height)]


def _clamp(v: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, v))


def _load_high_score() -> int:
    try:
        with open(_ARCANOID_HIGH_SCORE_FILE, "r", encoding="utf-8") as f:
            raw = f.read().strip()
            return int(raw) if raw else 0
    except OSError:
        return 0


def _store_high_score(value: int) -> None:
    try:
        ensure_scores_dir()
        with open(_ARCANOID_HIGH_SCORE_FILE, "w", encoding="utf-8") as f:
            f.write(str(int(value)))
    except OSError:
        pass


class ArcanoidGame(GameProtocol):
    def __init__(self, config: ArcanoidConfig | None = None) -> None:
        self._config = config or ArcanoidConfig()
        self._state = ArcanoidState()
        self._state.high_score = _load_high_score()
        self._reset_round(start_running=False)

    def user_input(self, action: Action, hold: bool) -> None:
        if action == Action.START:
            hs = self._state.high_score
            self._state = ArcanoidState(
                high_score=hs,
                status=ArcanoidStatus.RUNNING,
            )
            self._reset_round(start_running=True)
            return

        if (
            action == Action.PAUSE
            and self._state.status == ArcanoidStatus.RUNNING
        ):
            self._state.status = ArcanoidStatus.PAUSED
            self._state.pause = True
            return

        if (
            action == Action.PAUSE
            and self._state.status == ArcanoidStatus.PAUSED
        ):
            self._state.status = ArcanoidStatus.RUNNING
            self._state.pause = False
            return

        if action == Action.TERMINATE:
            self._state.status = ArcanoidStatus.GAME_OVER
            self._state.game_over = True
            return

        if self._state.status not in (
            ArcanoidStatus.RUNNING,
            ArcanoidStatus.PAUSED,
        ):
            return

        if action == Action.LEFT:
            self._state.paddle_col = _clamp(
                self._state.paddle_col - 1,
                self._min_paddle_center(),
                self._max_paddle_center(),
            )
            if not self._state.ball_released:
                self._state.ball_col = self._state.paddle_col
        elif action == Action.RIGHT:
            self._state.paddle_col = _clamp(
                self._state.paddle_col + 1,
                self._min_paddle_center(),
                self._max_paddle_center(),
            )
            if not self._state.ball_released:
                self._state.ball_col = self._state.paddle_col
        elif action in (Action.ACTION, Action.UP):
            if self._state.status == ArcanoidStatus.RUNNING:
                self._state.ball_released = True

        _ = hold

    def update_current_state(self) -> State:
        if (
            self._state.status == ArcanoidStatus.RUNNING
            and not self._state.pause
        ):
            self._step_ball()

        field = _empty_matrix(self._config.height, self._config.width)
        for r, c in self._state.bricks:
            if 0 <= r < self._config.height and 0 <= c < self._config.width:
                field[r][c] = True

        paddle_row = self._config.height - 1
        for c in self._paddle_cols():
            field[paddle_row][c] = True

        field[self._state.ball_row][self._state.ball_col] = True
        return State(
            field=field,
            next=_empty_matrix(self._config.height, self._config.width),
            score=self._state.score,
            high_score=self._state.high_score,
            level=self._state.level,
            speed=self._state.speed,
            pause=self._state.pause,
        )

    def get_render_layers(self) -> dict[str, object]:
        paddle_row = self._config.height - 1
        half = self._config.paddle_width // 2
        paddle_x = self._state.paddle_col - half
        paddle_w = self._config.paddle_width
        paddle_x = max(0, min(self._config.width - paddle_w, paddle_x))

        bricks = [
            {"x": int(c), "y": int(r), "w": 1, "h": 1}
            for (r, c) in sorted(self._state.bricks)
        ]

        return {
            "game_over": bool(self._state.game_over),
            "status": str(self._state.status.value),
            "lives": int(self._state.lives),
            "arcanoid": {
                "ball": {
                    "x": float(self._state.ball_col) + 0.5,
                    "y": float(self._state.ball_row) + 0.5,
                    "r": 0.35,
                },
                "paddle": {
                    "x": float(paddle_x),
                    "y": float(paddle_row),
                    "w": float(paddle_w),
                    "h": 1.0,
                },
                "bricks": bricks,
            },
        }

    def _step_ball(self) -> None:
        if not self._state.ball_released:
            self._state.ball_col = self._state.paddle_col
            self._state.ball_row = self._config.height - 2
            return

        nr = self._state.ball_row + self._state.ball_dr
        nc = self._state.ball_col + self._state.ball_dc

        if nc < 0 or nc >= self._config.width:
            self._state.ball_dc *= -1
            nc = self._state.ball_col + self._state.ball_dc

        if nr < 0:
            self._state.ball_dr *= -1
            nr = self._state.ball_row + self._state.ball_dr

        hit_any = False
        curr_r = self._state.ball_row
        curr_c = self._state.ball_col

        vert_hit = (nr, curr_c)
        if vert_hit in self._state.bricks:
            self._state.bricks.remove(vert_hit)
            self._state.ball_dr *= -1
            nr = curr_r + self._state.ball_dr
            hit_any = True

        horiz_hit = (curr_r, nc)
        if horiz_hit in self._state.bricks:
            self._state.bricks.remove(horiz_hit)
            self._state.ball_dc *= -1
            nc = curr_c + self._state.ball_dc
            hit_any = True

        diag_hit = (nr, nc)
        if (not hit_any) and diag_hit in self._state.bricks:
            self._state.bricks.remove(diag_hit)
            self._state.ball_dr *= -1
            nr = curr_r + self._state.ball_dr
            hit_any = True

        if hit_any:
            self._state.score += 1
            self._state.level = min(
                self._config.max_level,
                1 + self._state.score // self._config.points_per_level,
            )
            self._state.speed = self._state.level
            if self._state.score > self._state.high_score:
                self._state.high_score = self._state.score
                _store_high_score(self._state.high_score)

        paddle_row = self._config.height - 1
        if nr == paddle_row:
            cols = self._paddle_cols()
            min_c = min(cols)
            max_c = max(cols)
            if nc in cols or nc == min_c - 1 or nc == max_c + 1:
                self._state.ball_dr = -1
                if nc < self._state.paddle_col:
                    self._state.ball_dc = -1
                elif nc > self._state.paddle_col:
                    self._state.ball_dc = 1
                nc = _clamp(nc, min_c, max_c)
                nr = self._state.ball_row + self._state.ball_dr
            else:
                self._state.status = ArcanoidStatus.GAME_OVER
                self._state.game_over = True
                self._state.ball_released = False
                return

        self._state.ball_row = _clamp(nr, 0, self._config.height - 1)
        self._state.ball_col = _clamp(nc, 0, self._config.width - 1)

        if not self._state.bricks:
            self._state.status = ArcanoidStatus.WON
            self._state.game_over = True
            self._state.ball_released = False

    def _reset_round(self, start_running: bool) -> None:
        self._state.pause = False
        self._state.game_over = False
        if not start_running:
            self._state.status = ArcanoidStatus.INITIAL

        self._state.score = 0
        self._state.level = 1
        self._state.speed = 1
        self._state.lives = 1

        self._state.paddle_col = self._config.width // 2
        self._state.ball_row = self._config.height - 2
        self._state.ball_col = self._state.paddle_col
        self._state.ball_dr = -1
        self._state.ball_dc = 1
        self._state.ball_released = False
        self._state.bricks = self._build_bricks()

    def _build_bricks(self) -> Set[Cell]:
        bricks: Set[Cell] = set()
        start_row = self._config.bricks_top
        end_row = min(
            self._config.height - 3,
            start_row + self._config.bricks_rows,
        )
        for r in range(start_row, end_row):
            for c in range(self._config.width):
                bricks.add((r, c))
        return bricks

    def _paddle_cols(self) -> Tuple[int, ...]:
        half = self._config.paddle_width // 2
        cols = []
        for c in range(
            self._state.paddle_col - half,
            self._state.paddle_col + half + 1,
        ):
            if 0 <= c < self._config.width:
                cols.append(c)
        return tuple(cols)

    def _min_paddle_center(self) -> int:
        return self._config.paddle_width // 2

    def _max_paddle_center(self) -> int:
        return self._config.width - 1 - self._config.paddle_width // 2
