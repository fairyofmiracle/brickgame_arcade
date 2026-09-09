import { applyRootStyles } from './src/utils.js';
import { GameBoard } from './src/game-board.js';
import { rootStyles, keyCodes } from './src/config.js';

applyRootStyles(rootStyles);
const gameBoard = new GameBoard(document.querySelector('#game-board'));
const $gameBoard = document.querySelector('#game-board');
const $gameCanvas = document.querySelector('#game-canvas');
const canvasCtx = $gameCanvas ? $gameCanvas.getContext('2d') : null;

const $sidePanel = document.querySelector('#side-panel');
const $panelTetris = document.querySelector('#panel-tetris');
const $panelSnake = document.querySelector('#panel-snake');
const $panelRace = document.querySelector('#panel-race');
const $panelArcanoid = document.querySelector('#panel-arcanoid');

const $scoreTetris = document.querySelector('#score');
const $highScoreTetris = document.querySelector('#high-score');
const $levelTetris = document.querySelector('#level');
const $speedTetris = document.querySelector('#speed');

const $scoreSnake = document.querySelector('#score-snake');
const $highScoreSnake = document.querySelector('#high-score-snake');

const $scoreRace = document.querySelector('#score-race');
const $highScoreRace = document.querySelector('#high-score-race');
const $levelRace = document.querySelector('#level-race');
const $nitroIndicator = document.querySelector('#nitro-indicator');
const $healthBar = document.querySelector('#health-bar');
const $scoreArcanoid = document.querySelector('#score-arcanoid');
const $highScoreArcanoid = document.querySelector('#high-score-arcanoid');
const $levelArcanoid = document.querySelector('#level-arcanoid');
const $speedArcanoid = document.querySelector('#speed-arcanoid');

const RACE_MAX_LIVES = 3;

// Важно для запуска по IP: при обращении к http://<ip>:8005
// фронт должен стучаться в тот же origin, а не в localhost.
const API_BASE_URL =
    (window.location && window.location.origin && window.location.origin !== 'null')
        ? window.location.origin
        : 'http://127.0.0.1:8005';

const $menuView = document.querySelector('#menu-view');
const $gameSelection = document.querySelector('#game-selection');
const $gameContainer = document.querySelector('#game-container');
const $btnPause = document.querySelector('#btn-pause');
const $btnRestart = document.querySelector('#btn-restart');
const $btnMenu = document.querySelector('#btn-menu');
const $gameOverModal = document.querySelector('#game-over-modal');
const $modalScore = document.querySelector('#modal-score');
const $modalRestart = document.querySelector('#modal-restart');
const $modalMenu = document.querySelector('#modal-menu');
const $pauseModal = document.querySelector('#pause-modal');
const $pauseContinue = document.querySelector('#pause-continue');
const $pauseMenu = document.querySelector('#pause-menu');
const $nextPanel = document.querySelector('#next-panel');
const $nextBoard = document.querySelector('#next-board');
const $consoleBtnPause = document.querySelector('#console-buttons .console-btn-group:nth-child(1) .console-btn-small');
const $consoleBtnRestart = document.querySelector('#console-buttons .console-btn-group:nth-child(2) .console-btn-small');
const $consoleBtnMenu = document.querySelector('#console-buttons .console-btn-group:nth-child(3) .console-btn-small');
let selectedGameId = null;
let canvasUiMode = false;
let canvasScreen = 'menu'; // 'menu' | 'playing'
let canvasButtons = [];
let gamesList = [];

async function apiGet(path) {
    const res = await fetch(`${API_BASE_URL}${path}`);
    if (!res.ok) {
        const text = await res.text();
        throw new Error(`GET ${path} failed: ${res.status} ${text}`);
    }
    return res.json();
}

