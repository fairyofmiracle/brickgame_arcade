from __future__ import annotations

import ctypes
from pathlib import Path

from core import Action, FIELD_HEIGHT, FIELD_WIDTH, Matrix, State


LIB_PATH = (
    Path(__file__).resolve().parents[2]
    / "native_bridge"
    / "libbrickgames.so"
)

_lib: ctypes.CDLL | None = None


class CState(ctypes.Structure):
    _fields_ = [
        ("field", ctypes.c_int * (FIELD_HEIGHT * FIELD_WIDTH)),
        ("next", ctypes.c_int * (FIELD_HEIGHT * FIELD_WIDTH)),
        ("score", ctypes.c_int),
        ("high_score", ctypes.c_int),
        ("level", ctypes.c_int),
        ("speed", ctypes.c_int),
        ("pause", ctypes.c_int),
        ("game_over", ctypes.c_int),
    ]


def _get_lib() -> ctypes.CDLL:
    global _lib
    if _lib is None:
        _lib = ctypes.CDLL(str(LIB_PATH))
        _lib.tetris_init.restype = None
        _lib.tetris_user_input.argtypes = (ctypes.c_int, ctypes.c_int)
        _lib.tetris_user_input.restype = None
        _lib.tetris_update_state.argtypes = (ctypes.POINTER(CState),)
        _lib.tetris_update_state.restype = None
    return _lib


ACTION_MAP = {
    Action.START: 0,
    Action.PAUSE: 1,
    Action.TERMINATE: 2,
    Action.LEFT: 3,
    Action.RIGHT: 4,
    Action.UP: 5,
    Action.DOWN: 6,
    Action.ACTION: 7,
}


def _matrix_from_flat(flat) -> Matrix:
    return [
        [bool(flat[r * FIELD_WIDTH + c]) for c in range(FIELD_WIDTH)]
        for r in range(FIELD_HEIGHT)
    ]


def _colors_from_flat(flat) -> list[list[int]]:
    return [
        [int(flat[r * FIELD_WIDTH + c]) for c in range(FIELD_WIDTH)]
        for r in range(FIELD_HEIGHT)
    ]


class TetrisCProxy:
    def __init__(self) -> None:
        _get_lib().tetris_init()
        self._last_cstate: CState | None = None

    def user_input(self, action: Action, hold: bool) -> None:
        _get_lib().tetris_user_input(ACTION_MAP[action], int(hold))

    def update_current_state(self) -> State:
        cstate = CState()
        _get_lib().tetris_update_state(ctypes.byref(cstate))
        self._last_cstate = cstate
        field = _matrix_from_flat(cstate.field)
        nxt = _matrix_from_flat(cstate.next)
        return State(
            field=field,
            next=nxt,
            score=cstate.score,
            high_score=cstate.high_score,
            level=cstate.level,
            speed=cstate.speed,
            pause=bool(cstate.pause),
        )

    def get_render_layers(self) -> dict[str, object]:
        if self._last_cstate is None:
            return {}
        return {
            "game_over": bool(self._last_cstate.game_over),
            "field_colors": _colors_from_flat(self._last_cstate.field),
            "next_colors": _colors_from_flat(self._last_cstate.next),
        }
