from __future__ import annotations

import argparse
import curses
import time
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

import requests

from brick_game.client import ActionId


GAME_ID_TETRIS = 2
GAME_ID_SNAKE = 3
GAME_ID_RACE = 1
GAME_ID_ARCANOID = 4


MatrixBool = List[List[bool]]
MatrixInt = List[List[int]]


def _clamp(v: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, v))


def _render_char(
        game_id: int,
        colors: Optional[MatrixInt],
        field: MatrixBool,
        r: int,
        c: int) -> str:
    if colors is not None:
        try:
            v = int(colors[r][c])
        except Exception:
            v = 0
        # snake: 0=empty, 1=snake, 2=food (см. snake_bridge.cpp)
        if v == 0:
            return " "
        if v == 1:
            return "O"
        if v == 2:
            return "*"
    return "█" if field[r][c] else " "


@dataclass
class GameStateView:
    field: MatrixBool
    next: MatrixBool
    score: int
    high_score: int
    level: int
    speed: int
    pause: bool
    game_over: bool
    field_colors: Optional[MatrixInt] = None


class RestClient:
    def __init__(self, base_url: str, timeout_s: float = 5.0) -> None:
        self._base_url = base_url.rstrip("/")
        self._timeout_s = timeout_s
        self._session = requests.Session()

    def _url(self, path: str) -> str:
        return f"{self._base_url}{path}"

    def select_game(self, game_id: int) -> None:
        resp = self._session.post(
            self._url(f"/games/{game_id}"), timeout=self._timeout_s)
        if resp.status_code != 200:
            raise RuntimeError(
                f"POST /games/{game_id} failed: "
                f"{resp.status_code} {resp.text}"
            )

    def user_input(self, action_id: ActionId, hold: bool) -> None:
        payload: Dict[str, Any] = {
            "action_id": int(action_id), "hold": bool(hold)}
        resp = self._session.post(
            self._url("/actions"), json=payload, timeout=self._timeout_s)
        if resp.status_code != 200:
            raise RuntimeError(
                f"POST /actions failed: {resp.status_code} {resp.text}")

    def get_state(self) -> GameStateView:
        resp = self._session.get(self._url("/state"), timeout=self._timeout_s)
        if resp.status_code != 200:
            raise RuntimeError(
                f"GET /state failed: {resp.status_code} {resp.text}")
        data = resp.json()
        field = data.get("field", [])
        next_field = data.get("next", [])
        # field_colors есть у змейки
        field_colors = data.get("field_colors", None)
        return GameStateView(
            field=field,
            next=next_field,
            score=int(data.get("score", 0)),
            high_score=int(data.get("high_score", 0)),
            level=int(data.get("level", 1)),
            speed=int(data.get("speed", 0)),
            pause=bool(data.get("pause", False)),
            game_over=bool(data.get("game_over", False)),
            field_colors=field_colors,
        )


def _occupied_in_next(next_matrix: MatrixBool) -> List[Tuple[int, int]]:
    # На вебе prev рисуется в фиксированном окне 4x4.
    occ: List[Tuple[int, int]] = []
    for r in range(4):
        for c in range(4):
            if r < len(next_matrix) and c < len(
                    next_matrix[r]) and bool(
                    next_matrix[r][c]):
                occ.append((r, c))
    return occ


def _center_to_4x4(occ: List[Tuple[int, int]]) -> List[List[bool]]:
    grid = [[False for _ in range(4)] for _ in range(4)]
    if not occ:
        return grid
    min_r = min(r for r, _ in occ)
    max_r = max(r for r, _ in occ)
    min_c = min(c for _, c in occ)
    max_c = max(c for _, c in occ)
    h = max_r - min_r + 1
    w = max_c - min_c + 1
    off_r = (4 - h) // 2
    off_c = (4 - w) // 2
    for r, c in occ:
        nr = r - min_r + off_r
        nc = c - min_c + off_c
        if 0 <= nr < 4 and 0 <= nc < 4:
            grid[nr][nc] = True
    return grid


def _draw_menu(stdscr: Any, client: RestClient) -> int:
    stdscr.clear()
    stdscr.addstr(2, 2, "BrickGame (REST) - console UI", curses.A_BOLD)
    stdscr.addstr(4, 2, "Выберите игру:")
    # Синхронизируем нумерацию меню с gameId на сервере.
    stdscr.addstr(6, 4, "1) Тетрис")
    stdscr.addstr(7, 4, "2) Змейка")
    stdscr.addstr(8, 4, "3) Гонки")
    stdscr.addstr(9, 4, "4) Арканоид")
    stdscr.addstr(10, 2, "Esc) выход")
    stdscr.refresh()
    while True:
        ch = stdscr.getch()
        if ch == -1:
            time.sleep(0.02)
            continue
        if ch == 27:  # ESC
            return 0
        if ch in (ord("1"), ord("2"), ord("3"), ord("4")):
            game = int(chr(ch))
            # соответствие gameId на сервере (web/REST):
            if game == 1:  # Tetris (menu)
                return GAME_ID_TETRIS
            if game == 2:  # Snake (menu)
                return GAME_ID_SNAKE
            if game == 3:  # Race (menu)
                return GAME_ID_RACE
            if game == 4:  # Arcanoid (menu)
                return GAME_ID_ARCANOID


