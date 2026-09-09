from __future__ import annotations

import os
from enum import IntEnum
from typing import Any, Dict, List, Optional
from pathlib import Path

from fastapi import FastAPI, HTTPException, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from core import Action, GameProtocol, State, ensure_scores_dir
from brick_game.race.engine import RaceGame
from brick_game.tetris.c_bridge import TetrisCProxy
from brick_game.snake.c_bridge import SnakeCProxy
from brick_game.arcanoid.engine import ArcanoidGame
from brick_game.server.leaderboard import (
    get_leaderboard,
    init_db as init_leaderboard_db,
    submit_score,
    validate_nickname,
)
from brick_game.server.mascot import MascotSayRequest, mascot_say


class ErrorMessage(BaseModel):
    message: str


class GameInfo(BaseModel):
    id: int
    name: str


class GamesList(BaseModel):
    games: List[GameInfo]


class UserActionBody(BaseModel):
    action_id: int
    hold: bool


class MascotSayBody(BaseModel):
    nickname: str = ""
    game_id: Optional[int] = None
    event: str = "hint"
    screen: str = "menu"
    score: int = 0
    level: int = 1
    pause: bool = False
    game_over: bool = False


class MascotSayResponse(BaseModel):
    lines: List[str]
    source: str


class LeaderboardEntry(BaseModel):
    nickname: str
    score: int
    level: int = 1
    created_at: str


class LeaderboardGameBlock(BaseModel):
    game_id: int
    name: str
    entries: List[LeaderboardEntry]


class LeaderboardResponse(BaseModel):
    games: List[LeaderboardGameBlock]


class LeaderboardSubmitBody(BaseModel):
    nickname: str
    game_id: int
    score: int
    level: int = 1


class GameStateBody(BaseModel):
    field: List[List[bool]]
    next: List[List[bool]]
    score: int
    high_score: int
    level: int
    speed: int
    pause: bool
    # цветовые слои для UI (0=пусто, >0=тип клетки)
    field_colors: Optional[List[List[int]]] = None
    next_colors: Optional[List[List[int]]] = None
    # дополнительные поля для UI (спецификации не мешают, JSON допускает
    # лишние поля)
    game_over: bool = False
    status: str = "running"
    nitro: Optional[bool] = None
    lives: Optional[int] = None
    # примитивы для canvas-рендера (арканоид и т.п.)
    arcanoid: Optional[Dict[str, Any]] = None

    @classmethod
    def from_state(cls, state: State) -> "GameStateBody":
        return cls(
            field=state.field,
            next=state.next,
            score=state.score,
            high_score=state.high_score,
            level=state.level,
            speed=state.speed,
            pause=state.pause,
        )


class GameId(IntEnum):
    RACE = 1
    TETRIS = 2
    SNAKE = 3
    ARCANOID = 4


class ActionId(IntEnum):
    START = 0
    PAUSE = 1
    TERMINATE = 2
    LEFT = 3
    RIGHT = 4
    UP = 5
    DOWN = 6
    ACTION = 7

    def to_action(self) -> Action:
        mapping = {
            ActionId.START: Action.START,
            ActionId.PAUSE: Action.PAUSE,
            ActionId.TERMINATE: Action.TERMINATE,
            ActionId.LEFT: Action.LEFT,
            ActionId.RIGHT: Action.RIGHT,
            ActionId.UP: Action.UP,
            ActionId.DOWN: Action.DOWN,
            ActionId.ACTION: Action.ACTION,
        }
        return mapping[self]


_games_registry: Dict[GameId, str] = {
    GameId.RACE: "Racing",
    GameId.TETRIS: "Tetris",
    GameId.SNAKE: "Snake",
    GameId.ARCANOID: "Arcanoid",
}

_current_game: Optional[GameProtocol] = None
_current_game_id: Optional[GameId] = None


def _error_body(detail: Any) -> Dict[str, str]:
    """
    Тело ошибки в формате спецификации OpenAPI.

    Соответствует `components.schemas.ErrorMessage`.
    """
    if isinstance(detail, str):
        return {"message": detail}
    if isinstance(detail, list):
        parts: List[str] = []
        for item in detail:
            if isinstance(item, dict):
                loc = item.get("loc", ())
                msg = item.get("msg", "")
                loc_s = ".".join(str(x) for x in loc) if loc else ""
                parts.append(f"{loc_s}: {msg}" if loc_s else str(msg))
            else:
                parts.append(str(item))
        return {
            "message": "; ".join(parts) if parts else "Некорректный запрос"
        }
    return {"message": str(detail)}