async function apiPost(path, body) {
    const res = await fetch(`${API_BASE_URL}${path}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: body ? JSON.stringify(body) : null,
    });
    if (!res.ok) {
        const text = await res.text();
        throw new Error(`POST ${path} failed: ${res.status} ${text}`);
    }
    return res.json().catch(() => ({}));
}

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

let currentState = null;
let gameSession = 0;

function clearBoard() {
    // сбрасываем визуал, чтобы при переключении игр не “мигало” прошлое состояние
    const w = rootStyles['--game-board-width'] ?? 10;
    const h = rootStyles['--game-board-height'] ?? 20;
    for (let row = 0; row < h; row++) {
        for (let col = 0; col < w; col++) {
            gameBoard.disableTile(col, row);
        }
    }
    if (canvasCtx && $gameCanvas) {
        canvasCtx.clearRect(0, 0, $gameCanvas.width, $gameCanvas.height);
    }
}

function _tileSizePx() {
    const root = getComputedStyle(document.documentElement);
    const raw = root.getPropertyValue('--tile-size') || '20px';
    const n = Number(String(raw).trim().replace('px', ''));
    return Number.isFinite(n) && n > 0 ? n : 20;
}

function _cssVarColor(name, fallback) {
    const root = getComputedStyle(document.documentElement);
    const v = String(root.getPropertyValue(name) || '').trim();
    return v || fallback;
}

function _ensureCanvasSize() {
    if (!$gameCanvas) return;
    const wCells = rootStyles['--game-board-width'] ?? 10;
    const hCells = rootStyles['--game-board-height'] ?? 20;
    const tile = _tileSizePx();
    const gap = 2; // соответствует --game-board-gap по умолчанию
    const cssW = wCells * tile + (wCells - 1) * gap + gap * 2;
    const cssH = hCells * tile + (hCells - 1) * gap + gap * 2;
    const dpr = window.devicePixelRatio || 1;
    $gameCanvas.style.width = `${cssW}px`;
    $gameCanvas.style.height = `${cssH}px`;
    $gameCanvas.width = Math.floor(cssW * dpr);
    $gameCanvas.height = Math.floor(cssH * dpr);
    if (canvasCtx) {
        canvasCtx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }
}

function setBoardRenderMode(useCanvas) {
    if ($gameCanvas) $gameCanvas.classList.toggle('hidden', !useCanvas);
    if ($gameBoard) $gameBoard.classList.toggle('hidden', useCanvas);
    if (useCanvas) _ensureCanvasSize();
}

function setCanvasUiMode(on) {
    canvasUiMode = Boolean(on);
    if (!canvasUiMode) {
        if ($sidePanel) $sidePanel.classList.remove('hidden');
        hideGameOverModal();
        hidePauseModal();
        return;
    }

    // Canvas UI owns menu/pause/gameover/hud — hide HTML overlays and side panel.
    if ($sidePanel) $sidePanel.classList.add('hidden');
    if ($gameOverModal) $gameOverModal.classList.add('hidden');
    if ($pauseModal) $pauseModal.classList.add('hidden');

    // Ensure we render in the game container where canvas lives.
    if ($gameContainer) $gameContainer.style.display = 'block';
    if ($menuView) $menuView.style.display = 'none';
    setBoardRenderMode(true);
}

async function loadGamesList() {
    try {
        const data = await apiGet('/games');
        const games = Array.isArray(data?.games) ? data.games : [];
        gamesList = games
            .map((g) => ({ id: Number(g.id), name: String(g.name ?? g.id) }))
            .filter((g) => Number.isFinite(g.id) && g.id > 0);
    } catch (e) {
        console.error('Failed to load /games', e);
        gamesList = [
            { id: 2, name: 'Tetris' },
            { id: 3, name: 'Snake' },
            { id: 1, name: 'Racing' },
            { id: 4, name: 'Arcanoid' },
        ];
    }
}

function _canvasRectToLocal(e) {
    if (!$gameCanvas) return null;
    const rect = $gameCanvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    return { x, y, w: rect.width, h: rect.height };
}

function _hitTest(buttons, x, y) {
    for (const b of buttons) {
        if (!b) continue;
        if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) return b;
    }
    return null;
}

function _drawButton(ctx, b, opts = {}) {
    const radius = opts.radius ?? 10;
    const fill = opts.fill ?? 'rgba(0,0,0,0.35)';
    const stroke = opts.stroke ?? 'rgba(255,255,255,0.18)';
    const text = opts.text ?? b.label ?? '';
    const textColor = opts.textColor ?? '#f5f5f5';
    const font = opts.font ?? '14px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';

    ctx.save();
    ctx.beginPath();
    const x = b.x, y = b.y, w = b.w, h = b.h;
    ctx.moveTo(x + radius, y);
    ctx.arcTo(x + w, y, x + w, y + h, radius);
    ctx.arcTo(x + w, y + h, x, y + h, radius);
    ctx.arcTo(x, y + h, x, y, radius);
    ctx.arcTo(x, y, x + w, y, radius);
    ctx.closePath();
    ctx.fillStyle = fill;
    ctx.fill();
    ctx.strokeStyle = stroke;
    ctx.lineWidth = 1;
    ctx.stroke();

    ctx.fillStyle = textColor;
    ctx.font = font;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, x + w / 2, y + h / 2);
    ctx.restore();
}

function renderArcanoidCanvasMenu() {
    if (!canvasCtx || !$gameCanvas) return;
    _ensureCanvasSize();
    const ctx = canvasCtx;
    const rect = $gameCanvas.getBoundingClientRect();
    const W = rect.width;
    const H = rect.height;

    const bg = _cssVarColor('--tile-color', '#0f0f0f');
    const accent = _cssVarColor('--tile-active-color', '#ff9f1a');
    ctx.clearRect(0, 0, $gameCanvas.width, $gameCanvas.height);
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, W, H);

    ctx.save();
    ctx.fillStyle = '#f5f5f5';
    ctx.font = '18px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.fillText('BRICKGAME', W / 2, 14);
    ctx.font = '14px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.fillStyle = 'rgba(245,245,245,0.85)';
    ctx.fillText('Выберите игру', W / 2, 40);
    ctx.restore();

    const buttons = [];
    const btnW = Math.min(220, W - 40);
    const btnH = 34;
    const gap = 10;
    const startY = 70;
    const x = (W - btnW) / 2;

    const list = (gamesList && gamesList.length) ? gamesList : [
        { id: 2, name: 'Tetris' },
        { id: 3, name: 'Snake' },
        { id: 1, name: 'Racing' },
        { id: 4, name: 'Arcanoid' },
    ];

    let y = startY;
    for (const g of list) {
        buttons.push({
            kind: 'select-game',
            gameId: g.id,
            label: `${g.id}) ${g.name}`,
            x,
            y,
            w: btnW,
            h: btnH,
        });
        y += btnH + gap;
        if (y + btnH > H - 10) break;
    }

    for (const b of buttons) {
        const isArc = b.gameId === 4;
        _drawButton(ctx, b, {
            fill: isArc ? 'rgba(255,159,26,0.18)' : 'rgba(0,0,0,0.35)',
            stroke: isArc ? 'rgba(255,159,26,0.45)' : 'rgba(255,255,255,0.18)',
        });
    }

    // footer help
    ctx.save();
    ctx.fillStyle = 'rgba(245,245,245,0.7)';
    ctx.font = '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'bottom';
    ctx.fillText('Esc: меню  |  P: пауза  |  R: рестарт', W / 2, H - 8);
    ctx.restore();

    canvasButtons = buttons;
}

function renderArcanoidCanvas(state) {
    if (!canvasCtx || !$gameCanvas) return;
    _ensureCanvasSize();

    const field = Array.isArray(state?.field) ? state.field : [];
    const wCells = rootStyles['--game-board-width'] ?? 10;
    const hCells = rootStyles['--game-board-height'] ?? 20;
    const tile = _tileSizePx();
    const gap = 2;
    const pad = gap;

    const bg = _cssVarColor('--tile-color', '#0f0f0f');
    const active = _cssVarColor('--tile-active-color', '#ff9f1a');

    canvasCtx.clearRect(0, 0, $gameCanvas.width, $gameCanvas.height);

    // background
    canvasCtx.fillStyle = bg;
    canvasCtx.fillRect(0, 0, wCells * tile + (wCells + 1) * gap, hCells * tile + (hCells + 1) * gap);

    // cells
    canvasCtx.fillStyle = active;
    for (let r = 0; r < hCells; r++) {
        for (let c = 0; c < wCells; c++) {
            const on = Boolean(field?.[r]?.[c]);
            if (!on) continue;
            const x = pad + c * (tile + gap);
            const y = pad + r * (tile + gap);
            canvasCtx.fillRect(x, y, tile, tile);
        }
    }
}

function applyGameTheme(gameId) {
    // “консольные” оттенки для разных игр
    const root = document.documentElement;
    if (!root) return;

    if (gameId === 2) {
        // Тетрис — CYAN (как в ncurses init_pair(5))
        root.style.setProperty('--tile-active-color', '#00e5ff');
    } else if (gameId === 3) {
        // Змейка — GREEN (как init_pair(10))
        root.style.setProperty('--tile-active-color', '#2eea6a');
    } else if (gameId === 1) {
        // Гонки — RED (как init_pair(7))
        root.style.setProperty('--tile-active-color', '#ff3b3b');
    } else if (gameId === 4) {
        // Арканоид — теплый оранжевый цвет
        root.style.setProperty('--tile-active-color', '#ff9f1a');
    } else {
        // Меню/по умолчанию
        root.style.setProperty('--tile-active-color', '#00e5ff');
    }
}

function showGameOverModal(score) {
    if ($modalScore) $modalScore.textContent = String(score ?? 0);
    if ($gameOverModal) $gameOverModal.classList.remove('hidden');
}

function hideGameOverModal() {
    if ($gameOverModal) $gameOverModal.classList.add('hidden');
}

function showPauseModal() {
    if ($pauseModal) $pauseModal.classList.remove('hidden');
}

function hidePauseModal() {
    if ($pauseModal) $pauseModal.classList.add('hidden');
}

function setActiveSidePanel(gameId) {
    if ($panelTetris) $panelTetris.classList.toggle('hidden', gameId !== 2);
    if ($panelSnake) $panelSnake.classList.toggle('hidden', gameId !== 3);
    if ($panelRace) $panelRace.classList.toggle('hidden', gameId !== 1);
    if ($panelArcanoid) $panelArcanoid.classList.toggle('hidden', gameId !== 4);
}

function ensureMeterCells(container, count) {
    if (!container) return;
    const wanted = Math.max(0, Number(count) || 0);
    while (container.childElementCount < wanted) {
        const c = document.createElement('div');
        c.className = 'meter-cell';
        container.appendChild(c);
    }
    while (container.childElementCount > wanted) {
        container.removeChild(container.lastElementChild);
    }
}

function setMeterValue(container, valueOn, maxCount) {
    if (!container) return;
    const max = Math.max(0, Number(maxCount) || 0);
    const on = Math.max(0, Math.min(max, Number(valueOn) || 0));
    ensureMeterCells(container, max);
    const cells = container.children;
    for (let i = 0; i < cells.length; i++) {
        cells[i]?.classList.toggle('on', i < on);
    }
}

function initNextBoard() {
    if (!$nextBoard) return;
    if ($nextBoard.childElementCount > 0) return;
    for (let i = 0; i < 16; i++) {
        const t = document.createElement('div');
        t.className = 'next-tile';
        $nextBoard.appendChild(t);
    }
}

function setNextVisible(visible) {
    if (!$nextPanel) return;
    $nextPanel.style.display = visible ? 'block' : 'none';
}

function renderNext(state) {
    // В консольном тетрисе next рисуется как квадрат next_size x next_size.
    // На вебе делаем компактно 4x4 (в твоём проекте это типично для тетриса).
    if (!state || selectedGameId !== 2 || !$nextBoard) {
        setNextVisible(false);
        return;
    }
    initNextBoard();
    setNextVisible(true);

    const next = Array.isArray(state.next) ? state.next : null;
    const tiles = $nextBoard.children;
    for (let i = 0; i < 16; i++) {
        tiles[i]?.classList.remove('on');
    }
    if (!next) return;

    /** Центрируем фигуру в превью 4×4 (раньше прижималась к левому верхнему углу). */
    const occupied = [];
    for (let r = 0; r < 4; r++) {
        for (let c = 0; c < 4; c++) {
            if (next[r]?.[c]) occupied.push([r, c]);
        }
    }
    if (occupied.length === 0) return;

    let minR = 4;
    let maxR = -1;
    let minC = 4;
    let maxC = -1;
    for (const [r, c] of occupied) {
        minR = Math.min(minR, r);
        maxR = Math.max(maxR, r);
        minC = Math.min(minC, c);
        maxC = Math.max(maxC, c);
    }
    const h = maxR - minR + 1;
    const w = maxC - minC + 1;
    const offR = Math.floor((4 - h) / 2);
    const offC = Math.floor((4 - w) / 2);

    for (const [r, c] of occupied) {
        const nr = r - minR + offR;
        const nc = c - minC + offC;
        if (nr >= 0 && nr < 4 && nc >= 0 && nc < 4) {
            const idx = nr * 4 + nc;
            tiles[idx]?.classList.add('on');
        }
    }
}

function renderState(state) {
    currentState = state;
    const { field, field_colors, score, high_score, level, speed, game_over, pause, nitro, lives } = state;
    setActiveSidePanel(selectedGameId);

    if (selectedGameId === 4) {
        setCanvasUiMode(true);
        canvasScreen = 'playing';
        renderArcanoidCanvas(state);

        // HUD overlays on canvas
        if (canvasCtx && $gameCanvas) {
            const rect = $gameCanvas.getBoundingClientRect();
            const W = rect.width;
            const H = rect.height;
            const ctx = canvasCtx;
            ctx.save();
            ctx.fillStyle = 'rgba(0,0,0,0.45)';
            ctx.fillRect(8, 8, 124, 64);
            ctx.fillStyle = '#f5f5f5';
            ctx.font = '12px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'top';
            ctx.fillText(`Score: ${score ?? 0}`, 14, 14);
            ctx.fillText(`High:  ${high_score ?? 0}`, 14, 30);
            ctx.fillText(`Lv: ${level ?? 1}  Sp: ${speed ?? 1}`, 14, 46);
            ctx.restore();

            // overlays for pause / game over
            if (pause || game_over) {
                const buttons = [];
                ctx.save();
                ctx.fillStyle = 'rgba(0,0,0,0.65)';
                ctx.fillRect(0, 0, W, H);
                ctx.fillStyle = '#f5f5f5';
                ctx.font = '18px ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'top';
                const title = game_over ? 'GAME OVER' : 'PAUSED';
                ctx.fillText(title, W / 2, 36);

                const bw = Math.min(200, W - 40);
                const bh = 34;
                const x = (W - bw) / 2;
                let y = 72;
                if (!game_over) {
                    buttons.push({ kind: 'continue', label: 'Продолжить', x, y, w: bw, h: bh });
                    y += bh + 10;
                }
                buttons.push({ kind: 'restart', label: 'Рестарт', x, y, w: bw, h: bh });
                y += bh + 10;
                buttons.push({ kind: 'menu', label: 'В меню', x, y, w: bw, h: bh });

                for (const b of buttons) {
                    _drawButton(ctx, b, {
                        fill: 'rgba(0,0,0,0.35)',
                        stroke: 'rgba(255,255,255,0.25)',
                    });
                }
                ctx.restore();
                canvasButtons = buttons;
            } else {
                canvasButtons = [];
            }
        }
    } else {
        setCanvasUiMode(false);
        setBoardRenderMode(false);
        // рендер игрового поля
        // GameBoard ожидает координаты как (x=column, y=row),
        // поэтому передаём (col, row), а не (row, col)
        const colors = Array.isArray(field_colors) ? field_colors : null;
        for (let row = 0; row < field.length; row++) {
            for (let col = 0; col < field[row].length; col++) {
                const v = colors ? (colors[row]?.[col] ?? 0) : (field[row][col] ? 3 : 0);
                gameBoard.setTileValue(col, row, v);
            }
        }
    }

    if (selectedGameId === 2) {
        if ($scoreTetris) $scoreTetris.textContent = String(score);
        if ($highScoreTetris) $highScoreTetris.textContent = String(high_score);
        if ($levelTetris) $levelTetris.textContent = String(level);
        if ($speedTetris) $speedTetris.textContent = String(speed);
    } else if (selectedGameId === 3) {
        if ($scoreSnake) $scoreSnake.textContent = String(score);
        if ($highScoreSnake) $highScoreSnake.textContent = String(high_score);
    } else if (selectedGameId === 1) {
        if ($scoreRace) $scoreRace.textContent = String(score);
        if ($highScoreRace) $highScoreRace.textContent = String(high_score);
        if ($levelRace) $levelRace.textContent = String(level ?? 1);
        if (lives != null) {
            setMeterValue($healthBar, lives, RACE_MAX_LIVES);
        } else {
            setMeterValue($healthBar, 0, RACE_MAX_LIVES);
        }
        if ($nitroIndicator) {
            const on = Boolean(nitro);
            $nitroIndicator.textContent = on ? 'ВКЛ' : 'ВЫКЛ';
            $nitroIndicator.classList.toggle('on', on);
            $nitroIndicator.classList.toggle('off', !on);
        }
    } else if (selectedGameId === 4) {
        if ($scoreArcanoid) $scoreArcanoid.textContent = String(score);
        if ($highScoreArcanoid) $highScoreArcanoid.textContent = String(high_score);
        if ($levelArcanoid) $levelArcanoid.textContent = String(level ?? 1);
        if ($speedArcanoid) $speedArcanoid.textContent = String(speed ?? 1);
    }

    renderNext(state);

    if (game_over) {
        hidePauseModal();
        showGameOverModal(score);
    } else {
        hideGameOverModal();
    }

    if (!game_over && pause) {
        showPauseModal();
    } else {
        hidePauseModal();
    }
}

async function fetchAndRenderState() {
    const session = gameSession;
    if (fetchAndRenderState._inFlight) return;
    fetchAndRenderState._inFlight = true;
    try {
        if (canvasUiMode && canvasScreen === 'menu') return;
        const state = await apiGet('/state');
        if (session !== gameSession || !selectedGameId) return;
        renderState(state);
    } catch (e) {
        console.error(e);
    } finally {
        fetchAndRenderState._inFlight = false;
    }
}

async function sendAction(actionId, hold) {
    try {
        await apiPost('/actions', { action_id: actionId, hold });
    } catch (e) {
        console.error(e);
    }
}

function showMenu() {
    // инвалидация всех “в полёте” запросов состояния
    gameSession += 1;
    // попросим сервер остановить текущую игру (не критично, если не сработает)
    if (selectedGameId) {
        fetch(`${API_BASE_URL}/games`, { method: 'DELETE' }).catch(() => {});
    }
    if (canvasUiMode) {
        selectedGameId = null;
        currentState = null;
        clearBoard();
        applyGameTheme(4);
        setCanvasUiMode(true);
        canvasScreen = 'menu';
        renderArcanoidCanvasMenu();
        return;
    }

    selectedGameId = null;
    hideGameOverModal();
    hidePauseModal();
    clearBoard();
    setBoardRenderMode(false);
    applyGameTheme(null);
    currentState = null;
    renderNext(null);
    if ($scoreTetris) $scoreTetris.textContent = '0';
    if ($levelTetris) $levelTetris.textContent = '1';
    if ($speedTetris) $speedTetris.textContent = '1';
    if ($scoreSnake) $scoreSnake.textContent = '0';
    if ($scoreRace) $scoreRace.textContent = '0';
    if ($levelRace) $levelRace.textContent = '1';
    if ($scoreArcanoid) $scoreArcanoid.textContent = '0';
    if ($levelArcanoid) $levelArcanoid.textContent = '1';
    if ($speedArcanoid) $speedArcanoid.textContent = '1';
    setMeterValue($healthBar, 0, RACE_MAX_LIVES);
    setActiveSidePanel(null);
    if ($gameContainer) $gameContainer.style.display = 'none';
    if ($menuView) $menuView.style.display = 'block';
}

async function restartGame() {
    if (!selectedGameId) return;
    await apiPost(`/games/${selectedGameId}`);
    await fetchAndRenderState();
}

async function togglePause() {
    if (!selectedGameId) return;
    await sendAction(ActionId.Pause, false);
    await fetchAndRenderState();
}

document.addEventListener('keydown', function (event) {
    if (!selectedGameId) {
        // In canvas menu we still want Esc to keep menu visible.
        if (canvasUiMode && canvasScreen === 'menu') return;
        return;
    }
    if (currentState && currentState.game_over) {
        if (event.code === 'KeyR') {
            restartGame();
        } else if (event.code === 'Escape') {
            showMenu();
        }
        return;
    }
    if (event.code === 'Escape') {
        showMenu();
        return;
    }
    if (event.code === 'KeyP') {
        togglePause();
        return;
    }
    if (event.code === 'KeyR') {
        restartGame();
        return;
    }

    // Для тетриса поворот ожидается по `Space` (как правило в консольной версии — клавиша "Action").
    if (selectedGameId === 2 && event.code === 'Space') {
        // В бекенде поворот срабатывает и на `ActionId.Up` (см. `tetris.c`).
        sendAction(ActionId.Up, false);
        return;
    }
    if (selectedGameId === 4 && event.code === 'Space') {
        // Для арканоида пробел = запуск мяча (action).
        sendAction(ActionId.Action, false);
        return;
    }

    if (keyCodes.up.includes(event.code)) {
        if (selectedGameId === 1) {
            sendAction(ActionId.Up, true);
        } else if (selectedGameId === 2) {
            sendAction(ActionId.Up, false);
        } else {
            sendAction(ActionId.Up, false);
        }
    }
    if (keyCodes.right.includes(event.code)) {
        sendAction(ActionId.Right, false);
    }
    if (keyCodes.down.includes(event.code)) {
        if (selectedGameId === 2) {
            if (event.repeat) return;
            startTetrisSoftDrop();
        } else {
            sendAction(ActionId.Down, false);
        }
    }
    if (keyCodes.left.includes(event.code)) {
        sendAction(ActionId.Left, false);
    }
});

document.addEventListener('keyup', function (event) {
    if (!selectedGameId) return;
    if (selectedGameId === 1 && keyCodes.up.includes(event.code)) {
        sendAction(ActionId.Up, false);
    }
    if (selectedGameId === 2 && keyCodes.down.includes(event.code)) {
        stopTetrisSoftDrop();
    }
});

let _tetrisSoftDropTimer = null;
function startTetrisSoftDrop() {
    if (_tetrisSoftDropTimer) return;
    sendAction(ActionId.Down, false);
    _tetrisSoftDropTimer = setInterval(() => {
        if (selectedGameId !== 2) return;
        if (currentState && currentState.game_over) return;
        sendAction(ActionId.Down, false);
    }, 60);
}

function stopTetrisSoftDrop() {
    if (!_tetrisSoftDropTimer) return;
    clearInterval(_tetrisSoftDropTimer);
    _tetrisSoftDropTimer = null;
}

function addPressFeedback(btn) {
    if (!btn) return;
    const down = () => btn.classList.add('is-pressed');
    const up = () => btn.classList.remove('is-pressed');
    btn.addEventListener('mousedown', down);
    btn.addEventListener('touchstart', down, { passive: true });
    btn.addEventListener('mouseup', up);
    btn.addEventListener('mouseleave', up);
    btn.addEventListener('touchend', up);
    btn.addEventListener('touchcancel', up);
}
addPressFeedback($consoleBtnPause);
addPressFeedback($consoleBtnRestart);
addPressFeedback($consoleBtnMenu);

async function selectGame(gameId) {
    // инвалидация “старых” ответов состояния + очистка экрана
    gameSession += 1;
    clearBoard();
    applyGameTheme(gameId);
    renderNext(null);
    setActiveSidePanel(gameId);
    if (gameId === 4) {
        setCanvasUiMode(true);
        canvasScreen = 'playing';
    } else {
        setCanvasUiMode(false);
    }
    setBoardRenderMode(gameId === 4);

    await apiPost(`/games/${gameId}`);
    selectedGameId = gameId;
    hideGameOverModal();
    hidePauseModal();
    if ($gameContainer) $gameContainer.style.display = 'block';
    if ($menuView) $menuView.style.display = 'none';
    await fetchAndRenderState();
}

if ($gameSelection) {
    $gameSelection.addEventListener('click', async (event) => {
        const target = event.target;
        if (!(target instanceof HTMLButtonElement)) return;

        const gameId = Number(target.getAttribute('data-game-id'));
        if (!gameId) return;

        try {
            await selectGame(gameId);
        } catch (e) {
            console.error('Failed to select game', e);
        }
    });
}

if ($btnMenu) {
    $btnMenu.addEventListener('click', showMenu);
}
if ($btnRestart) {
    $btnRestart.addEventListener('click', () => restartGame().catch(console.error));
}
if ($btnPause) {
    $btnPause.addEventListener('click', () => togglePause().catch(console.error));
}

if ($consoleBtnMenu) {
    $consoleBtnMenu.addEventListener('click', showMenu);
}
if ($consoleBtnRestart) {
    $consoleBtnRestart.addEventListener('click', () => restartGame().catch(console.error));
}
if ($consoleBtnPause) {
    $consoleBtnPause.addEventListener('click', () => togglePause().catch(console.error));
}

if ($modalMenu) {
    $modalMenu.addEventListener('click', showMenu);
}
if ($modalRestart) {
    $modalRestart.addEventListener('click', () => restartGame().catch(console.error));
}

if ($pauseMenu) {
    $pauseMenu.addEventListener('click', showMenu);
}
if ($pauseContinue) {
    $pauseContinue.addEventListener('click', () => togglePause().catch(console.error));
}

setInterval(() => {
    if (selectedGameId) {
        fetchAndRenderState();
    }
}, 200);

if ($gameCanvas) {
    $gameCanvas.addEventListener('click', async (e) => {
        if (!canvasUiMode) return;
        const pos = _canvasRectToLocal(e);
        if (!pos) return;
        const hit = _hitTest(canvasButtons, pos.x, pos.y);
        if (!hit) return;

        try {
            if (canvasScreen === 'menu' && hit.kind === 'select-game') {
                const gid = Number(hit.gameId);
                if (!gid) return;
                // If user selected a non-arcanoid game from canvas menu,
                // exit canvas mode and use normal HTML UI for that game.
                if (gid !== 4) {
                    setCanvasUiMode(false);
                    await selectGame(gid);
                    return;
                }
                await selectGame(4);
                await fetchAndRenderState();
                return;
            }
            if (canvasScreen === 'playing') {
                if (hit.kind === 'menu') {
                    showMenu();
                    return;
                }
                if (hit.kind === 'restart') {
                    await restartGame();
                    return;
                }
                if (hit.kind === 'continue') {
                    await togglePause();
                    return;
                }
            }
        } catch (err) {
            console.error(err);
        }
    });
}

// initial load: prefetch games for canvas menu
loadGamesList().then(() => {
    // no-op
}).catch(() => {});
