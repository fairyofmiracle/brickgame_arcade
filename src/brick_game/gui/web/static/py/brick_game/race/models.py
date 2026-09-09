from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import List, Tuple

from core import FIELD_HEIGHT, FIELD_WIDTH


class RaceStatus(str, Enum):
    INITIAL = "initial"
    RUNNING = "running"
    PAUSED = "paused"
    GAME_OVER = "game_over"


Col = int
Pos = Tuple[int, Col]


@dataclass(slots=True)
class RaceConfig:
    height: int = FIELD_HEIGHT
    width: int = FIELD_WIDTH
    lanes: int = 3
    spawn_every_ticks: int = 10


@dataclass(slots=True)
class RaceState:
    status: RaceStatus = RaceStatus.INITIAL
    pause: bool = False
    game_over: bool = False

    score: int = 0
    high_score: int = 0
    level: int = 1
    speed: int = 1
    lives: int = 3

    player_col: Col = FIELD_WIDTH // 2
    enemies: List[Pos] = field(default_factory=list)
    tick: int = 0