def _draw_state(stdscr: Any, game_id: int, view: GameStateView,
                next_centered: Optional[List[List[bool]]]) -> None:
    stdscr.erase()
    curses.curs_set(0)

    height = 20
    width = 10

    board_top = 2
    board_left = 2
    # side panel
    # В ncurses-версии C тетриса/змейки каждая клетка печатается
    # как "3 символа" (см. x = 3*j).
    # Поэтому гонки (python) тоже рисуем клетками шириной 3, чтобы визуально
    # было одинаково.
    cell_width = 3 if game_id == GAME_ID_RACE else 1
    board_width_chars = width * cell_width
    side_left = board_left + board_width_chars + 5

    # board
    for r in range(height):
        for c in range(width):
            # Для гонок добавляем явный "правый" разделитель между клетками,
            # иначе соседние занятые клетки визуально слипаются.
            if game_id == GAME_ID_RACE and cell_width == 3:
                on = bool(view.field[r][c])
                # Не используем '#', чтобы визуально не было как "решётка".
                # Каждая клетка должна занимать ровно 3 символа.
                stdscr.addstr(
                    board_top + r,
                    board_left + c * cell_width,
                    "███" if on else "   ",
                )
                continue

            ch = _render_char(game_id, view.field_colors, view.field, r, c)
            # Сериализуем в строку фиксированной ширины, чтобы курсор не
            # "съезжал".
            if cell_width == 1:
                stdscr.addstr(board_top + r, board_left + c, ch)
            else:
                if ch == " ":
                    stdscr.addstr(
                        board_top + r,
                        board_left + c * cell_width,
                        " " * cell_width,
                    )
                else:
                    stdscr.addstr(
                        board_top + r,
                        board_left + c * cell_width,
                        ch * cell_width,
                    )

    # side info
    stdscr.addstr(board_top, side_left, "STATUS:", curses.A_BOLD)
    stdscr.addstr(board_top + 1, side_left, f"Score: {view.score}")
    stdscr.addstr(board_top + 2, side_left, f"High:  {view.high_score}")
    stdscr.addstr(board_top + 3, side_left, f"Level: {view.level}")
    stdscr.addstr(board_top + 4, side_left, f"Speed: {view.speed}")
    stdscr.addstr(board_top + 6, side_left,
                  f"Pause: {'ON' if view.pause else 'OFF'}")
    stdscr.addstr(board_top + 7, side_left,
                  f"GameOver: {'YES' if view.game_over else 'NO'}")

    if game_id == GAME_ID_RACE:
        # lives/nitro не мапятся в StateView, но можно легко расширить при
        # желании
        stdscr.addstr(board_top + 9, side_left, "Keys: ← → Up")

    if game_id == GAME_ID_TETRIS:
        # В C-версии тетриса поворот = стрелка UP, поэтому делаем так же.
        stdscr.addstr(board_top + 9, side_left, "Keys: ← → ↑ ↓")

        if next_centered is not None:
            next_top = board_top + 11
            next_left = side_left
            stdscr.addstr(next_top, next_left, "NEXT", curses.A_BOLD)
            for rr in range(4):
                for cc in range(4):
                    ch = "█" if next_centered[rr][cc] else " "
                    stdscr.addstr(next_top + 1 + rr, next_left + cc, ch)

    if game_id == GAME_ID_ARCANOID:
        stdscr.addstr(board_top + 9, side_left, "Keys: ← → Space/↑")

    # overlays
    if view.pause and not view.game_over:
        title = "PAUSED"
        stdscr.addstr(
            board_top + height // 2,
            board_left + board_width_chars // 2 - (len(title) // 2),
            title,
            curses.A_REVERSE,
        )
    if view.game_over:
        title = "GAME OVER"
        stdscr.addstr(
            board_top + height // 2,
            board_left + board_width_chars // 2 - (len(title) // 2),
            title,
            curses.A_REVERSE,
        )
        msg = "Press R to restart"
        stdscr.addstr(
            board_top + height // 2 + 1,
            board_left + board_width_chars // 2 - (len(msg) // 2),
            msg,
        )

    stdscr.refresh()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-url", default="http://127.0.0.1:8005",
                        help="REST base URL, e.g. http://127.0.0.1:8005")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument(
        "--game-id",
        type=int,
        default=0,
        help=(
            "Принудительно запустить игру: "
            "1=Гонки, 2=Тетрис, 3=Змейка, 4=Арканоид"
        ))
    parser.add_argument("--no-menu", action="store_true",
                        help="Не показывать меню; запуск сразу выбранной игры")
    args = parser.parse_args()

    client = RestClient(base_url=args.base_url, timeout_s=args.timeout)

    def run(stdscr: Any) -> None:
        curses.curs_set(0)
        stdscr.nodelay(True)
        stdscr.keypad(True)

        forced_game_id = args.game_id if (args.no_menu or args.game_id) else 0
        return_to_menu = not args.no_menu

        while True:
            game_id = forced_game_id
            if not game_id:
                game_id = _draw_menu(stdscr, client)
                if game_id == 0:
                    return

            try:
                client.select_game(game_id)
            except Exception as e:
                stdscr.clear()
                stdscr.addstr(2, 2, f"Failed to start game: {e}")
                stdscr.refresh()
                time.sleep(2)
                continue

            next_centered = None
            last_fetch = 0.0
            fetch_interval = 0.2

            # hold emulation
            last_up_press = 0.0
            nitro_sent_hold = False
            last_down_press = 0.0
            soft_drop_sent = False

            while True:
                now = time.time()

                ch = stdscr.getch()
                if ch != -1:
                    # keyboard handling
                    # На русской раскладке "q" -> 'й', "p" -> 'з', "r" -> 'к'
                    # Поэтому добавляем поддержку обоих вариантов, чтобы кнопки
                    # не "ломались".
                    if ch in (ord("q"), ord("Q"), ord("й"), ord("Й")):
                        # "q" должен вести себя как выход в меню
                        # (как остальные игры).
                        if return_to_menu:
                            forced_game_id = 0
                            break
                        # В режиме `--no-menu` (гонки из C-консоли)
                        # просто выходим,
                        # чтобы C-меню показалось снова.
                        return
                    if ch == 27:  # ESC back to menu
                        if return_to_menu:
                            forced_game_id = 0
                            break
                        # В режиме `--no-menu` выходим,
                        # чтобы управление вернулось в C.
                        return
                    if ch in (ord("p"), ord("P"), ord("з"), ord("З")):
                        client.user_input(ActionId.PAUSE, False)
                    if ch in (ord("r"), ord("R"), ord("к"), ord("К")):
                        client.select_game(game_id)  # restart (как в web)

                    # rotations / moves
                    if game_id == GAME_ID_TETRIS:
                        if ch == ord(" ") or ch == curses.KEY_UP:
                            client.user_input(ActionId.UP, False)  # rotate
                        elif ch == curses.KEY_LEFT:
                            client.user_input(ActionId.LEFT, False)
                        elif ch == curses.KEY_RIGHT:
                            client.user_input(ActionId.RIGHT, False)
                        elif ch == curses.KEY_DOWN:
                            # soft drop (send repeatedly)
                            client.user_input(ActionId.DOWN, False)
                            last_down_press = now
                            soft_drop_sent = True
                    elif game_id == GAME_ID_SNAKE:
                        if ch == curses.KEY_UP:
                            client.user_input(ActionId.UP, False)
                        elif ch == curses.KEY_DOWN:
                            client.user_input(ActionId.DOWN, False)
                        elif ch == curses.KEY_LEFT:
                            client.user_input(ActionId.LEFT, False)
                        elif ch == curses.KEY_RIGHT:
                            client.user_input(ActionId.RIGHT, False)
                    elif game_id == GAME_ID_RACE:
                        if ch == curses.KEY_LEFT:
                            client.user_input(ActionId.LEFT, False)
                        elif ch == curses.KEY_RIGHT:
                            client.user_input(ActionId.RIGHT, False)
                        elif ch == curses.KEY_UP:
                            last_up_press = now
                            client.user_input(ActionId.UP, True)
                            nitro_sent_hold = True
                    elif game_id == GAME_ID_ARCANOID:
                        if ch == curses.KEY_LEFT:
                            client.user_input(ActionId.LEFT, False)
                        elif ch == curses.KEY_RIGHT:
                            client.user_input(ActionId.RIGHT, False)
                        elif ch == curses.KEY_UP or ch == ord(" "):
                            client.user_input(ActionId.ACTION, False)

                # emulate nitro "release": если UP не нажимали недавно —
                # отпускаем
                if game_id == GAME_ID_RACE and nitro_sent_hold:
                    if now - last_up_press > 0.12:
                        client.user_input(ActionId.UP, False)
                        nitro_sent_hold = False

                # emulate tetris soft-drop while holding down recently
                if game_id == GAME_ID_TETRIS and soft_drop_sent:
                    if now - last_down_press > 0.12:
                        soft_drop_sent = False
                    else:
                        # повторная отправка раз в ~60мс, чтобы похоже на web
                        # (без знания release)
                        if int((now - last_down_press) / 0.06) >= 1:
                            client.user_input(ActionId.DOWN, False)

                if now - last_fetch >= fetch_interval:
                    try:
                        view = client.get_state()
                    except Exception as e:
                        stdscr.addstr(0, 0, f"State error: {e}"[
                                      :curses.COLS - 1])
                        stdscr.refresh()
                        time.sleep(0.5)
                        continue

                    next_centered = None
                    if game_id == GAME_ID_TETRIS:
                        occ = _occupied_in_next(view.next)
                        next_centered = _center_to_4x4(occ)

                    _draw_state(stdscr, game_id, view, next_centered)
                    last_fetch = now

                    if view.game_over:
                        # ждём ввода R/ESC
                        pass

                time.sleep(0.01)

    curses.wrapper(run)


if __name__ == "__main__":
    main()
