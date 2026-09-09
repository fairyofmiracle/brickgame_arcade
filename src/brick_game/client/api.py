from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Optional

import requests

from core import Action, GameProtocol, Matrix, State


@dataclass(slots=True)
class ApiConfig:
    # Match the server port used by Dockerfile/docker-compose.yml
    base_url: str = "http://127.0.0.1:8005"
    timeout_s: float = 5.0


class ActionId(IntEnum):
    START = 0
    PAUSE = 1
    TERMINATE = 2
    LEFT = 3
    RIGHT = 4
    UP = 5
    DOWN = 6
    ACTION = 7

    @classmethod
    def from_action(cls, action: Action) -> "ActionId":
        mapping = {
            Action.START: cls.START,
            Action.PAUSE: cls.PAUSE,
            Action.TERMINATE: cls.TERMINATE,
            Action.LEFT: cls.LEFT,
            Action.RIGHT: cls.RIGHT,
            Action.UP: cls.UP,
            Action.DOWN: cls.DOWN,
            Action.ACTION: cls.ACTION,
        }
        return mapping[action]


class BrickGameApiClient(GameProtocol):
    """
    REST-клиент к Python-серверу BrickGame.
    Реализует универсальный интерфейс `GameProtocol`.
    """

    def __init__(self, config: Optional[ApiConfig] = None) -> None:
        self._config = config or ApiConfig()
        self._session = requests.Session()
        self._selected: bool = False

    def _url(self, path: str) -> str:
        return f"{self._config.base_url.rstrip('/')}{path}"

    def select_game(self, game_id: int) -> None:
        """
        Выбор игры по REST (POST /games/{gameId}).
        Для гонок используется game_id=1.
        """

        resp = self._session.post(
            self._url(f"/games/{game_id}"),
            timeout=self._config.timeout_s,
        )
        if resp.status_code == 200:
            self._selected = True
            return
        if resp.status_code == 404:
            raise RuntimeError("Игра с таким идентификатором не найдена")
        if resp.status_code == 409:
            raise RuntimeError("Уже запущена другая игра")
        raise RuntimeError(
            f"Ошибка сервера при выборе игры: {resp.status_code} {resp.text}")

    def user_input(self, action: Action, hold: bool) -> None:
        if not self._selected:
            # по спецификации это 400 Bad Request
            raise RuntimeError(
                "Игра не выбрана. Сначала вызови select_game().")

        action_id = int(ActionId.from_action(action))
        payload = {"action_id": action_id, "hold": hold}
        resp = self._session.post(
            self._url("/actions"),
            json=payload,
            timeout=self._config.timeout_s,
        )
        if resp.status_code == 200:
            return
        if resp.status_code == 400:
            raise RuntimeError(
                f"Ошибка запроса или игра не запущена: {resp.text}")
        raise RuntimeError(
            f"Ошибка сервера при отправке действия: {
                resp.status_code} {
                resp.text}")

    def update_current_state(self) -> State:
        if not self._selected:
            raise RuntimeError(
                "Игра не выбрана. Сначала вызови select_game().")

        resp = self._session.get(
            self._url("/state"),
            timeout=self._config.timeout_s,
        )
        if resp.status_code == 400:
            raise RuntimeError("Игра не запущена на сервере")
        if resp.status_code != 200:
            raise RuntimeError(
                f"Ошибка сервера при запросе состояния: {
                    resp.status_code} {
                    resp.text}")

        data = resp.json()
        field: Matrix = data.get("field", [])
        next_field: Matrix = data.get("next", [])
        return State(
            field=field,
            next=next_field,
            score=int(data.get("score", 0)),
            high_score=int(data.get("high_score", 0)),
            level=int(data.get("level", 0)),
            speed=int(data.get("speed", 0)),
            pause=bool(data.get("pause", False)),
        )
