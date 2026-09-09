import { applyRootStyles } from './src/utils.js';
import { rootStyles, keyCodes } from './src/config.js';
import { drawGameFx, drawTetrisPile } from './src/game_fx.js';
import { PyodideEngine } from './pyodide_engine.js';
import { WasmEngine } from './wasm_engine.js';
import {
    fetchLeaderboard,
    getSchoolNick,
    setSchoolNick,
    isValidSchoolNick,
    submitLeaderboardScore,
} from './leaderboard.js';

applyRootStyles(rootStyles);

const API_BASE_URL =
    (window.location && window.location.origin && window.location.origin !== 'null')
        ? window.location.origin
        : 'http://127.0.0.1:8007';

const $canvas = document.querySelector('#game-canvas');
const ctx = $canvas ? $canvas.getContext('2d') : null;

const ActionId = {
    Start: 0,
    Pause: 1,
    Terminate: 2,
    Left: 3,
    Right: 4,
    Up: 5,
    Down: 6,
    Action: 7,
};

// Visual scale of the game field on canvas (does not change logic size 10x20).
const UI_TILE_SCALE = 1.35;

const GAME_THEME = {
    2: { name: 'Тетрис', accent: '#00e5ff' },
    3: { name: 'Змейка', accent: '#2eea6a' },
    1: { name: 'Гонки', accent: '#ff7ad9' },
    4: { name: 'Арканоид', accent: '#cbb7ff' },
};

const BG_THEME = {
    // 2=Tetris, 3=Snake, 1=Racing, 4=Arcanoid
    2: {
        baseFill: '#3949c7',
        star: 'rgba(235,248,255,ALPHA)',
        cloud1: 'rgba(180, 200, 255, ALPHA)',
        cloud2: 'rgba(150, 175, 248, ALPHA)',
        cloud3: 'rgba(205, 225, 255, ALPHA)',
        driftSpeed: 12,
        starCount: 130,
        seed: 1337,
    },
    3: {
        baseFill: '#2b2a61',
        star: 'rgba(235,255,245,ALPHA)',
        cloud1: 'rgba(170, 240, 200, ALPHA)',
        cloud2: 'rgba(120, 200, 165, ALPHA)',
        cloud3: 'rgba(210, 255, 230, ALPHA)',
        driftSpeed: 11,
        starCount: 120,
        seed: 7331,
    },
    1: {
        baseFill: '#4a2a7a',
        star: 'rgba(255,235,252,ALPHA)',
        cloud1: 'rgba(255, 190, 230, ALPHA)',
        cloud2: 'rgba(220, 160, 230, ALPHA)',
        cloud3: 'rgba(255, 210, 245, ALPHA)',
        driftSpeed: 13,
        starCount: 125,
        seed: 2021,
    },
    4: {
        // Current “main” look (purple sky) becomes Arcanoid.
        baseFill: '#5b56b9',
        star: 'rgba(255,255,255,ALPHA)',
        cloud1: 'rgba(190, 182, 255, ALPHA)',
        cloud2: 'rgba(165, 158, 238, ALPHA)',
        cloud3: 'rgba(205, 198, 255, ALPHA)',
        driftSpeed: 12,
        starCount: 120,
        seed: 1337,
    },
};

let gamesList = [];
let selectedGameId = null;
let currentState = null;
let screen = 'menu'; // menu | playing
let buttons = [];
let softDropTimer = null;
let hoverGirl = false;
let girlHitBox = null;
let leaderboardData = [];
let leaderboardFetchedAt = 0;
let nickDraft = getSchoolNick();
let nickFieldFocused = false;
let nickFieldBox = null;
let lastSubmittedScoreKey = '';
let sessionBestScore = 0;
let sessionBestLevel = 1;
let leaderboardNotice = '';
let menuPanel = { ox: 0, oy: 0, w: 0, h: 0 };
let canvasCssW = 0;
let canvasCssH = 0;
let ui = { w: 0, h: 0, ox: 0, oy: 0 };
let hoverButtonIdx = -1;
let anim = { t0: performance.now(), t: 0 };
let menuError = '';
const DEBUG_SKY = false;

// Surface runtime errors inside the canvas UI (helps debugging on Windows + caching issues).
// If anything throws during module init / refresh, you'll see the message in the UI.
window.addEventListener('error', (e) => {
    try {
        const msg = String(e?.message || e?.error?.message || e || 'Unknown error');
        menuError = `JS error: ${msg}`.slice(0, 120);
        render();
    } catch {
        // ignore
    }
});
window.addEventListener('unhandledrejection', (e) => {
    try {
        const r = e?.reason;
        const msg = String(r?.message || r || 'Unhandled rejection');
        menuError = `Promise error: ${msg}`.slice(0, 120);
        render();
    } catch {
        // ignore
    }
});

function highScoreStorageKey(gameId) {
    // Store per-game to avoid mixing leaderboards.
    // Requirement: Arcanoid high score must be stored in LocalStorage.
    return `brickgame.high_score.${Number(gameId)}`;
}

function loadLocalHighScore(gameId) {
    try {
        const raw = localStorage.getItem(highScoreStorageKey(gameId));
        const v = Number.parseInt(String(raw ?? ''), 10);
        return Number.isFinite(v) && v > 0 ? v : 0;
    } catch {
        return 0;
    }
}

function storeLocalHighScore(gameId, value) {
    try {
        localStorage.setItem(
            highScoreStorageKey(gameId),
            String(Math.max(0, Number(value) || 0)),
        );
    } catch {
        // ignore storage errors (private mode, etc.)
    }
}

function syncHighScoreForWeb(state) {
    if (!state || !selectedGameId) return state;
    // Enforce LocalStorage leaderboard for Arcanoid (id=4).
    if (Number(selectedGameId) !== 4) return state;

    const localHigh = loadLocalHighScore(selectedGameId);
    const score = Number(state?.score ?? 0) || 0;
    const engineHigh = Number(state?.high_score ?? 0) || 0;
    const best = Math.max(localHigh, engineHigh, score);
    if (best !== localHigh) storeLocalHighScore(selectedGameId, best);
    state.high_score = best;
    return state;
}

// Engine is selected automatically per game:
// - 2/3: WASM (C/C++)
// - 1/4: Pyodide (Python-in-WASM runtime)
// - fallback: REST
let activeEngine = 'rest'; // rest | pyodide | wasm
const pyodideEngine = new PyodideEngine();
const wasmEngine = new WasmEngine();
// Workaround: ensure sky is painted in-game even if some path skips render().
const FORCE_SKY_IN_GAME = true;
// Menu FX overlay is disabled (keep clean sky background).

function nowSec() {
    return performance.now() / 1000;
}

function tileSizePx() {
    const root = getComputedStyle(document.documentElement);
    const raw = root.getPropertyValue('--tile-size') || '20px';
    const n = Number(String(raw).trim().replace('px', ''));
    return Number.isFinite(n) && n > 0 ? n : 20;
}

function cssColor(name, fallback) {
    const root = getComputedStyle(document.documentElement);
    const v = String(root.getPropertyValue(name) || '').trim();
    return v || fallback;
}

function accentForGame(gameId) {
    const t = GAME_THEME[Number(gameId)];
    return t?.accent || cssColor('--tile-active-color', '#00e5ff');
}

function clamp01(x) {
    if (!Number.isFinite(x)) return 0;
    return Math.max(0, Math.min(1, x));
}

function hexToRgb(hex) {
    const s = String(hex || '').trim();
    if (!s.startsWith('#')) return null;
    const raw = s.slice(1);
    if (raw.length === 3) {
        const r = parseInt(raw[0] + raw[0], 16);
        const g = parseInt(raw[1] + raw[1], 16);
        const b = parseInt(raw[2] + raw[2], 16);
        return Number.isFinite(r) && Number.isFinite(g) && Number.isFinite(b) ? { r, g, b } : null;
    }
    if (raw.length === 6) {
        const r = parseInt(raw.slice(0, 2), 16);
        const g = parseInt(raw.slice(2, 4), 16);
        const b = parseInt(raw.slice(4, 6), 16);
        return Number.isFinite(r) && Number.isFinite(g) && Number.isFinite(b) ? { r, g, b } : null;
    }
    return null;
}

function rgbMul(rgb, k) {
    const kk = Number.isFinite(k) ? k : 1;
    const clamp = (v) => Math.max(0, Math.min(255, Math.round(v)));
    return {
        r: clamp((rgb?.r ?? 0) * kk),
        g: clamp((rgb?.g ?? 0) * kk),
        b: clamp((rgb?.b ?? 0) * kk),
    };
}

function rgbToCss(rgb) {
    const r = Math.max(0, Math.min(255, Math.round(rgb?.r ?? 0)));
    const g = Math.max(0, Math.min(255, Math.round(rgb?.g ?? 0)));
    const b = Math.max(0, Math.min(255, Math.round(rgb?.b ?? 0)));
    return `rgb(${r},${g},${b})`;
}

function bowColorForGame(gameId) {
    const id = Number(gameId) || 2;
    if (id === 3) return '#2eea6a'; // green (Snake)
    if (id === 1) return '#ff7ad9'; // pink (Racing)
    if (id === 4) return '#cbb7ff'; // purple (Arcanoid)
    return '#7fd4ff'; // blue (Tetris)
}

function menuHoverGameId() {
    const hovered = buttons?.[hoverButtonIdx];
    if (hovered?.kind === 'select') return Number(hovered.gameId);
    return 2;
}

