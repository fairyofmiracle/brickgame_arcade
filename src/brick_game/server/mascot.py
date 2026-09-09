from __future__ import annotations

import os
import random
import time
from typing import Dict, List, Optional, Tuple

import httpx

GAME_NAMES = {
    1: "Гонки",
    2: "Тетрис",
    3: "Змейка",
    4: "Арканоид",
}

# Simple in-memory rate limit: IP -> list of request timestamps.
_RATE_BUCKET: Dict[str, List[float]] = {}
_RATE_LIMIT = 20
_RATE_WINDOW_S = 60.0


class MascotSayRequest:
    __slots__ = (
        "nickname",
        "game_id",
        "event",
        "screen",
        "score",
        "level",
        "pause",
        "game_over",
    )

    def __init__(
        self,
        nickname: str = "",
        game_id: Optional[int] = None,
        event: str = "hint",
        screen: str = "menu",
        score: int = 0,
        level: int = 1,
        pause: bool = False,
        game_over: bool = False,
    ) -> None:
        self.nickname = nickname
        self.game_id = game_id
        self.event = event
        self.screen = screen
        self.score = score
        self.level = level
        self.pause = pause
        self.game_over = game_over


def _check_rate_limit(client_ip: str) -> None:
    now = time.time()
    bucket = _RATE_BUCKET.setdefault(client_ip, [])
    cutoff = now - _RATE_WINDOW_S
    _RATE_BUCKET[client_ip] = [t for t in bucket if t >= cutoff]
    if len(_RATE_BUCKET[client_ip]) >= _RATE_LIMIT:
        raise ValueError("rate_limit")
    _RATE_BUCKET[client_ip].append(now)


def _split_lines(text: str, max_lines: int = 3, max_len: int = 42) -> List[str]:
    raw = [ln.strip() for ln in text.replace("\r", "").split("\n") if ln.strip()]
    if not raw:
        raw = [text.strip()] if text.strip() else ["Удачи!"]
    lines: List[str] = []
    for part in raw:
        if len(part) <= max_len:
            lines.append(part)
        else:
            words = part.split()
            cur = ""
            for w in words:
                chunk = f"{cur} {w}".strip()
                if len(chunk) <= max_len:
                    cur = chunk
                else:
                    if cur:
                        lines.append(cur)
                    cur = w[:max_len]
            if cur:
                lines.append(cur)
        if len(lines) >= max_lines:
            break
    return lines[:max_lines] or ["Вперёд!"]


def _fallback_lines(req: MascotSayRequest) -> List[str]:
    nick = (req.nickname or "друг").strip()[:20] or "друг"
    game = GAME_NAMES.get(int(req.game_id or 0), "BrickGame")
    event = (req.event or "hint").lower()

    pools: Dict[str, List[List[str]]] = {
        "menu_hello": [
            [f"Привет, {nick}!", "Я Алёна — твоя", "помощница в BrickGame ♥"],
            ["Рада видеть тебя!", "Выбери игру —", "подскажу по ходу дела."],
        ],
        "menu_click": [
            ["Меня зовут Алёна!", "Проекты сделаны", "в Школе 21."],
            [f"Привет, {nick}!", "Спроси совет —", "я рядом на панели."],
        ],
        "game_start": [
            [f"Поехали в {game}!", "Дыши ровно и", "смотри на поле."],
            [f"Старт: {game}!", "Первые очки —", "самые важные."],
        ],
        "level_up": [
            [f"Уровень {req.level}!", "Шар/темп быстрее —", "держи центр."],
            [f"Новый уровень {req.level}!", "Ты молодец,", "не расслабляйся!"],
        ],
        "game_over": [
            [f"Счёт {req.score}.", "Было круто!", "Ещё раунд?"],
            ["Не сдалась?", "Нажми R —", "и снова в бой!"],
        ],
        "pause": [
            ["Пауза — ок.", "Отдохни секунду", "и продолжай."],
            ["Стоим.", "P — снять", "с паузы."],
        ],
        "hint": {
            1: [
                ["Держи полосу!", "Нитро — только", "когда видишь дальше."],
                ["Перестраивайся", "заранее — враги", "не ждут."],
            ],
            2: [
                ["Собирай линии", "снизу — так", "безопаснее."],
                ["Смотри на next", "и планируй", "поворот заранее."],
            ],
            3: [
                ["Не крутися", "в тупике —", "думай на шаг вперёд."],
                ["Еда на краю?", "Обведи кольцом", "без риска."],
            ],
            4: [
                ["Action — пуск", "шара. Лови", "отскоки центром."],
                ["Каждые 5 очков", "— уровень.", "Скорость растёт!"],
            ],
        },
        "click": [
            [f"Слушаю, {nick}!", "Сейчас подскажу", "по игре."],
            ["Тык по мне?", "Лови совет", "от сердца ♥"],
        ],
    }

    if event == "hint" and req.game_id in pools["hint"]:
        choices = pools["hint"][int(req.game_id)]
        return random.choice(choices)

    choices = pools.get(event) or pools["hint"].get(4, [[ "Вперёд!" ]])
    if isinstance(choices, dict):
        choices = choices.get(int(req.game_id or 4), [["Вперёд!"]])
    return random.choice(choices)


async def _llm_lines(req: MascotSayRequest) -> Optional[List[str]]:
    api_key = os.environ.get("MASCOT_LLM_API_KEY", "").strip()
    if not api_key:
        return None

    base_url = os.environ.get(
        "MASCOT_LLM_BASE_URL",
        "https://api.openai.com/v1",
    ).rstrip("/")
    model = os.environ.get("MASCOT_LLM_MODEL", "gpt-4o-mini")

    game = GAME_NAMES.get(int(req.game_id or 0), "меню")
    nick = (req.nickname or "игрок").strip()[:20]

    system = (
        "Ты Алёна — дружелюбная пиксельная помощница в игре BrickGame "
        "(Школа 21). Отвечай только по-русски, 2–3 короткие строки "
        "(до 42 символов каждая), без markdown и без эмодзи кроме ♥. "
        "Давай игровые советы, поддержку и лёгкий юмор. "
        "Не выдумывай механики, которых нет."
    )
    user = (
        f"Событие: {req.event}. Экран: {req.screen}. "
        f"Игрок: {nick}. Игра: {game}. "
        f"Счёт: {req.score}. Уровень: {req.level}. "
        f"Пауза: {req.pause}. Game over: {req.game_over}. "
        "Напиши реплику маскота для облачка на Canvas."
    )

    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": user},
        ],
        "max_tokens": 120,
        "temperature": 0.85,
    }

    try:
        async with httpx.AsyncClient(timeout=12.0) as client:
            resp = await client.post(
                f"{base_url}/chat/completions",
                headers={
                    "Authorization": f"Bearer {api_key}",
                    "Content-Type": "application/json",
                },
                json=payload,
            )
            resp.raise_for_status()
            data = resp.json()
        content = (
            data.get("choices", [{}])[0]
            .get("message", {})
            .get("content", "")
        )
        if not content:
            return None
        return _split_lines(str(content))
    except Exception:
        return None


async def mascot_say(
    req: MascotSayRequest,
    client_ip: str = "unknown",
) -> Tuple[List[str], str]:
    try:
        _check_rate_limit(client_ip)
    except ValueError:
        lines = _fallback_lines(req)
        return lines, "fallback"

    llm = await _llm_lines(req)
    if llm:
        return llm, "llm"
    return _fallback_lines(req), "fallback"
