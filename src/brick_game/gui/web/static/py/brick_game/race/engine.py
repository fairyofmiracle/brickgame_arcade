from __future__ import annotations

import random
from typing import List

from core import (
    Action,
    GameProtocol,
    Matrix,
    SCORES_DIR,
    State,
    ensure_scores_dir,
)
from brick_game.race.models import Col, Pos, RaceConfig, RaceState, RaceStatus

RACE_MAX_LEVEL = 10
RACE_POINTS_PER_LEVEL = 5

_SPRITE_HEIGHT = 5
_MIN_VERTICAL_GAP = 7
_CONVOY_ROW_GAP = 14
_CONVOY_SIZE = 2
_NITRO_SPAWN_DIVISOR = 2
_NITRO_SPAWN_MIN_TICKS = 4


def _level_from_score(score: int) -> int:
    return min(RACE_MAX_LEVEL, 1 + max(0, int(score)) // RACE_POINTS_PER_LEVEL)


def _enemy_step_for_level(level: int) -> int:
    lv = max(1, min(RACE_MAX_LEVEL, level))
    return min(5, 1 + (lv - 1) // 2)


def _empty_matrix(height: int, width: int) -> Matrix:
    return [[False for _ in range(width)] for _ in range(height)]


_RACE_HIGH_SCORE_FILE = SCORES_DIR / "high_score_race.txt"


def _load_high_score() -> int:
    try:
        with open(_RACE_HIGH_SCORE_FILE, "r", encoding="utf-8") as f:
            raw = f.read().strip()
            return int(raw) if raw else 0
    except OSError:
        return 0


def _store_high_score(value: int) -> None:
    try:
        ensure_scores_dir()
        with open(_RACE_HIGH_SCORE_FILE, "w", encoding="utf-8") as f:
            f.write(str(int(value)))
    except OSError:
        pass


class RaceGame(GameProtocol):
    def __init__(self, config: RaceConfig | None = None) -> None:
        self._config = config or RaceConfig()
        self._state = RaceState()
        self._state.high_score = _load_high_score()
        self._accelerated: bool = False
        self._lane_centers: List[int] = [1, 5, 8]
        self._state.player_col = self._lane_centers[1]

    def user_input(self, action: Action, hold: bool) -> None:
        if action == Action.START:
            hs = self._state.high_score
            self._state = RaceState(status=RaceStatus.RUNNING, high_score=hs)
            self._accelerated = False
            return

        if action == Action.PAUSE and self._state.status == RaceStatus.RUNNING:
            self._state.status = RaceStatus.PAUSED
            self._state.pause = True
            return

        if action == Action.PAUSE and self._state.status == RaceStatus.PAUSED:
            self._state.status = RaceStatus.RUNNING
            self._state.pause = False
            return

        if action == Action.TERMINATE:
            self._state.status = RaceStatus.GAME_OVER
            self._state.game_over = True
            return

        if self._state.status != RaceStatus.RUNNING or self._state.pause:
            return

        if action == Action.LEFT:
            self._state.player_col = max(
                self._lane_centers[0],
                self._state.player_col - 1,
            )
        elif action == Action.RIGHT:
            self._state.player_col = min(
                self._lane_centers[-1],
                self._state.player_col + 1,
            )
        elif action == Action.UP:
            self._accelerated = bool(hold)

    def update_current_state(self) -> State:
        if self._state.status == RaceStatus.RUNNING and not self._state.pause:
            self._step()

        field = _empty_matrix(self._config.height, self._config.width)

        player_sprite = [
            (-3, 0),
            (-2, -1), (-2, 0), (-2, 1),
            (-1, 0),
            (0, -1), (0, 0), (0, 1),
        ]
        enemy_sprite = [
            (-4, 0),
            (-3, -1), (-3, 0), (-3, 1),
            (-2, 0),
            (-1, -1), (-1, 0), (-1, 1),
            (0, 0),
        ]

        def draw(row: int, col: int, offs) -> None:
            for dy, dx in offs:
                r = row + dy
                c = col + dx
                if (
                    0 <= r < self._config.height
                    and 0 <= c < self._config.width
                ):
                    field[r][c] = True

        for er, ec in self._state.enemies:
            draw(er, ec, enemy_sprite)

        pr = self._config.height - 1
        draw(pr, self._state.player_col, player_sprite)

        nxt = _empty_matrix(self._config.height, self._config.width)
        return State(
            field=field,
            next=nxt,
            score=self._state.score,
            high_score=self._state.high_score,
            level=self._state.level,
            speed=self._state.speed,
            pause=self._state.pause,
        )

    def get_render_layers(self) -> dict[str, object]:
        return {
            "nitro": bool(self._accelerated),
            "lives": int(self._state.lives),
            "game_over": bool(self._state.game_over),
            "status": str(self._state.status.value),
        }

    def _step(self) -> None:
        self._state.tick += 1

        level_step = _enemy_step_for_level(self._state.level)
        step = level_step * (2 if self._accelerated else 1)
        self._state.speed = step

        new_enemies: List[Pos] = []
        passed = 0
        for row, lane in self._state.enemies:
            new_row = row + step
            if new_row >= self._config.height:
                passed += 1
            else:
                new_enemies.append((new_row, lane))

        self._state.enemies = new_enemies
        if passed:
            self._state.score += passed
            self._state.level = _level_from_score(self._state.score)
            if self._state.score > self._state.high_score:
                self._state.high_score = self._state.score
                _store_high_score(self._state.high_score)

        interval = self._spawn_interval_ticks()
        if self._state.tick % interval == 0:
            self._spawn_enemy(convoy=self._accelerated)

        pr = self._config.height - 1
        for row, col in self._state.enemies:
            vert_ok = row >= pr - 3
            horiz_ok = abs(col - self._state.player_col) <= 1
            if vert_ok and horiz_ok:
                if self._state.lives > 0:
                    self._state.lives -= 1
                if self._state.lives <= 0:
                    self._state.status = RaceStatus.GAME_OVER
                    self._state.game_over = True
                    self._accelerated = False
                else:
                    self._state.enemies = []
                break

    def _spawn_interval_ticks(self) -> int:
        base = self._config.spawn_every_ticks
        if self._accelerated:
            return max(_NITRO_SPAWN_MIN_TICKS, base // _NITRO_SPAWN_DIVISOR)
        return base

    def _spawn_enemy(self, convoy: bool = False) -> None:
        top = _SPRITE_HEIGHT + _MIN_VERTICAL_GAP
        if any(r <= top for r, _ in self._state.enemies):
            return

        col: Col = random.choice(self._lane_centers)
        if convoy:
            for i in range(_CONVOY_SIZE):
                row = -i * _CONVOY_ROW_GAP
                self._state.enemies.append((row, col))
        else:
            self._state.enemies.append((0, col))
