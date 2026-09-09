from __future__ import annotations

import re
import sqlite3
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

GAME_NAMES = {
    1: "Гонки",
    2: "Тетрис",
    3: "Змейка",
    4: "Арканоид",
}

_NICK_RE = re.compile(r"^[a-zA-Z0-9_]{3,20}$")
_MAX_SCORE = 9_999_999


def _db_path() -> Path:
    root = Path(__file__).resolve().parents[3]
    data = root / "data"
    data.mkdir(parents=True, exist_ok=True)
    return data / "leaderboard.db"


def _connect() -> sqlite3.Connection:
    conn = sqlite3.connect(str(_db_path()))
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    with _connect() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS scores (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                nickname TEXT NOT NULL,
                game_id INTEGER NOT NULL,
                score INTEGER NOT NULL,
                level INTEGER NOT NULL DEFAULT 1,
                created_at TEXT NOT NULL
            )
            """
        )
        cols = {
            str(r[1])
            for r in conn.execute("PRAGMA table_info(scores)").fetchall()
        }
        if "level" not in cols:
            conn.execute(
                "ALTER TABLE scores ADD COLUMN level INTEGER NOT NULL DEFAULT 1"
            )
        conn.execute(
            """
            CREATE INDEX IF NOT EXISTS idx_scores_game_score
            ON scores (game_id, score DESC)
            """
        )
        conn.commit()


def validate_nickname(nickname: str) -> str:
    nick = (nickname or "").strip()
    if not _NICK_RE.match(nick):
        raise ValueError("Ник: латиница, цифры, _, 3–20 символов")
    return nick


def submit_score(
    nickname: str,
    game_id: int,
    score: int,
    level: int = 1,
) -> Dict[str, Any]:
    nick = validate_nickname(nickname)
    gid = int(game_id)
    if gid not in GAME_NAMES:
        raise ValueError("Неизвестная игра")
    pts = int(score)
    if pts < 0 or pts > _MAX_SCORE:
        raise ValueError("Некорректный счёт")
    lvl = int(level)
    if lvl < 1 or lvl > 999:
        raise ValueError("Некорректный уровень")

    now = datetime.now(timezone.utc).isoformat()
    with _connect() as conn:
        conn.execute(
            """
            INSERT INTO scores (nickname, game_id, score, level, created_at)
            VALUES (?, ?, ?, ?, ?)
            """,
            (nick, gid, pts, lvl, now),
        )
        conn.commit()
    return {
        "ok": True,
        "nickname": nick,
        "game_id": gid,
        "score": pts,
        "level": lvl,
    }


def get_leaderboard(
    game_id: Optional[int] = None,
    limit: int = 5,
) -> Dict[str, Any]:
    lim = max(1, min(50, int(limit)))
    games: List[Dict[str, Any]] = []

    if game_id is not None:
        ids = [int(game_id)]
    else:
        ids = sorted(GAME_NAMES.keys())

    with _connect() as conn:
        for gid in ids:
            if gid not in GAME_NAMES:
                continue
            rows = conn.execute(
                """
                SELECT nickname, score, level, created_at
                FROM scores
                WHERE game_id = ?
                ORDER BY score DESC, id ASC
                LIMIT ?
                """,
                (gid, lim),
            ).fetchall()
            games.append(
                {
                    "game_id": gid,
                    "name": GAME_NAMES[gid],
                    "entries": [
                        {
                            "nickname": str(r["nickname"]),
                            "score": int(r["score"]),
                            "level": int(r["level"] or 1),
                            "created_at": str(r["created_at"]),
                        }
                        for r in rows
                    ],
                }
            )

    return {"games": games}