function lerp(a, b, t) {
    return a + (b - a) * t;
}

function easeInOut(t) {
    const x = clamp01(t);
    return x < 0.5 ? 2 * x * x : 1 - Math.pow(-2 * x + 2, 2) / 2;
}

// Menu FX: only for Tetris (no worm/car/rocket on menu).
const MENU_TETRIS_BG_ALPHA = 0.16;

function drawMenuTetrisFx(box) {
    if (!ctx) return;
    drawGameFx(ctx, {
        t: Number.isFinite(anim?.t) ? anim.t : 0,
        gameId: 2,
        box,
        roundRect,
        accent: accentForGame(2),
        alpha: MENU_TETRIS_BG_ALPHA,
        mode: 'menu',
    });
}

function drawPlayingGameFx(gameId) {
    if (!ctx) return;
    const id = Number(gameId) || 2;
    // Keep in-game FX only for Tetris.
    if (id !== 2) return;
    // A small strip near the title area (above UI block).
    const box = {
        x: ui.ox,
        y: Math.max(8, ui.oy - 92),
        w: ui.w,
        h: 70,
    };
    drawGameFx(ctx, {
        t: Number.isFinite(anim?.t) ? anim.t : 0,
        gameId: id,
        box,
        roundRect,
        accent: accentForGame(id),
        alpha: 0.65,
        mode: 'playing',
    });
}

function ensureCanvasSize() {
    if (!$canvas || !ctx) return;
    // Fullscreen canvas (page = canvas). UI is centered inside it.
    const cssW = Math.max(320, Math.round(window.innerWidth || 800));
    const cssH = Math.max(320, Math.round(window.innerHeight || 600));
    canvasCssW = cssW;
    canvasCssH = cssH;
    const dpr = window.devicePixelRatio || 1;
    const targetW = Math.floor(cssW * dpr);
    const targetH = Math.floor(cssH * dpr);

    // Important: assigning to canvas width/height clears the drawing buffer.
    // We call ensureCanvasSize() from multiple render paths, so only resize
    // when the size actually changed.
    if ($canvas.width !== targetW) $canvas.width = targetW;
    if ($canvas.height !== targetH) $canvas.height = targetH;
    $canvas.style.width = `${cssW}px`;
    $canvas.style.height = `${cssH}px`;

    // Always apply transform (does not clear the buffer).
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    // UI "screen" geometry (game area + HUD panel), centered.
    const wCells = rootStyles['--game-board-width'] ?? 10;
    const hCells = rootStyles['--game-board-height'] ?? 20;
    const tile = Math.round(tileSizePx() * UI_TILE_SCALE);
    const gap = 2;
    const hudW = 170;
    ui.w = Math.round(wCells * tile + (wCells + 1) * gap + hudW);
    ui.h = Math.round(hCells * tile + (hCells + 1) * gap);
    ui.ox = Math.round((cssW - ui.w) / 2);
    ui.oy = Math.round((cssH - ui.h) / 2);
}

async function apiGet(path) {
    const res = await fetch(`${API_BASE_URL}${path}`);
    if (!res.ok) throw new Error(`GET ${path} failed: ${res.status}`);
    return res.json();
}

