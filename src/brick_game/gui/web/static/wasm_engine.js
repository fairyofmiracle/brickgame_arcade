// Emscripten-backed engine for legacy C/C++ games (Tetris, Snake).
//
// Expected build artifacts (served as static files):
// - /static/wasm/brickgame.js
// - /static/wasm/brickgame.wasm
//
// The Emscripten module must export:
// - _bg_init, _bg_input, _bg_tick
// - _bg_field_ptr, _bg_colors_ptr, _bg_next_ptr, _bg_next_size, _bg_meta_ptr

function _loadScriptOnce(src) {
  return new Promise((resolve, reject) => {
    const existing = document.querySelector(`script[data-src="${src}"]`);
    if (existing) {
      existing.addEventListener('load', () => resolve());
      existing.addEventListener('error', (e) => reject(e));
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

export class WasmEngine {
  constructor() {
    this._ready = false;
    this._module = null;
    this._gameId = null;
  }

  supportsGame(gameId) {
    return Number(gameId) === 2 || Number(gameId) === 3;
  }

  async init() {
    if (this._ready) return;
    await _loadScriptOnce('/static/wasm/brickgame.js');

    // The script should define a factory `createBrickgameModule`.
    if (typeof createBrickgameModule !== 'function') {
      throw new Error('WASM module factory createBrickgameModule() not found');
    }
    this._module = await createBrickgameModule();
    this._ready = true;
  }

  async selectGame(gameId) {
    await this.init();
    const gid = Number(gameId);
    if (!this.supportsGame(gid)) {
      throw new Error(`WASM engine does not support game_id=${gid}`);
    }
    const ok = this._module._bg_init(gid);
    if (!ok) throw new Error(`bg_init(${gid}) failed`);
    this._gameId = gid;
  }

  async restartGame() {
    if (!this._gameId) return;
    await this.selectGame(this._gameId);
  }

  async sendAction(actionId, hold) {
    await this.init();
    if (!this._gameId) return;
    this._module._bg_input(Number(actionId), hold ? 1 : 0);
  }

  async fetchState() {
    await this.init();
    if (!this._gameId) return null;

    this._module._bg_tick();

    const M = this._module;
    const fieldPtr = M._bg_field_ptr();
    const colorsPtr = M._bg_colors_ptr();
    const nextPtr = M._bg_next_ptr();
    const metaPtr = M._bg_meta_ptr();
    const nextSize = M._bg_next_size();

    const fieldFlat = M.HEAPU8.subarray(fieldPtr, fieldPtr + 200);
    const colorsFlat = M.HEAPU8.subarray(colorsPtr, colorsPtr + 200);
    const nextFlat = M.HEAPU8.subarray(nextPtr, nextPtr + 16);
    const meta = M.HEAP32.subarray(metaPtr >> 2, (metaPtr >> 2) + 8);

    // Convert flat arrays to 2D matrices compatible with existing UI/state.
    const field = [];
    const field_colors = [];
    for (let r = 0; r < 20; r++) {
      const row = [];
      const crow = [];
      for (let c = 0; c < 10; c++) {
        const idx = r * 10 + c;
        row.push(Boolean(fieldFlat[idx]));
        crow.push(Number(colorsFlat[idx] || 0));
      }
      field.push(row);
      field_colors.push(crow);
    }

    // next is 4x4 in UI; we provide it always.
    const next = [];
    for (let r = 0; r < 4; r++) {
      const row = [];
      for (let c = 0; c < 4; c++) {
        row.push(Boolean(nextFlat[r * 4 + c]));
      }
      next.push(row);
    }

    const score = Number(meta[0] || 0);
    const high_score = Number(meta[1] || 0);
    const level = Number(meta[2] || 1);
    const speed = Number(meta[3] || 1);
    const pause = Boolean(meta[4] || 0);
    const game_over = Boolean(meta[5] || 0);

    const state = {
      field,
      next: nextSize ? next : [[false, false, false, false], [false, false, false, false], [false, false, false, false], [false, false, false, false]],
      score,
      high_score,
      level,
      speed,
      pause,
      game_over,
      status: game_over ? 'game_over' : (pause ? 'paused' : 'running'),
      field_colors,
      game_id: Number(this._gameId),
    };

    return state;
  }
}

