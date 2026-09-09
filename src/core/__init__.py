"""
Общие типы и интерфейсы для всех игр BrickGame (Python‑реализация).

Интерфейс повторяет спецификацию из `materials/library-specification_RUS.md`:
- перечисление действий пользователя (Action);
- структура состояния игры (State);
- абстрактный интерфейс игры (GameProtocol).
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import List, Protocol

# Корень репозитория: src/core/__init__.py → parents[2]
_REPO_ROOT: Path = Path(__file__).resolve().parents[2]
# Все файлы рекордов — в одной папке у корня проекта
SCORES_DIR: Path = _REPO_ROOT / "data"


def ensure_scores_dir() -> Path:
    """Создать папку для рекордов, если её ещё нет."""
    SCORES_DIR.mkdir(parents=True, exist_ok=True)
    return SCORES_DIR


# Размер поля по спецификации BrickGame
FIELD_HEIGHT: int = 20
FIELD_WIDTH: int = 10


class Action(Enum):
    """
    Действия пользователя, соответствуют enum Action из спецификации.
    Имена совпадают с C#/C‑вариантом, чтобы было проще сопоставлять логику.
    """

    START = "Start"
    PAUSE = "Pause"
    TERMINATE = "Terminate"
    LEFT = "Left"
    RIGHT = "Right"
    UP = "Up"
    DOWN = "Down"
    ACTION = "Action"


# Тип для матрицы поля / следующей фигуры
Matrix = List[List[bool]]


@dataclass(frozen=True)
class State:
    """
    Снимок состояния игры для отрисовки в интерфейсе.
    Поля и их порядок соответствуют State из спецификации.
    """

    field: Matrix
    next: Matrix
    score: int
    high_score: int
    level: int
    speed: int
    pause: bool


class GameProtocol(Protocol):
    """
    Универсальный интерфейс библиотеки игры.
    Все игры (гонки, тетрис, змейка и т.д.) должны реализовать этот протокол.
    """

    def user_input(self, action: Action, hold: bool) -> None:
        """Обработать пользовательский ввод."""

        ...

    def update_current_state(self) -> State:
        """Вернуть текущее состояние игры для отрисовки."""

        ...