async function apiPost(path, body) {
    const res = await fetch(`${API_BASE_URL}${path}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: body ? JSON.stringify(body) : null,
    });
    const json = await res.json().catch(() => ({}));
    if (!res.ok) {
        const msg =
            (json && (json.message || json.detail)) || `HTTP ${res.status}`;
        throw new Error(`POST ${path} failed: ${msg}`);
    }
    return json;
}

async function apiDelete(path) {
    await fetch(`${API_BASE_URL}${path}`, { method: 'DELETE' }).catch(() => {});
}

function seededRand(seed) {
    let s = seed >>> 0;
    return () => {
        s = (s * 1664525 + 1013904223) >>> 0;
        return s / 4294967296;
    };
}

function drawPixelSkyVariant(theme, opts = {}) {
    if (!ctx || !$canvas) return;
    ensureCanvasSize();
    const W = canvasCssW || $canvas.getBoundingClientRect().width;
    const H = canvasCssH || $canvas.getBoundingClientRect().height;
    const cloudBoost = Number(opts.cloudBoost ?? 1);
    const starBoost = Number(opts.starBoost ?? 1);
    const t = Number.isFinite(anim?.t) ? anim.t : 0;
    const th = theme || BG_THEME[4];
    const r = seededRand(Number.isFinite(th?.seed) ? th.seed : 1337);

    ctx.save();
    // Reset any leaked canvas state that could hide stars/clouds.
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
    ctx.shadowBlur = 0;
    ctx.shadowColor = 'transparent';
    ctx.filter = 'none';
    // Clear in CSS pixels (canvas is scaled by dpr via setTransform).
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = String(th?.baseFill || '#5b56b9');
    ctx.fillRect(0, 0, W, H);

    // Center glow (was previously visible as a soft "middle highlight").
    // Keep it subtle but noticeable on both menu and gameplay screens.
    {
        const gx = W / 2;
        const gy = H * 0.42;
        const r0 = Math.max(80, Math.min(W, H) * 0.10);
        const r1 = Math.max(220, Math.min(W, H) * 0.55);
        const g = ctx.createRadialGradient(gx, gy, r0, gx, gy, r1);
        g.addColorStop(0, 'rgba(203, 183, 255, 0.22)');  // purple glow core
        g.addColorStop(0.35, 'rgba(203, 183, 255, 0.10)');
        g.addColorStop(1, 'rgba(0, 0, 0, 0)');
        ctx.fillStyle = g;
        ctx.fillRect(0, 0, W, H);
    }

    // Twinkling stars (stable positions + time-based alpha).
    const starTpl = String(th?.star || 'rgba(255,255,255,ALPHA)');
    const starCount = Math.max(60, Math.min(260, Number(th?.starCount || 120)));
    for (let i = 0; i < starCount; i++) {
        const x = Math.floor(r() * W);
        const y = Math.floor(r() * H);
        const base = r() < 0.12 ? 3 : (r() < 0.35 ? 2 : 1);
        const size = Math.max(1, Math.round(base * starBoost));
        const phase = r() * Math.PI * 2;
        // Make stars more visible on typical monitors (they were too subtle).
        const a0 = 0.85 + 0.35 * Math.sin(t * 1.2 + phase);
        const a = Math.max(0.2, Math.min(1, a0 * starBoost));
        ctx.fillStyle = starTpl.replace('ALPHA', a.toFixed(3));
        ctx.fillRect(x, y, size, size);
    }

    function cloud(x, y, w, h) {
        // Soft clouds (menu look).
        // Increase opacity a bit to avoid looking like a flat fill.
        const a1 = Math.max(0, Math.min(1, 1.05 * cloudBoost));
        const a2 = Math.max(0, Math.min(1, 0.96 * cloudBoost));
        const a3 = Math.max(0, Math.min(1, 0.92 * cloudBoost));
        const c1 = String(th?.cloud1 || 'rgba(190, 182, 255, ALPHA)');
        const c2 = String(th?.cloud2 || 'rgba(165, 158, 238, ALPHA)');
        const c3 = String(th?.cloud3 || 'rgba(205, 198, 255, ALPHA)');
        ctx.fillStyle = c1.replace('ALPHA', String(a1));
        ctx.fillRect(x, y, w, h);
        ctx.fillStyle = c2.replace('ALPHA', String(a2));
        ctx.fillRect(x + 8, y + 6, w - 12, h - 10);
        ctx.fillStyle = c3.replace('ALPHA', String(a3));
        ctx.fillRect(x + 14, y + 12, Math.max(10, w - 26), Math.max(6, h - 18));
    }

    // More clouds for a richer background.
    const driftSpeed = Number.isFinite(th?.driftSpeed) ? th.driftSpeed : 12;
    // Keep drift deterministic across crossfade overlays.
    const drift = (t * driftSpeed) % (W + 400);
    cloud(18 + drift * 0.12, 46, 120, 34);
    cloud(W - 160 - drift * 0.18, 86, 140, 36);
    cloud(34 + drift * 0.08, H - 120, 150, 42);
    cloud(W - 210 - drift * 0.10, H - 70, 180, 38);
    cloud(60 + drift * 0.06, 120, 180, 44);
    cloud(W - 260 - drift * 0.14, 150, 210, 52);
    cloud(120 + drift * 0.04, H - 190, 220, 56);
    cloud(W - 340 - drift * 0.05, H - 220, 260, 60);
    cloud(40 + drift * 0.09, H - 80, 130, 34);
    cloud(W * 0.35 - drift * 0.09, 64, 170, 40);
    cloud(W * 0.62 + drift * 0.07, 140, 160, 38);

    ctx.restore();
}

function drawPixelSky(opts = {}) {
    // Backward-compatible wrapper: current “main” (Arcanoid) theme.
    drawPixelSkyVariant(BG_THEME[4], opts);
}

function withUi(cb) {
    if (!ctx) return;
    ctx.save();
    ctx.translate(ui.ox, ui.oy);
    cb();
    ctx.restore();
}

function toUiPos(p) {
    return { x: p.x - ui.ox, y: p.y - ui.oy };
}

function withMenuPanel(cb) {
    if (!ctx) return;
    ctx.save();
    ctx.translate(menuPanel.ox, menuPanel.oy);
    cb();
    ctx.restore();
}

function toMenuPos(p) {
    return { x: p.x - menuPanel.ox, y: p.y - menuPanel.oy };
}

function computeMenuPanel() {
    menuPanel.w = canvasCssW || 800;
    menuPanel.h = canvasCssH || 600;
    menuPanel.ox = 0;
    menuPanel.oy = 0;
}

async function refreshLeaderboard(force = false) {
    const t = nowSec();
    if (!force && t - leaderboardFetchedAt < 8) return;
    const limit = screen === 'playing' ? 21 : 5;
    try {
        leaderboardData = await fetchLeaderboard(API_BASE_URL, limit);
        leaderboardFetchedAt = t;
    } catch (_e) {
        if (!leaderboardData.length) leaderboardData = [];
    }
}

function saveNickDraft() {
    const v = String(nickDraft || '').trim();
    if (isValidSchoolNick(v)) setSchoolNick(v);
}

function resolveSchoolNick() {
    const saved = getSchoolNick();
    if (isValidSchoolNick(saved)) return saved;
    const draft = String(nickDraft || '').trim();
    if (isValidSchoolNick(draft)) return draft;
    return '';
}

function trackSessionScore(state) {
    if (!state || screen !== 'playing') return;
    const score = Number(state?.score ?? 0) || 0;
    const high = Number(state?.high_score ?? 0) || 0;
    const level = Number(state?.level ?? 1) || 1;
    sessionBestScore = Math.max(sessionBestScore, score, high);
    sessionBestLevel = Math.max(sessionBestLevel, level);
}

function bestLevelForLeaderboard(state) {
    const level = Number(state?.level ?? 1) || 1;
    return Math.max(level, sessionBestLevel);
}

function bestScoreForLeaderboard(state) {
    const score = Number(state?.score ?? 0) || 0;
    const high = Number(state?.high_score ?? 0) || 0;
    const local =
        Number(selectedGameId) === 4 ? loadLocalHighScore(selectedGameId) : 0;
    return Math.max(score, high, sessionBestScore, local);
}

function isNickInLeaderboardTop(nick, gameId, topN = 5) {
    const block = (leaderboardData || []).find(
        (b) => Number(b?.game_id) === Number(gameId),
    );
    const entries = Array.isArray(block?.entries) ? block.entries : [];
    return entries
        .slice(0, topN)
        .some((e) => String(e?.nickname || '') === nick);
}

function drawNickField(x, y, w, h) {
    if (!ctx) return;
    const accent = '#cbb7ff';
    ctx.save();
    roundRect(x, y, w, h, 12);
    const bg = ctx.createLinearGradient(x, y, x, y + h);
    bg.addColorStop(0, 'rgba(0,0,0,0.42)');
    bg.addColorStop(1, 'rgba(0,0,0,0.24)');
    ctx.fillStyle = bg;
    ctx.fill();
    ctx.strokeStyle = nickFieldFocused ? accent : 'rgba(255,255,255,0.22)';
    ctx.lineWidth = nickFieldFocused ? 2 : 1;
    ctx.stroke();

    ctx.font =
        '11px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillStyle = 'rgba(255,255,255,0.72)';
    ctx.fillText('Введи свой ник', x + 12, y + 8);

    const textY = y + 26;
    const display = String(nickDraft || '');
    ctx.font =
        '15px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.fillStyle = 'rgba(255,255,255,0.95)';
    const shown = display || 'мой_ник';
    ctx.globalAlpha = display ? 1 : 0.45;
    ctx.fillText(shown, x + 12, textY);
    ctx.globalAlpha = 1;

    if (nickFieldFocused) {
        const caretX = x + 12 + ctx.measureText(display).width + 2;
        if (Math.floor(nowSec() * 2) % 2 === 0) {
            ctx.fillStyle = accent;
            ctx.fillRect(caretX, textY + 1, 2, 14);
        }
    }

    if (!isValidSchoolNick(display) && display.length > 0) {
        ctx.font =
            '10px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
        ctx.fillStyle = 'rgba(255,160,160,0.95)';
        ctx.fillText('латиница, цифры, _, 3–20', x + 12, y + h - 14);
    }

    ctx.restore();
    nickFieldBox = { x, y, w, h };
}

function drawAppFooter() {
    if (!ctx) return;
    const school =
        'Проект выполнен в рамках обучения в Школе цифровых технологий 21';
    const nick = 'tablebri';
    const footerY = canvasCssH - 10;

    ctx.save();
    ctx.font =
        '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';

    const heart = ' ♥ ';
    const full = `${nick}${heart}${school}`;
    const fullW = ctx.measureText(full).width;
    const nickW = ctx.measureText(nick).width;
    const heartW = ctx.measureText(heart).width;
    let x = canvasCssW / 2 - fullW / 2;

    ctx.fillStyle = 'rgba(255,255,255,0.92)';
    ctx.fillText(nick, x + nickW / 2, footerY);
    x += nickW;

    ctx.fillStyle = '#ff7ad9';
    ctx.fillText(heart, x + heartW / 2, footerY);
    x += heartW;

    ctx.fillStyle = 'rgba(255,255,255,0.70)';
    ctx.fillText(school, x + ctx.measureText(school).width / 2, footerY);
    ctx.restore();
}

function drawLeaderboardPanel(x, y, w, h, opts = null) {
    if (!ctx) return;
    const onlyGameId =
        opts && typeof opts === 'object' && !Array.isArray(opts)
            ? opts.onlyGameId ?? null
            : opts;
    const topLimit =
        opts && typeof opts === 'object' && !Array.isArray(opts)
            ? Number(opts.topLimit ?? 5)
            : 5;
    const panelTitle =
        opts && typeof opts === 'object' && !Array.isArray(opts)
            ? String(opts.title || `ТОП-${topLimit}`)
            : `ТОП-${topLimit}`;
    const compact = topLimit > 10;
    const lineH = compact
        ? Math.max(10, Math.floor((h - 52) / (topLimit + 1)))
        : 13;
    const rowFont = compact ? 10 : 11;
    const titleFont = compact ? 12 : 14;

    ctx.save();
    roundRect(x, y, w, h, 14);
    ctx.fillStyle = 'rgba(0,0,0,0.22)';
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,0.20)';
    ctx.lineWidth = 1;
    ctx.stroke();

    ctx.font =
        `${titleFont}px ArcadeLocal, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace`;
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    ctx.fillStyle = 'rgba(255,255,255,0.95)';
    ctx.fillText(panelTitle, x + 12, y + 10);

    let sy = y + (compact ? 28 : 34);
    let blocks = Array.isArray(leaderboardData) ? leaderboardData : [];
    if (onlyGameId != null) {
        blocks = blocks.filter(
            (b) => Number(b?.game_id) === Number(onlyGameId),
        );
    }

    if (!blocks.length) {
        ctx.font =
            '11px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
        ctx.fillStyle = 'rgba(255,255,255,0.65)';
        ctx.fillText('Пока пусто —', x + 12, sy);
        ctx.fillText('сыграй и займи', x + 12, sy + lineH);
        ctx.fillText('первое место!', x + 12, sy + lineH * 2);
        ctx.restore();
        return;
    }

    for (const block of blocks) {
        const gid = Number(block?.game_id ?? 0);
        const accent = accentForGame(gid);
        const name = String(block?.name || GAME_THEME[gid]?.name || 'Игра');
        const entries = Array.isArray(block?.entries) ? block.entries : [];

        ctx.font =
            `${rowFont}px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace`;
        ctx.fillStyle = accent;
        ctx.fillText(name, x + 12, sy);
        sy += lineH + (compact ? 1 : 2);

        if (!entries.length) {
            ctx.fillStyle = 'rgba(255,255,255,0.55)';
            ctx.fillText('  —', x + 12, sy);
            sy += lineH + 6;
            continue;
        }

        const top = entries.slice(0, topLimit);
        const nickMax = compact ? 11 : 9;
        for (let i = 0; i < top.length; i++) {
            const e = top[i];
            const nick = String(e?.nickname || '?').slice(0, nickMax);
            const score = Number(e?.score ?? 0);
            const level = Number(e?.level ?? 1) || 1;
            ctx.fillStyle = 'rgba(255,255,255,0.88)';
            ctx.fillText(
                `${i + 1}. ${nick}`,
                x + 12,
                sy,
            );
            ctx.textAlign = 'right';
            ctx.fillStyle = 'rgba(255,255,255,0.72)';
            ctx.fillText(`ур.${level}  ${score}`, x + w - 10, sy);
            ctx.textAlign = 'left';
            sy += lineH;
            if (sy > y + h - 12) break;
        }
        sy += compact ? 4 : 6;
        if (sy > y + h - 16) break;
    }
    ctx.restore();
}

async function maybeSubmitLeaderboardScore(state) {
    if (!state?.game_over || !selectedGameId) return;
    const nick = resolveSchoolNick();
    const score = bestScoreForLeaderboard(state);
    const level = bestLevelForLeaderboard(state);
    if (!nick) {
        leaderboardNotice = 'Введи ник — счёт не отправлен';
        return;
    }
    if (score <= 0) {
        leaderboardNotice = '';
        return;
    }
    const key = `${selectedGameId}:${nick}:${score}`;
    if (key === lastSubmittedScoreKey) return;
    try {
        await submitLeaderboardScore(
            API_BASE_URL,
            nick,
            selectedGameId,
            score,
            level,
        );
        lastSubmittedScoreKey = key;
        await refreshLeaderboard(true);
        leaderboardNotice = isNickInLeaderboardTop(nick, selectedGameId, 21)
            ? `Счёт ${score} (ур.${level}) — ты в топе!`
            : `Счёт ${score} (ур.${level}) записан (в топе 21 лучших)`;
    } catch (_e) {
        leaderboardNotice = 'Не удалось сохранить счёт — проверь, что сервер запущен';
    }
}

function roundRect(x, y, w, h, r) {
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
}

function drawFrame(x, y, w, h, accent = null) {
    if (!ctx) return;
    ctx.save();
    if (accent) {
        ctx.shadowColor = accent;
        ctx.shadowBlur = 10;
    }
    ctx.strokeStyle = 'rgba(255,255,255,0.80)';
    ctx.lineWidth = 2;
    roundRect(x, y, w, h, 14);
    ctx.stroke();
    ctx.restore();
}

function renderCenteredOverlay(state) {
    if (!ctx || !$canvas || !state) return;
    if (!state.pause && !state.game_over) return;

    const W = ui.w;
    const H = ui.h;
    const ox = ui.ox;
    const oy = ui.oy;
    const status = String(state?.status ?? '');
    const won = status.toLowerCase().includes('won');
    const title = state?.pause ? 'ПАУЗА' : (won ? 'ПОБЕДА!' : 'ПРОИГРЫШ');

    // dim only inside UI block
    ctx.save();
    ctx.fillStyle = 'rgba(0,0,0,0.60)';
    roundRect(ox, oy, W, H, 18);
    ctx.fill();

    const boxW = Math.min(W - 36, 460);
    const boxH = state?.game_over ? 132 : 104;
    const boxX = Math.round(ox + (W - boxW) / 2);
    const boxY = Math.round(oy + (H - boxH) / 2);

    ctx.strokeStyle = 'rgba(255,255,255,0.55)';
    ctx.lineWidth = 3;
    roundRect(boxX, boxY, boxW, boxH, 16);
    ctx.stroke();

    ctx.fillStyle = '#ffffff';
    ctx.font =
        '28px ArcadeLocal, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    drawTextShadowed(title, boxX + boxW / 2, boxY + 12);

    if (state?.game_over) {
        const finalScore = bestScoreForLeaderboard(state);
        ctx.font =
            '14px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
        ctx.fillStyle = 'rgba(255,255,255,0.95)';
        drawTextShadowed(
            `Счёт: ${finalScore}   ур.${bestLevelForLeaderboard(state)}`,
            boxX + boxW / 2,
            boxY + 48,
        );
        if (leaderboardNotice) {
            ctx.font =
                '11px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
            ctx.fillStyle = leaderboardNotice.includes('Не удалось')
                ? 'rgba(255,140,140,0.95)'
                : 'rgba(200,255,200,0.92)';
            drawTextShadowed(
                leaderboardNotice,
                boxX + boxW / 2,
                boxY + 68,
            );
        }
    }

    ctx.font =
        '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.fillStyle = 'rgba(255,255,255,0.92)';
    drawTextShadowed(
        'Esc — меню   |   P — пауза   |   R — рестарт',
        boxX + boxW / 2,
        boxY + (state?.game_over ? 98 : 58),
    );
    ctx.restore();
}

function glassPanel(x, y, w, h, r, accent = '#cbb7ff') {
    if (!ctx) return;
    // Outer bezel
    ctx.save();
    roundRect(x, y, w, h, r);
    const g = ctx.createLinearGradient(x, y, x, y + h);
    g.addColorStop(0, 'rgba(0,0,0,0.55)');
    g.addColorStop(1, 'rgba(0,0,0,0.30)');
    ctx.fillStyle = g;
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,0.16)';
    ctx.lineWidth = 1;
    ctx.stroke();

    // Inner glass
    const p = 6;
    roundRect(x + p, y + p, w - p * 2, h - p * 2, Math.max(8, r - 6));
    const g2 = ctx.createLinearGradient(x, y, x + w, y + h);
    g2.addColorStop(0, 'rgba(255,255,255,0.06)');
    g2.addColorStop(0.6, 'rgba(255,255,255,0.02)');
    g2.addColorStop(1, 'rgba(0,0,0,0.10)');
    ctx.fillStyle = g2;
    ctx.fill();

    // Accent line
    ctx.globalAlpha = 0.65;
    ctx.strokeStyle = accent;
    ctx.lineWidth = 2;
    roundRect(x + p + 2, y + p + 2, w - (p + 2) * 2, h - (p + 2) * 2, Math.max(6, r - 10));
    ctx.stroke();
    ctx.restore();
}

function drawButton(b, accent = false, accentColorOverride = null) {
    if (!ctx) return;
    const accentColor =
        accentColorOverride || accentForGame(b?.gameId ?? selectedGameId);
    ctx.save();
    roundRect(b.x, b.y, b.w, b.h, 12);
    const bg = ctx.createLinearGradient(b.x, b.y, b.x, b.y + b.h);
    bg.addColorStop(0, 'rgba(0,0,0,0.40)');
    bg.addColorStop(1, 'rgba(0,0,0,0.22)');
    ctx.fillStyle = bg;
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,0.18)';
    ctx.lineWidth = 1;
    ctx.stroke();

    if (accent) {
        ctx.save();
        ctx.shadowColor = accentColor;
        ctx.shadowBlur = 14;
        ctx.globalAlpha = 0.22;
        roundRect(b.x + 2, b.y + 2, b.w - 4, b.h - 4, 11);
        ctx.fillStyle = accentColor;
        ctx.fill();
        ctx.restore();

        ctx.globalAlpha = 0.9;
        ctx.strokeStyle = accentColor;
        ctx.lineWidth = 2;
        roundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 11);
        ctx.stroke();
        ctx.globalAlpha = 1;
    }
    ctx.fillStyle = '#f5f5f5';
    ctx.font =
        '15px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(b.label, b.x + b.w / 2, b.y + b.h / 2);
    ctx.restore();
}

function drawTextShadowed(text, x, y) {
    if (!ctx) return;
    ctx.save();
    ctx.fillStyle = 'rgba(0,0,0,0.55)';
    ctx.fillText(text, x + 1, y + 1);
    ctx.restore();
    ctx.fillText(text, x, y);
}

function drawArcadeText(text, x, y, opts = {}) {
    if (!ctx) return;
    const size = Number(opts.size ?? 18);
    const align = String(opts.align ?? 'center');
    const baseline = String(opts.baseline ?? 'top');
    const color = String(opts.color ?? '#ffffff');
    const glow = String(opts.glow ?? '#cbb7ff');
    const stroke = String(opts.stroke ?? 'rgba(0,0,0,0.62)');
    const font =
        opts.font
        || `${size}px ArcadeLocal, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace`;

    ctx.save();
    ctx.font = font;
    ctx.textAlign = align;
    ctx.textBaseline = baseline;

    // No outline stroke: only soft glow + subtle shadow.
    ctx.shadowColor = glow;
    ctx.shadowBlur = Math.max(8, Math.round(size * 0.55));
    ctx.fillStyle = color;
    ctx.fillText(text, x + 1, y + 1);
    ctx.fillText(text, x, y);
    ctx.restore();
}

function drawPixelGirl(x, y, scale = 5, opts = {}) {
    if (!ctx) return;
    const bounceY = Number(opts?.bounceY) || 0;
    const drawY = y + bounceY;
    const bow = String(opts?.bowColor || bowColorForGame(selectedGameId));
    const bowRgb = hexToRgb(bow);
    const bowShadow = bowRgb ? rgbToCss(rgbMul(bowRgb, 0.78)) : '#5aaee8';
    const P = {
        // palette
        X: '#0b0b10', // outline (near-black)
        S: '#f3c7a8', // skin
        H: '#c23a3f', // hair (red)
        h: '#a12f33', // hair shadow
        r: '#de8184', // hair highlight
        E: '#56b6ff', // eyes (blue)
        W: '#ffffff', // eye white
        B: bow,
        b: bowShadow,
        U: '#ffffff', // blouse/white
        D: '#2bf8ff', // dress (blue)
        d: '#1f2268', // dress shadow
        K: '#0b0b10', // shoes
        M: '#e88a9a', // smile
        T: null, // transparent
    };

    // Clean sprite near the reference (17x24).
    // 'T' = transparent, 'X' = outline.
    // opts.mood: 'neutral' | 'smile' | 'sad' (default smile in menu)
    const mood = String(opts?.mood || 'smile');
    const W = 17;
    const raw = [
        'TTTXXXTTTTTXXXTTT',
        'TTXBBbXTXTXbBBXTT',
        'TTXBBBbXbXbBBBXTT',
        'TTXBbXXXXXXXbBXTT',
        'TTXbXhHHHHHhXbXTT',
        'TTTXhHHHHHHHhXTTT',
        'TTXhHHHHHHHHHhXTT',
        'TTXrHHHHHHHHHrXTT',
        'TXhrrHHHHHHHrrhXT',
        'TXhHHrrHrhSrHHhXT',
        'TXHHHHHHhSShHHHXT',
        'TXHHHhXSSSXShHHXT',
        'TXhHhSXSSSXSSHhXT',
        'TTXHSSESSSESSHXTT',
        mood === 'sad'
            ? 'TTXHXSSSSSSSXHXTT'
            : 'TTXHXSSMMSMSSXHXTT',
        'TXhHHXXXXXXXHHhXT',
        'TXHHHHXUSUXHHHHXT',
        'XHHHHXUdUdUXHHHHX',
        'XhHhXUUdddUUXhHhX',
        'XhhXSXXDdDXXSXhhX',
        'TXhhXhXDDDXhXhhXT',
        'TXhhhhXSXSXhhhhXT',
        'TTXhhhXdXdXhhhXTT',
        'TTTXXXXXXXXXXXTTT',
    ];
    const sprite = raw.map((row) => (row + 'T'.repeat(W)).slice(0, W));

    const map = {
        X: P.X,
        S: P.S,
        H: P.H,
        h: P.h,
        r: P.r,
        E: P.E,
        W: P.W,
        B: P.B,
        b: P.b,
        U: P.U,
        D: P.D,
        d: P.d,
        K: P.K,
        M: P.M,
        T: null,
    };

    ctx.save();
    ctx.imageSmoothingEnabled = false;
    for (let r = 0; r < sprite.length; r++) {
        const row = sprite[r];
        for (let c = 0; c < row.length; c++) {
            const ch = row[c];
            const color = map[ch] ?? null;
            if (!color) continue;
            ctx.fillStyle = color;
            ctx.fillRect(
                Math.floor(x + c * scale),
                Math.floor(drawY + r * scale),
                scale,
                scale,
            );
        }
    }
    ctx.restore();
    return {
        x,
        y: drawY,
        w: W * scale,
        h: sprite.length * scale,
    };
}

function drawTooltip(x, y, lines) {
    if (!ctx) return;
    const pad = 10;
    ctx.save();
    ctx.font = '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    const maxW = Math.max(...lines.map((t) => ctx.measureText(t).width));
    const w = Math.ceil(maxW + pad * 2);
    const h = Math.ceil(lines.length * 16 + pad * 2);

    const bx = Math.max(10, Math.min(canvasCssW - w - 10, x));
    const by = Math.max(10, Math.min(canvasCssH - h - 10, y));

    ctx.fillStyle = 'rgba(0,0,0,0.55)';
    roundRect(bx, by, w, h, 10);
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,0.20)';
    ctx.stroke();

    ctx.fillStyle = 'rgba(255,255,255,0.92)';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    let ty = by + pad;
    for (const line of lines) {
        ctx.fillText(line, bx + pad, ty);
        ty += 16;
    }
    ctx.restore();
}

function drawSpeechCloud(x, y, lines) {
    if (!ctx) return;
    const pad = 10;
    ctx.save();
    ctx.font =
        '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    const maxW = Math.max(...lines.map((t) => ctx.measureText(t).width));
    const w = Math.ceil(maxW + pad * 2);
    const h = Math.ceil(lines.length * 16 + pad * 2);
    const bx = Math.max(10, Math.min(ui.w - w - 10, x));
    const by = Math.max(10, Math.min(ui.h - h - 10, y));

    // White cloud bubble
    ctx.fillStyle = 'rgba(255,255,255,0.92)';
    roundRect(bx, by, w, h, 14);
    ctx.fill();
    ctx.strokeStyle = 'rgba(0,0,0,0.18)';
    ctx.stroke();

    // tail
    ctx.beginPath();
    ctx.moveTo(bx + 18, by + h);
    ctx.lineTo(bx + 30, by + h + 10);
    ctx.lineTo(bx + 42, by + h);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();

    ctx.fillStyle = 'rgba(20,20,30,0.92)';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'top';
    let ty = by + pad;
    for (const line of lines) {
        ctx.fillText(line, bx + pad, ty);
        ty += 16;
    }
    ctx.restore();
}

function renderMenu() {
    if (!ctx || !$canvas) return;
    ensureCanvasSize();
    const W = ui.w;
    const H = ui.h;
    const sidePad = 20;
    const lbW = 248;
    const btnW = Math.min(280, W - 60);
    const nickH = 52;
    const firstY = 112;

    // Слева — рейтинг.
    const lbH = Math.min(H - firstY - 24, H - 80);
    drawLeaderboardPanel(sidePad, ui.oy + firstY, lbW, lbH, {
        topLimit: 5,
        title: 'ТОП-5',
    });

    // Справа — ник для топа.
    drawNickField(canvasCssW - btnW - sidePad, ui.oy + firstY, btnW, nickH);

    // BRICKGAME — по центру ui-блока, как в оригинале.
    withUi(() => {
        ctx.save();
        drawArcadeText('BRICKGAME', W / 2, 12, {
            size: 60,
            align: 'center',
            baseline: 'top',
            glow: '#cbb7ff',
        });
        ctx.restore();
    });

    const ru = {
        2: GAME_THEME[2].name,
        3: GAME_THEME[3].name,
        1: GAME_THEME[1].name,
        4: GAME_THEME[4].name,
    };
    const byId = new Map();
    for (const g of gamesList || []) {
        byId.set(Number(g.id), String(g.name || g.id));
    }
    const orderedIds = [2, 3, 1, 4];
    const list = orderedIds.map((id) => ({
        id,
        name: ru[id] || byId.get(id) || String(id),
    }));

    withUi(() => {
        const btnH = 42;
        const gap = 10;
        const girlScale = 7;
        const girlW = 17 * girlScale;
        const girlH = 24 * girlScale;
        const girlGap = 26;
        const groupW = btnW + girlGap + girlW;
        const x = Math.max(8, Math.round((W - groupW) / 2));
        const gx = Math.min(W - girlW - 8, x + btnW + girlGap);

        let y = firstY;
        buttons = [];
        for (let i = 0; i < list.length; i++) {
            const g = list[i];
            buttons.push({
                kind: 'select',
                gameId: Number(g.id),
                label: `${g.name}`,
                x,
                y,
                w: btnW,
                h: btnH,
                _i: i,
            });
            y += btnH + gap;
        }

        for (let i = 0; i < buttons.length; i++) {
            const b = buttons[i];
            const isHover = i === hoverButtonIdx;
            drawButton(b, isHover, '#cbb7ff');
            if (isHover) {
                ctx.save();
                ctx.strokeStyle = 'rgba(255,255,255,0.25)';
                ctx.lineWidth = 2;
                roundRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4, 12);
                ctx.stroke();
                ctx.restore();
            }
        }

        const blockH = list.length * btnH + (list.length - 1) * gap;
        const gyRaw = Math.round(firstY + (blockH - girlH) / 2);
        const gy = Math.max(8, Math.min(H - girlH - 8, gyRaw));
        const hoverId = menuHoverGameId();
        girlHitBox = drawPixelGirl(gx, gy, girlScale, {
            bowColor: bowColorForGame(hoverId),
        });

        const cheer = {
            2: ['Вперёд!', 'Собери линию за линией —', 'ты справишься!'],
            3: ['Удачи!', 'Думай на шаг вперёд —', 'и не спеши поворачивать.'],
            1: ['Поехали!', 'Держи траекторию и', 'лови ритм скорости.'],
            4: ['Вперёд!', 'Лови отскоки и', 'разбей все кирпичи!'],
        };
        const hovered = buttons?.[hoverButtonIdx];
        if (hovered?.kind === 'select') {
            const lines = cheer[Number(hovered.gameId)] || ['Удачи!', 'Вперёд!'];
            drawTooltip(girlHitBox.x + girlHitBox.w + 12, girlHitBox.y - 6, lines);
        }

        if (hoverGirl && girlHitBox) {
            drawTooltip(girlHitBox.x + girlHitBox.w + 12, girlHitBox.y - 6, [
                'Привет, меня зовут Алёна!',
                'Проекты выполнены в рамках',
                'обучения в Школе цифровых технологий 21 ♥',
            ]);
        }

        if (menuError) {
            ctx.save();
            ctx.fillStyle = 'rgba(0,0,0,0.55)';
            roundRect(18, H - 70, W - 36, 52, 12);
            ctx.fill();
            ctx.strokeStyle = 'rgba(255,255,255,0.22)';
            ctx.stroke();
            ctx.fillStyle = 'rgba(255,255,255,0.92)';
            ctx.font =
                '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(String(menuError).slice(0, 80), W / 2, H - 44);
            ctx.restore();
        }
    });
}

function renderMatrix(field, colors) {
    if (!ctx || !$canvas) return;
    ensureCanvasSize();
    const W = ui.w;
    const H = ui.h;

    const wCells = rootStyles['--game-board-width'] ?? 10;
    const hCells = rootStyles['--game-board-height'] ?? 20;
    const tile = Math.round(tileSizePx() * UI_TILE_SCALE);
    const gap = 2;
    const pad = gap;
    const hudX = pad + wCells * (tile + gap) + 8;

    withUi(() => {
        // Keep playfield separate from HUD.
        ctx.save();
        ctx.beginPath();
        ctx.rect(0, 0, hudX - 6, H);
        ctx.clip();
        // White rounded frame for playfield area.
        drawFrame(pad - 1, pad - 1, hudX - 6 - (pad - 1) * 2, H - (pad - 1) * 2, null);

        const accent = accentForGame(selectedGameId);
        const tileR = Math.max(3, Math.round(tile * 0.22));
        const tetrisPalette = ['#ff7ad9', '#7dffb1', '#ffd84a']; // pink, light green, butter

        // draw grid cells
        for (let r = 0; r < hCells; r++) {
            for (let c = 0; c < wCells; c++) {
                const on = Boolean(field?.[r]?.[c]);
                if (!on && !colors) continue;

                const v = colors ? (colors?.[r]?.[c] ?? 0) : (on ? 3 : 0);
                if (!v) continue;
                let color = accent;
                if (selectedGameId === 2 && colors) {
                    // Use native integer ids to vary tetris block colors.
                    const vv = Number(v) || 0;
                    const idx = Math.abs(vv - 1) % tetrisPalette.length;
                    color = tetrisPalette[idx];
                } else {
                    // Snake
                    if (v === 1) color = '#7dffb1';
                    if (v === 2) color = '#ffd84a';
                }
                ctx.fillStyle = color;

                const x = pad + c * (tile + gap);
                const y = pad + r * (tile + gap);
                roundRect(x, y, tile, tile, tileR);
                ctx.fill();
            }
        }
        ctx.restore();
    });

    return { hudX, pad, tile, gap, wCells, hCells };
}

function renderArcanoidPrimitives(state) {
    if (!ctx || !$canvas) return null;
    ensureCanvasSize();
    const W = ui.w;
    const H = ui.h;

    const wCells = rootStyles['--game-board-width'] ?? 10;
    const hCells = rootStyles['--game-board-height'] ?? 20;
    const tile = Math.round(tileSizePx() * UI_TILE_SCALE);
    const gap = 2;
    const pad = gap;
    const hudX = pad + wCells * (tile + gap) + 8;

    const accent = accentForGame(4);

    const a = state.arcanoid || {};
    const ball = a.ball || {};
    const paddle = a.paddle || {};
    const bricks = Array.isArray(a.bricks) ? a.bricks : [];

    withUi(() => {
        // No playfield background overlay: keep the same sky as menu.

        // White rounded frame for playfield area.
        drawFrame(pad - 1, pad - 1, hudX - 6 - (pad - 1) * 2, H - (pad - 1) * 2, null);

        // bricks
        const tileR = Math.max(3, Math.round(tile * 0.22));
        ctx.fillStyle = 'rgba(255,255,255,0.18)';
        for (const br of bricks) {
            const x = pad + (Number(br.x) || 0) * (tile + gap);
            const y = pad + (Number(br.y) || 0) * (tile + gap);
            roundRect(x, y, tile, tile, tileR);
            ctx.fill();
        }
        ctx.fillStyle = accent;
        for (const br of bricks) {
            const x = pad + (Number(br.x) || 0) * (tile + gap) + 2;
            const y = pad + (Number(br.y) || 0) * (tile + gap) + 2;
            roundRect(x, y, tile - 4, tile - 4, Math.max(2, tileR - 2));
            ctx.fill();
        }

        // paddle
        const px = pad + (Number(paddle.x) || 0) * (tile + gap);
        const py = pad + (Number(paddle.y) || 0) * (tile + gap);
        const pw = (Number(paddle.w) || 3) * (tile + gap) - gap;
        const ph = tile;
        ctx.fillStyle = 'rgba(255,255,255,0.25)';
        roundRect(px, py, pw, ph, 6);
        ctx.fill();
        ctx.fillStyle = accent;
        roundRect(px + 2, py + 2, pw - 4, ph - 4, 6);
        ctx.fill();

        // ball
        const bx = pad + ((Number(ball.x) || 0) * (tile + gap));
        const by = pad + ((Number(ball.y) || 0) * (tile + gap));
        const br = Math.max(3, (Number(ball.r) || 0.35) * tile);
        ctx.beginPath();
        ctx.fillStyle = '#ffffff';
        ctx.arc(bx, by, br, 0, Math.PI * 2);
        ctx.fill();
    });

    return { hudX, pad, tile, gap, wCells, hCells };
}

function renderHud(state, geom) {
    if (!ctx || !$canvas || !geom) return;
    const W = ui.w;
    const H = ui.h;
    const { hudX, pad } = geom;
    const panelW = Math.max(140, W - hudX - 10);

    withUi(() => {
        ctx.save();
        ctx.textAlign = 'left';
        ctx.textBaseline = 'top';

        const accent = accentForGame(selectedGameId);
        const panelX = hudX;
        // Match the playfield vertical span.
        const panelY = pad;
        const panelH = H - pad * 2;
        const innerPad = 10;

        // No separate HUD container frame: keep HUD elements directly on sky.

        const score = Number(state?.score ?? 0);
        const high = Number(state?.high_score ?? 0);
        const level = Number(state?.level ?? 1);
        const speed = Number(state?.speed ?? 1);

        const sx = panelX + innerPad;
        let sy = panelY + 12;

        const cardW = panelW - innerPad * 2;
        const cardH = 30;
        const cardR = 12;
        const labelX = sx + 10;
        const valueX = sx + cardW - 10;

        const drawCard = (label, value, valueColor = 'rgba(255,255,255,0.95)') => {
            ctx.save();
            ctx.fillStyle = 'rgba(0,0,0,0.18)';
            roundRect(sx, sy, cardW, cardH, cardR);
            ctx.fill();
            ctx.strokeStyle = 'rgba(255,255,255,0.22)';
            ctx.lineWidth = 1;
            ctx.stroke();

            ctx.font =
                '15px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'middle';
            ctx.fillStyle = 'rgba(255,255,255,0.78)';
            ctx.fillText(label, labelX, sy + cardH / 2);

            ctx.textAlign = 'right';
            ctx.fillStyle = valueColor;
            ctx.fillText(String(value), valueX, sy + cardH / 2);
            ctx.restore();
            sy += cardH + 10;
        };

        const drawLivesCard = (lives) => {
            const n = Math.max(0, Math.min(9, Number(lives) || 0));
            const h = 76;
            ctx.save();
            ctx.fillStyle = 'rgba(0,0,0,0.10)';
            roundRect(sx, sy, cardW, h, cardR);
            ctx.fill();
            ctx.strokeStyle = 'rgba(255,255,255,0.14)';
            ctx.lineWidth = 1;
            ctx.stroke();

            // label
            ctx.font =
                '14px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'top';
            ctx.fillStyle = 'rgba(255,255,255,0.78)';
            ctx.fillText('Жизни', labelX, sy + 10);

            // Pixel hearts (bigger, under the label) with wider spacing.
            const heart = [
                '..11..11..',
                '.11111111.',
                '.11111111.',
                '..111111..',
                '...1111...',
                '....11....',
            ];
            const scale = 3; // pixel size
            const pxW = heart[0].length * scale;
            const pxH = heart.length * scale;
            const gap = 10; // space between hearts (CSS px)
            const count = Math.max(0, Math.min(3, n));
            const totalW = count > 0 ? count * pxW + (count - 1) * gap : 0;
            const baseY = Math.round(sy + 34);
            let baseX = Math.round(sx + (cardW - totalW) / 2);
            if (count === 0) {
                ctx.fillStyle = 'rgba(255,255,255,0.55)';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.font =
                    '18px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
                ctx.fillText('—', sx + cardW / 2, sy + 48);
            } else {
                ctx.save();
                ctx.imageSmoothingEnabled = false;
                for (let i = 0; i < count; i++) {
                    for (let r = 0; r < heart.length; r++) {
                        const row = heart[r];
                        for (let c = 0; c < row.length; c++) {
                            if (row[c] !== '1') continue;
                            ctx.fillStyle = '#ff7ad9';
                            ctx.fillRect(
                                baseX + i * (pxW + gap) + c * scale,
                                baseY + r * scale,
                                scale,
                                scale,
                            );
                        }
                    }
                }
                ctx.restore();
            }
            ctx.restore();
            sy += h + 10;
        };

        drawCard('Счёт', score);
        drawCard('Рекорд', high);
        drawCard('Уровень', level);
        drawCard('Скорость', speed);

        // tetris next 4x4
        if (selectedGameId === 2 && Array.isArray(state?.next)) {
            // Next block in its own card
            const nextCardH = 104;
            ctx.save();
            ctx.fillStyle = 'rgba(0,0,0,0.24)';
            roundRect(sx, sy, cardW, nextCardH, cardR);
            ctx.fill();
            ctx.strokeStyle = 'rgba(255,255,255,0.14)';
            ctx.stroke();
            ctx.restore();

            ctx.save();
            ctx.font =
                '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
            ctx.fillStyle = 'rgba(255,255,255,0.80)';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'top';
            ctx.fillText('Следующая', sx + 10, sy + 8);
            ctx.restore();

            // Centered + bigger preview (must not overlap the title).
            const gap = 3;
            // Keep a reserved header band for the label.
            const innerTop = sy + 30;
            const innerH = nextCardH - 36;
            const maxTileByW = Math.floor((cardW - 24 - 3 * gap) / 4);
            const maxTileByH = Math.floor((innerH - 3 * gap) / 4);
            const tile = Math.max(12, Math.min(20, maxTileByW, maxTileByH));
            const gridW = 4 * tile + 3 * gap;
            const gridH = 4 * tile + 3 * gap;
            const baseX = Math.round(sx + (cardW - gridW) / 2);
            const baseY = Math.round(innerTop + Math.max(0, (innerH - gridH) / 2));
            const tetrisPalette = ['#ff7ad9', '#7dffb1', '#ffd84a'];
            const nextColors = Array.isArray(state?.next_colors) ? state.next_colors : null;
            for (let r = 0; r < 4; r++) {
                for (let c = 0; c < 4; c++) {
                    const on = Boolean(state?.next?.[r]?.[c]);
                    if (!on) continue;
                    if (nextColors) {
                        const v = Number(nextColors?.[r]?.[c] ?? 0);
                        const idx = Math.abs(v - 1) % tetrisPalette.length;
                        ctx.fillStyle = tetrisPalette[idx];
                    } else {
                        ctx.fillStyle = accentForGame(2);
                    }
                    roundRect(
                        baseX + c * (tile + gap),
                        baseY + r * (tile + gap),
                        tile,
                        tile,
                        3,
                    );
                    ctx.fill();
                }
            }
            sy += nextCardH + 10;
        }

        // Race lives
        if (selectedGameId === 1 && state?.lives != null) {
            drawLivesCard(state.lives);
        }

        // no HUD buttons (keyboard only)
        buttons = [];

        // pause/game over overlay is rendered in renderPlaying()

        // In-game mascot (place inside HUD, not on playfield)
        const girlScale = 6;
        const girlW = 17 * girlScale;
        const girlH = 24 * girlScale;
        const gx = Math.round(panelX + panelW - girlW - 12);
        const gy = Math.round(panelY + panelH - girlH - 12);
        drawPixelGirl(gx, gy, girlScale, {
            bowColor: bowColorForGame(selectedGameId),
        });

        ctx.restore();
    });
}

function applyThemeForGame(gameId) {
    const root = document.documentElement;
    if (!root) return;
    const t = GAME_THEME[Number(gameId)] || GAME_THEME[2];
    root.style.setProperty('--tile-active-color', t.accent);
}

async function selectGame(gameId) {
    menuError = '';
    saveNickDraft();
    if (!isValidSchoolNick(getSchoolNick())) {
        menuError = 'Введи ник (латиница, 3–20)';
        nickFieldFocused = true;
        renderMenu();
        return;
    }
    try {
        const gid = Number(gameId);
        sessionBestScore = 0;
        sessionBestLevel = 1;
        leaderboardNotice = '';
        const onGameReady = async () => {
            saveNickDraft();
            applyThemeForGame(gid);
            await refreshLeaderboard(true);
            render();
        };
        // Prefer WASM for legacy C/C++ games.
        if (wasmEngine.supportsGame(gid)) {
            try {
                await wasmEngine.selectGame(gid);
                activeEngine = 'wasm';
                selectedGameId = gid;
                currentState = null;
                screen = 'playing';
                await onGameReady();
                return;
            } catch (e) {
                // WASM artifacts may be missing; fall back.
                menuError = 'Не удалось запустить WASM. Запускаю через REST.';
            }
        }

        // Prefer Pyodide for pure-python games (runs in browser via WASM runtime).
        if (pyodideEngine.supportsGame(gid)) {
            try {
                await pyodideEngine.selectGame(gid);
                activeEngine = 'pyodide';
                selectedGameId = gid;
                currentState = null;
                screen = 'playing';
                await onGameReady();
                return;
            } catch (e) {
                menuError = 'Не удалось запустить Pyodide. Запускаю через REST.';
            }
        }

        await apiPost(`/games/${gid}`);
        activeEngine = 'rest';
        selectedGameId = gid;
        currentState = null;
        screen = 'playing';
        await onGameReady();
    } catch (e) {
        selectedGameId = null;
        currentState = null;
        screen = 'menu';
        menuError = String(e?.message || e || 'Не удалось запустить игру');
        throw e;
    }
}

async function restartGame() {
    if (!selectedGameId) return;
    sessionBestScore = 0;
    sessionBestLevel = 1;
    leaderboardNotice = '';
    if (activeEngine === 'wasm' && wasmEngine.supportsGame(selectedGameId)) {
        await wasmEngine.restartGame();
        return;
    }
    if (activeEngine === 'pyodide' && pyodideEngine.supportsGame(selectedGameId)) {
        await pyodideEngine.restartGame();
        return;
    }
    await apiPost(`/games/${selectedGameId}`);
}

async function togglePause() {
    if (!selectedGameId) return;
    if (activeEngine === 'wasm' && wasmEngine.supportsGame(selectedGameId)) {
        await wasmEngine.sendAction(ActionId.Pause, false);
    } else if (
        activeEngine === 'pyodide'
        && pyodideEngine.supportsGame(selectedGameId)
    ) {
        await pyodideEngine.sendAction(ActionId.Pause, false);
    } else {
        await apiPost('/actions', { action_id: ActionId.Pause, hold: false });
    }
    await fetchState().catch(() => {});
}

async function sendAction(actionId, hold = false) {
    if (!selectedGameId) return;
    if (activeEngine === 'wasm' && wasmEngine.supportsGame(selectedGameId)) {
        await wasmEngine.sendAction(actionId, Boolean(hold));
        return;
    }
    if (activeEngine === 'pyodide' && pyodideEngine.supportsGame(selectedGameId)) {
        await pyodideEngine.sendAction(actionId, Boolean(hold));
        return;
    }
    await apiPost('/actions', { action_id: actionId, hold: Boolean(hold) });
}

async function fetchState() {
    if (!selectedGameId) return;
    if (activeEngine === 'wasm' && wasmEngine.supportsGame(selectedGameId)) {
        currentState = await wasmEngine.fetchState();
        currentState = syncHighScoreForWeb(currentState);
        trackSessionScore(currentState);
        await maybeSubmitLeaderboardScore(currentState);
        return;
    }
    if (activeEngine === 'pyodide' && pyodideEngine.supportsGame(selectedGameId)) {
        currentState = await pyodideEngine.fetchState();
        currentState = syncHighScoreForWeb(currentState);
        trackSessionScore(currentState);
        await maybeSubmitLeaderboardScore(currentState);
        return;
    }
    currentState = await apiGet('/state');
    currentState = syncHighScoreForWeb(currentState);
    trackSessionScore(currentState);
    await maybeSubmitLeaderboardScore(currentState);
}

async function showMenu() {
    selectedGameId = null;
    currentState = null;
    screen = 'menu';
    buttons = [];
    nickFieldFocused = false;
    nickDraft = getSchoolNick();
    sessionBestScore = 0;
    sessionBestLevel = 1;
    leaderboardNotice = '';
    if (activeEngine === 'rest') {
        await apiDelete('/games');
    }
    await refreshLeaderboard(true);
    renderMenu();
}

function hitTest(x, y) {
    for (const b of buttons) {
        if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) return b;
    }
    return null;
}

function canvasLocalPos(e) {
    if (!$canvas) return null;
    const rect = $canvas.getBoundingClientRect();
    return { x: e.clientX - rect.left, y: e.clientY - rect.top };
}

function stopSoftDrop() {
    if (!softDropTimer) return;
    clearInterval(softDropTimer);
    softDropTimer = null;
}

function startSoftDrop() {
    if (softDropTimer) return;
    sendAction(ActionId.Down, false).catch(() => {});
    softDropTimer = setInterval(() => {
        if (selectedGameId !== 2) return;
        if (currentState?.game_over) return;
        sendAction(ActionId.Down, false).catch(() => {});
    }, 60);
}

function playingLeaderboardRect() {
    const gap = 16;
    let x = ui.ox + ui.w + gap;
    if (x + ui.w > canvasCssW - 8) {
        x = Math.max(8, ui.ox - ui.w - gap);
    }
    return { x, y: ui.oy, w: ui.w, h: ui.h };
}

function renderPlaying() {
    if (!ctx || !$canvas) return;
    // Always paint sky here too.
    // Some call paths render the playing screen without going through render(),
    // which caused the in-game background to fall back to the plain body color.
    ensureCanvasSize();
    drawPixelSky({ cloudBoost: 1.75, starBoost: 1.75 });

    const accent = accentForGame(selectedGameId);
    if (selectedGameId) {
        const lb = playingLeaderboardRect();
        drawLeaderboardPanel(lb.x, lb.y, lb.w, lb.h, {
            onlyGameId: selectedGameId,
            topLimit: 21,
            title: 'ТОП-21',
        });
        ctx.save();
        ctx.shadowColor = accent;
        ctx.shadowBlur = 14;
        ctx.lineWidth = 3;
        ctx.strokeStyle = 'rgba(255,255,255,0.80)';
        roundRect(lb.x - 8, lb.y - 8, lb.w + 16, lb.h + 16, 18);
        ctx.stroke();
        ctx.shadowBlur = 0;
        ctx.lineWidth = 2;
        ctx.strokeStyle = accent;
        roundRect(lb.x - 4, lb.y - 4, lb.w + 8, lb.h + 8, 16);
        ctx.stroke();
        ctx.restore();
    }

    // Bright arcade frame around the whole UI block (like menu vibe).
    ctx.save();
    ctx.shadowColor = accent;
    ctx.shadowBlur = 18;
    ctx.lineWidth = 4;
    ctx.strokeStyle = 'rgba(255,255,255,0.85)';
    roundRect(ui.ox - 12, ui.oy - 12, ui.w + 24, ui.h + 24, 20);
    ctx.stroke();
    ctx.shadowBlur = 0;
    ctx.lineWidth = 2;
    ctx.strokeStyle = accent;
    roundRect(ui.ox - 8, ui.oy - 8, ui.w + 16, ui.h + 16, 18);
    ctx.stroke();
    ctx.restore();

    // In-game themed FX (near the title strip).
    drawPlayingGameFx(selectedGameId);

    if (selectedGameId === 4 && currentState?.arcanoid) {
        const geom = renderArcanoidPrimitives(currentState);
        renderHud(currentState, geom);
        renderCenteredOverlay(currentState);
        const hintY = ui.oy + ui.h + 22;
        const y = hintY + 16 <= canvasCssH ? hintY : Math.max(8, canvasCssH - 22);
        // no frame around bottom hint
        ctx.save();
        ctx.fillStyle = 'rgba(255,255,255,0.92)';
        ctx.font =
            '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'top';
        drawTextShadowed('Esc: меню  |  P: пауза  |  R: рестарт', ui.ox + ui.w / 2, y);
        ctx.restore();
        // Draw game title last (on top of everything).
        const id = Number(selectedGameId ?? currentState?.game_id ?? 0);
        const title =
            GAME_THEME?.[id]?.name
            || gamesList?.find((g) => Number(g?.id) === id)?.name
            || `game_id=${id}`;
        ctx.save();
        ctx.fillStyle = 'rgba(255,255,255,0.95)';
        ctx.font =
            '40px ArcadeLocal, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'top';
        drawTextShadowed(title, ui.ox + ui.w / 2, Math.max(8, ui.oy - 76));
        ctx.restore();

        return;
    }
    const field = currentState?.field ?? [];
    const colors = Array.isArray(currentState?.field_colors)
        ? currentState.field_colors
        : null;
    const geom = renderMatrix(field, colors);
    renderHud(currentState, geom);
    renderCenteredOverlay(currentState);
    const hintY = ui.oy + ui.h + 22;
    const y = hintY + 16 <= canvasCssH ? hintY : Math.max(8, canvasCssH - 22);
    // no frame around bottom hint
    ctx.save();
    ctx.fillStyle = 'rgba(255,255,255,0.92)';
    ctx.font =
        '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    drawTextShadowed('Esc: меню  |  P: пауза  |  R: рестарт', ui.ox + ui.w / 2, y);
    ctx.restore();

    // Draw game title last (on top of everything).
    const id = Number(selectedGameId ?? currentState?.game_id ?? 0);
    const title =
        GAME_THEME?.[id]?.name
        || gamesList?.find((g) => Number(g?.id) === id)?.name
        || `game_id=${id}`;
    ctx.save();
    ctx.fillStyle = 'rgba(255,255,255,0.95)';
    ctx.font =
        '40px ArcadeLocal, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    drawTextShadowed(title, ui.ox + ui.w / 2, Math.max(8, ui.oy - 76));
    ctx.restore();

}

function render() {
    if (!ctx || !$canvas) return;
    ensureCanvasSize();
    // Same background in menu and in-game (sky + stars + clouds).
    // Boost a bit so clouds are clearly visible on most monitors.
    drawPixelSky({ cloudBoost: 1.75, starBoost: 1.75 });
    if (screen === 'menu') {
        // Menu FX: keep only Tetris (as before).
        renderMenu();
        if (hoverButtonIdx >= 0 && Number(menuHoverGameId()) === 2) {
            drawMenuTetrisFx({ x: 0, y: 0, w: canvasCssW, h: canvasCssH });
        }
    } else {
        renderPlaying();
    }

    drawAppFooter();

}

function tick() {
    anim.t = (performance.now() - anim.t0) / 1000;
    render();
    requestAnimationFrame(tick);
}

async function init() {
    ensureCanvasSize();
    window.addEventListener('resize', () => render());

    // Warm up the custom font so Canvas picks it up immediately.
    try {
        if (document?.fonts?.load) {
            await document.fonts.load('40px "ArcadeLocal"');
        }
    } catch (e) {
        // ignore (fallback fonts will be used)
    }

    try {
        const data = await apiGet('/games');
        const games = Array.isArray(data?.games) ? data.games : [];
        gamesList = games.map((g) => ({ id: Number(g.id), name: String(g.name) }));
    } catch (e) {
        gamesList = [];
    }

    await refreshLeaderboard(true);
    renderMenu();
    requestAnimationFrame(tick);

    if ($canvas) {
        $canvas.addEventListener('mousemove', (e) => {
            const p = canvasLocalPos(e);
            if (!p) return;

            if (screen === 'menu') {
                const u = toUiPos(p);
                hoverButtonIdx = -1;
                for (let i = 0; i < buttons.length; i++) {
                    const b = buttons[i];
                    if (
                        u.x >= b.x && u.x <= b.x + b.w
                        && u.y >= b.y && u.y <= b.y + b.h
                    ) {
                        hoverButtonIdx = i;
                        break;
                    }
                }
                let onGirl = false;
                if (girlHitBox) {
                    onGirl =
                        u.x >= girlHitBox.x
                        && u.x <= girlHitBox.x + girlHitBox.w
                        && u.y >= girlHitBox.y
                        && u.y <= girlHitBox.y + girlHitBox.h;
                }
                if (onGirl !== hoverGirl) hoverGirl = onGirl;
                render();
            }
        });

        $canvas.addEventListener('mouseleave', () => {
            hoverButtonIdx = -1;
            hoverGirl = false;
            render();
        });

        $canvas.addEventListener('click', async (e) => {
            const p = canvasLocalPos(e);
            if (!p) return;

            if (screen === 'menu') {
                if (nickFieldBox) {
                    const n = nickFieldBox;
                    const inNick =
                        p.x >= n.x && p.x <= n.x + n.w
                        && p.y >= n.y && p.y <= n.y + n.h;
                    nickFieldFocused = inNick;
                    if (!inNick) saveNickDraft();
                    render();
                    if (inNick) return;
                }
            }

            const u = toUiPos(p);
            const b = hitTest(u.x, u.y);
            if (!b) return;
            try {
                if (screen === 'menu' && b.kind === 'select') {
                    await selectGame(b.gameId);
                    await fetchState();
                    render();
                } else if (screen === 'playing') {
                    if (b.kind === 'menu') await showMenu();
                    if (b.kind === 'restart') await restartGame();
                    if (b.kind === 'pause') await togglePause();
                    await fetchState();
                    render();
                }
            } catch (err) {
                console.error(err);
            }
        });
    }

    document.addEventListener('keydown', (event) => {
        if (screen === 'menu' && nickFieldFocused) {
            if (event.code === 'Enter') {
                saveNickDraft();
                nickFieldFocused = false;
                render();
                event.preventDefault();
                return;
            }
            if (event.code === 'Escape') {
                nickFieldFocused = false;
                render();
                event.preventDefault();
                return;
            }
            if (event.code === 'Backspace') {
                nickDraft = String(nickDraft || '').slice(0, -1);
                render();
                event.preventDefault();
                return;
            }
            if (event.key && event.key.length === 1) {
                const ch = event.key;
                if (/[a-zA-Z0-9_]/.test(ch) && String(nickDraft || '').length < 20) {
                    nickDraft = `${nickDraft || ''}${ch}`;
                    render();
                }
                event.preventDefault();
                return;
            }
        }

        if (screen === 'menu') {
            if (event.code === 'Escape') return;
            return;
        }
        if (event.code === 'Escape') {
            showMenu().catch(() => {});
            return;
        }
        if (event.code === 'KeyR') {
            restartGame().then(fetchState).then(render).catch(() => {});
            return;
        }
        if (event.code === 'KeyP') {
            togglePause().then(fetchState).then(render).catch(() => {});
            return;
        }
        if (selectedGameId === 2 && event.code === 'Space') {
            sendAction(ActionId.Up, false).then(fetchState).then(render).catch(() => {});
            return;
        }
        if (selectedGameId === 4 && event.code === 'Space') {
            sendAction(ActionId.Action, false).then(fetchState).then(render).catch(() => {});
            return;
        }
        if (selectedGameId === 3 && event.code === 'Space') {
            sendAction(ActionId.Action, true).then(fetchState).then(render).catch(() => {});
            return;
        }

        if (keyCodes.up.includes(event.code)) {
            if (selectedGameId === 1) {
                sendAction(ActionId.Up, true).then(fetchState).then(render).catch(() => {});
            } else if (selectedGameId === 2) {
                sendAction(ActionId.Up, false).then(fetchState).then(render).catch(() => {});
            } else {
                sendAction(ActionId.Up, false).then(fetchState).then(render).catch(() => {});
            }
        }
        if (keyCodes.right.includes(event.code)) {
            sendAction(ActionId.Right, false).then(fetchState).then(render).catch(() => {});
        }
        if (keyCodes.left.includes(event.code)) {
            sendAction(ActionId.Left, false).then(fetchState).then(render).catch(() => {});
        }
        if (keyCodes.down.includes(event.code)) {
            if (selectedGameId === 2) {
                if (event.repeat) return;
                startSoftDrop();
            } else {
                sendAction(ActionId.Down, false).then(fetchState).then(render).catch(() => {});
            }
        }
    });

    document.addEventListener('keyup', (event) => {
        if (selectedGameId === 1 && keyCodes.up.includes(event.code)) {
            sendAction(ActionId.Up, false).catch(() => {});
        }
        if (selectedGameId === 2 && keyCodes.down.includes(event.code)) {
            stopSoftDrop();
        }
        if (selectedGameId === 3 && event.code === 'Space') {
            sendAction(ActionId.Action, false).catch(() => {});
        }
    });

    setInterval(async () => {
        if (screen !== 'playing') return;
        if (!selectedGameId) return;
        try {
            await fetchState();
            render();
        } catch (e) {
            // ignore polling errors
        }
    }, 200);
}

init().catch(console.error);
