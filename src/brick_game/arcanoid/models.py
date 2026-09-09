from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Set, Tuple

from core import FIELD_HEIGHT, FIELD_WIDTH


class ArcanoidStatus(str, Enum):
    INITIAL = "initial"
    RUNNING = "running"
    PAUSED = "paused"
    WON = "won"
    GAME_OVER = "game_over"


Cell = Tuple[int, int]


@dataclass(slots=True)
class ArcanoidConfig:
    height: int = FIELD_HEIGHT
    width: int = FIELD_WIDTH
    paddle_width: int = 3
    bricks_rows: int = 4
    bricks_top: int = 2
    points_per_level: int = 5
    max_level: int = 10


@dataclass(slots=True)
class ArcanoidState:
    status: ArcanoidStatus = ArcanoidStatus.INITIAL
    pause: bool = False
    game_over: bool = False

    score: int = 0
    high_score: int = 0
    level: int = 1
    speed: int = 1
    lives: int = 1

    paddle_col: int = FIELD_WIDTH // 2
    ball_row: int = FIELD_HEIGHT - 2
    ball_col: int = FIELD_WIDTH // 2
    ball_dr: int = -1
    ball_dc: int = 1
    ball_released: bool = False

    bricks: Set[Cell] = field(default_factory=set)
