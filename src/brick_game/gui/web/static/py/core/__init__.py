"""
Common types and interfaces for BrickGame (browser bundle for Pyodide).

This is a copy of `src/core/__init__.py`, adapted for browser execution:
- scores directory is kept under `/data` in the in-memory FS
  (UI storage should be done via LocalStorage on the JS side if needed).
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import List, Protocol

# In Pyodide, keep "files" under an in-memory directory.
SCORES_DIR: Path = Path("/data")


def ensure_scores_dir() -> Path:
    """Create scores dir if absent."""
    try:
        SCORES_DIR.mkdir(parents=True, exist_ok=True)
    except OSError:
        # In restricted FS environments we silently ignore.
        pass
    return SCORES_DIR


# Field size per BrickGame spec
FIELD_HEIGHT: int = 20
FIELD_WIDTH: int = 10


class Action(Enum):
    START = "Start"
    PAUSE = "Pause"
    TERMINATE = "Terminate"
    LEFT = "Left"
    RIGHT = "Right"
    UP = "Up"
    DOWN = "Down"
    ACTION = "Action"


Matrix = List[List[bool]]


@dataclass(frozen=True)
class State:
    field: Matrix
    next: Matrix
    score: int
    high_score: int
    level: int
    speed: int
    pause: bool


class GameProtocol(Protocol):
    def user_input(self, action: Action, hold: bool) -> None:
        ...

    def update_current_state(self) -> State:
        ...
