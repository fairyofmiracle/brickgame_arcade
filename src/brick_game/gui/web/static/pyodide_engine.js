// Pyodide-backed game engine for the canvas UI (hybrid mode).
//
// Goals:
// - Run Python game logic in the browser (Pyodide) for supported games.
// - Keep the existing REST engine as a fallback.
//
// NOTE: This module intentionally keeps the interface tiny and JSON-friendly.

const PYODIDE_CDN_URL = 'https://cdn.jsdelivr.net/pyodide/v0.25.1/full/pyodide.js';

function _loadScriptOnce(src) {
  return new Promise((resolve, reject) => {
    const existing = document.querySelector(`script[data-src="${src}"]`);
    if (existing) {
      existing.addEventListener('load', () => resolve());
      existing.addEventListener('error', (e) => reject(e));
      // If it already loaded earlier, resolve immediately.
      if (existing.getAttribute('data-loaded') === '1') resolve();
      return;
    }
    const s = document.createElement('script');
    s.src = src;
    s.async = true;
    s.defer = true;
    s.setAttribute('data-src', src);
    s.addEventListener('load', () => {
      s.setAttribute('data-loaded', '1');
      resolve();
    });
    s.addEventListener('error', (e) => reject(e));
    document.head.appendChild(s);
  });
}

async function _fetchText(path) {
  const res = await fetch(path);
  if (!res.ok) throw new Error(`Failed to fetch ${path}: ${res.status}`);
  return await res.text();
}

function _ensureDir(FS, dir) {
  const parts = dir.split('/').filter(Boolean);
  let cur = '';
  for (const p of parts) {
    cur += `/${p}`;
    try {
      FS.mkdir(cur);
    } catch (_e) {
      // ignore EEXIST
    }
  }
}

async function _installProjectSources(pyodide) {
  const FS = pyodide.FS;
  _ensureDir(FS, '/project');
  _ensureDir(FS, '/project/core');
  _ensureDir(FS, '/project/brick_game');
  _ensureDir(FS, '/project/brick_game/arcanoid');
  _ensureDir(FS, '/project/brick_game/race');

  const files = [
    ['/static/py/core/__init__.py', '/project/core/__init__.py'],
    ['/static/py/brick_game/__init__.py', '/project/brick_game/__init__.py'],
    ['/static/py/brick_game/arcanoid/__init__.py', '/project/brick_game/arcanoid/__init__.py'],
    ['/static/py/brick_game/arcanoid/models.py', '/project/brick_game/arcanoid/models.py'],
    ['/static/py/brick_game/arcanoid/engine.py', '/project/brick_game/arcanoid/engine.py'],
    ['/static/py/brick_game/race/__init__.py', '/project/brick_game/race/__init__.py'],
    ['/static/py/brick_game/race/models.py', '/project/brick_game/race/models.py'],
    ['/static/py/brick_game/race/engine.py', '/project/brick_game/race/engine.py'],
  ];

  await Promise.all(
    files.map(async ([url, dst]) => {
      const content = await _fetchText(url);
      FS.writeFile(dst, content, { encoding: 'utf8' });
    }),
  );

  // Make /project importable.
  pyodide.runPython(`
import sys
if "/project" not in sys.path:
    sys.path.insert(0, "/project")
`);
}

const _PY_HELPER = String.raw`
from __future__ import annotations

from typing import Any, Dict, Optional

from core import Action, State

_game = None
_game_id: Optional[int] = None

_ACTION_MAP = {
    0: Action.START,
    1: Action.PAUSE,
    2: Action.TERMINATE,
    3: Action.LEFT,
    4: Action.RIGHT,
    5: Action.UP,
    6: Action.DOWN,
    7: Action.ACTION,
}

def _state_to_dict(s: State) -> Dict[str, Any]:
    return {
        "field": s.field,
        "next": s.next,
        "score": int(s.score),
        "high_score": int(s.high_score),
        "level": int(s.level),
        "speed": int(s.speed),
        "pause": bool(s.pause),
    }

def init_game(game_id: int) -> Dict[str, Any]:
    global _game, _game_id
    gid = int(game_id)
    if gid == 4:
        from brick_game.arcanoid.engine import ArcanoidGame
        _game = ArcanoidGame()
    elif gid == 1:
        from brick_game.race.engine import RaceGame
        _game = RaceGame()
    else:
        raise ValueError(f"Unsupported game_id for Pyodide engine: {gid}")

    _game_id = gid
    _game.user_input(Action.START, hold=False)
    return {"ok": True, "game_id": gid}

def user_input(action_id: int, hold: bool) -> Dict[str, Any]:
    if _game is None:
        raise RuntimeError("Game is not initialized")
    aid = int(action_id)
    if aid not in _ACTION_MAP:
        raise ValueError(f"Unknown action_id: {aid}")
    _game.user_input(_ACTION_MAP[aid], hold=bool(hold))
    return {"ok": True}

def tick() -> Dict[str, Any]:
    if _game is None:
        raise RuntimeError("Game is not initialized")
    s = _game.update_current_state()
    body = _state_to_dict(s)

    layers: Dict[str, Any] = {}
    if hasattr(_game, "get_render_layers"):
        try:
            layers = getattr(_game, "get_render_layers")() or {}
        except Exception:
            layers = {}

    if isinstance(layers, dict):
        body.update(layers)
        if "arcanoid" in layers:
            body["arcanoid"] = layers["arcanoid"]

    # Harmonize with REST payload fields used by UI.
    body["game_id"] = int(_game_id or 0)
    if "game_over" not in body and hasattr(_game, "_state"):
        st = getattr(_game, "_state")
        if hasattr(st, "game_over"):
            body["game_over"] = bool(getattr(st, "game_over"))
        if hasattr(st, "status"):
            status = getattr(st, "status")
            body["status"] = str(getattr(status, "value", status))

    return body
`;

export class PyodideEngine {
  constructor() {
    this._pyodide = null;
    this._ready = false;
    this._gameId = null;
  }

  get isReady() {
    return this._ready;
  }

  supportsGame(gameId) {
    // Currently: race(1), arcanoid(4) are pure-python.
    return Number(gameId) === 1 || Number(gameId) === 4;
  }

  async init() {
    if (this._ready) return;
    await _loadScriptOnce(PYODIDE_CDN_URL);
    if (typeof loadPyodide !== 'function') {
      throw new Error('Pyodide loader is not available');
    }

    this._pyodide = await loadPyodide({
      // indexURL must match the directory containing pyodide.js
      indexURL: PYODIDE_CDN_URL.replace(/\/pyodide\.js$/, '/'),
    });

    await _installProjectSources(this._pyodide);
    this._pyodide.runPython(_PY_HELPER);
    this._ready = true;
  }

  async selectGame(gameId) {
    await this.init();
    const gid = Number(gameId);
    if (!this.supportsGame(gid)) {
      throw new Error(`Pyodide engine does not support game_id=${gid} yet`);
    }
    this._pyodide.runPython(`init_game(${gid})`);
    this._gameId = gid;
  }

  async restartGame() {
    if (!this._gameId) return;
    await this.selectGame(this._gameId);
  }

  async sendAction(actionId, hold) {
    await this.init();
    this._pyodide.runPython(`user_input(${Number(actionId)}, ${hold ? 'True' : 'False'})`);
  }

  async fetchState() {
    await this.init();
    const out = this._pyodide.runPython('tick()');
    // Convert Python dict -> JS object.
    return out?.toJs ? out.toJs({ dict_converter: Object.fromEntries }) : out;
  }
}