def create_app() -> FastAPI:
    # Нативные мосты (тетрис/змейка) пишут рекорды в data/ относительно корня
    # репозитория
    repo_root = Path(__file__).resolve().parents[3]
    try:
        os.chdir(repo_root)
    except OSError:
        pass
    ensure_scores_dir()
    try:
        init_leaderboard_db()
    except OSError:
        pass

    app = FastAPI(
        title="BrickGame",
        version="1.0",
        description="BrickGame 3.0 REST API (FastAPI implementation).",
    )

    @app.exception_handler(HTTPException)
    async def _http_exception_handler(
            _request: Request,
            exc: HTTPException,
    ) -> JSONResponse:
        return JSONResponse(
            status_code=exc.status_code,
            content=_error_body(exc.detail),
        )

    # В спецификации для ошибок тела запроса указан код 400,
    # а не 422 Unprocessable Entity.
    @app.exception_handler(RequestValidationError)
    async def _validation_exception_handler(
            _request: Request,
            exc: RequestValidationError,
    ) -> JSONResponse:
        return JSONResponse(
            status_code=400,
            content=_error_body(exc.errors()),
        )

    # .../src/brick_game
    src_root = Path(__file__).resolve().parents[1]
    static_dir = src_root / "gui" / "web" / "static"

    # Раздача статики веб-клиента
    app.mount(
        "/static",
        StaticFiles(directory=str(static_dir)),
        name="static",
    )

    @app.get("/", response_class=HTMLResponse)
    def index() -> HTMLResponse:
        """Отдать главную страницу веб-клиента."""
        with open(static_dir / "index.html", "r", encoding="utf-8") as f:
            return HTMLResponse(content=f.read())

    @app.get(
        "/games",
        response_model=GamesList,
        responses={"500": {"model": ErrorMessage}},
    )
    def list_games() -> GamesList:
        games = [GameInfo(id=int(gid), name=name)
                 for gid, name in _games_registry.items()]
        return GamesList(games=games)

    @app.post(
        "/games/{gameId}",
        responses={
            200: {"description": "Игра запущена"},
            404: {"model": ErrorMessage},
            409: {"model": ErrorMessage},
            500: {"model": ErrorMessage},
        },
    )
    def select_game(gameId: int):
        global _current_game, _current_game_id

        try:
            gid = GameId(gameId)
        except ValueError:
            raise HTTPException(status_code=404, detail="Игра не найдена")

        # Спецификация требует 409, если запущена другая игра.
        # Важно: не менять `_current_game` в конфликте.
        if _current_game_id is not None and _current_game_id != gid:
            raise HTTPException(
                status_code=409,
                detail="Другая игра уже запущена",
            )

        factories = {
            GameId.RACE: RaceGame,
            GameId.TETRIS: TetrisCProxy,
            GameId.SNAKE: SnakeCProxy,
            GameId.ARCANOID: ArcanoidGame,
        }
        _current_game = factories[gid]()
        _current_game_id = gid

        # Автоматически переводим игру в начальное состояние
        _current_game.user_input(Action.START, hold=False)
        return {}

    @app.delete("/games")
    def stop_game():
        global _current_game, _current_game_id
        _current_game = None
        _current_game_id = None
        return {}

    @app.delete("/api/games")
    def stop_game_api():
        return stop_game()

    # Совместимость со спецификацией: все эндпоинты дублируются под `/api/...`.
    @app.get(
        "/api/games",
        response_model=GamesList,
        responses={"500": {"model": ErrorMessage}},
    )
    def list_games_api() -> GamesList:
        return list_games()

    @app.post(
        "/api/games/{gameId}",
        responses={
            200: {"description": "Игра запущена"},
            404: {"model": ErrorMessage},
            409: {"model": ErrorMessage},
            500: {"model": ErrorMessage},
        },
    )
    def select_game_api(gameId: int):
        return select_game(gameId)

    @app.post(
        "/api/actions",
        responses={
            200: {"description": "Действие выполнено"},
            400: {"model": ErrorMessage},
            500: {"model": ErrorMessage},
        },
    )
    def perform_action_api(body: UserActionBody):
        return perform_action(body)

    @app.get(
        "/api/state",
        response_model=GameStateBody,
        responses={
            400: {"model": ErrorMessage},
            500: {"model": ErrorMessage},
        },
    )
    def get_state_api() -> GameStateBody:
        return get_state()

    @app.post(
        "/api/mascot/say",
        response_model=MascotSayResponse,
        responses={
            400: {"model": ErrorMessage},
            500: {"model": ErrorMessage},
        },
    )
    async def mascot_say_api(
            body: MascotSayBody,
            request: Request,
    ) -> MascotSayResponse:
        client_ip = request.client.host if request.client else "unknown"
        req = MascotSayRequest(
            nickname=body.nickname,
            game_id=body.game_id,
            event=body.event,
            screen=body.screen,
            score=body.score,
            level=body.level,
            pause=body.pause,
            game_over=body.game_over,
        )
        lines, source = await mascot_say(req, client_ip=client_ip)
        return MascotSayResponse(lines=lines, source=source)

    @app.get(
        "/api/leaderboard",
        response_model=LeaderboardResponse,
        responses={"400": {"model": ErrorMessage}},
    )
    def leaderboard_get(
            game_id: Optional[int] = None,
            limit: int = 5,
    ) -> LeaderboardResponse:
        try:
            data = get_leaderboard(game_id=game_id, limit=limit)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return LeaderboardResponse(**data)

    @app.post(
        "/api/leaderboard",
        responses={
            200: {"description": "Результат сохранён"},
            400: {"model": ErrorMessage},
        },
    )
    def leaderboard_post(body: LeaderboardSubmitBody) -> Dict[str, Any]:
        try:
            return submit_score(
                body.nickname,
                body.game_id,
                body.score,
                body.level,
            )
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

    @app.post(
        "/actions",
        responses={
            200: {"description": "Действие выполнено"},
            400: {"model": ErrorMessage},
            500: {"model": ErrorMessage},
        },
    )
    def perform_action(body: UserActionBody):
        if _current_game is None:
            raise HTTPException(status_code=400, detail="Игра не запущена")

        try:
            action_enum = ActionId(body.action_id)
        except ValueError:
            raise HTTPException(
                status_code=400, detail="Неизвестный action_id")

        _current_game.user_input(action_enum.to_action(), hold=body.hold)
        return {}

    @app.get(
        "/state",
        response_model=GameStateBody,
        responses={
            400: {"model": ErrorMessage},
            500: {"model": ErrorMessage},
        },
    )
    def get_state() -> GameStateBody:
        if _current_game is None:
            raise HTTPException(status_code=400, detail="Игра не запущена")

        state = _current_game.update_current_state()
        body = GameStateBody.from_state(state)

        # дополнительные слои рендера (если игра умеет)
        if hasattr(_current_game, "get_render_layers"):
            try:
                layers: Dict[str, Any] = getattr(
                    _current_game, "get_render_layers")()  # type: ignore[misc]
            except Exception:
                layers = {}

            if isinstance(layers, dict):
                if "field_colors" in layers:
                    body.field_colors = layers["field_colors"]
                if "next_colors" in layers:
                    body.next_colors = layers["next_colors"]
                if "game_over" in layers:
                    body.game_over = bool(layers["game_over"])
                if "status" in layers:
                    body.status = str(layers["status"])
                if "nitro" in layers:
                    body.nitro = bool(layers["nitro"])
                if "lives" in layers:
                    body.lives = int(layers["lives"])
                if "arcanoid" in layers:
                    body.arcanoid = layers["arcanoid"]

        # fallback для тех игр, где есть _state.status
        if hasattr(_current_game, "_state"):
            s = getattr(_current_game, "_state")
            if hasattr(s, "game_over"):
                body.game_over = bool(getattr(s, "game_over"))
            if hasattr(s, "status"):
                status = getattr(s, "status")
                body.status = str(getattr(status, "value", status))

        return body

    return app


app = create_app()

if __name__ == "__main__":
    # Локальный запуск:
    #   uvicorn brick_game.server.app:app --reload --port 8005
    import uvicorn

    uvicorn.run("brick_game.server.app:app",
                host="0.0.0.0", port=8005, reload=True)
