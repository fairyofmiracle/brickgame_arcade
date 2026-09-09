export function drawGameFx(ctx, opts) {
    if (!ctx) return;
    const t = Number(opts?.t ?? 0);
    const gameId = Number(opts?.gameId ?? 0);
    const box = opts?.box || { x: 0, y: 0, w: 0, h: 0 };
    const x0 = Number(box.x || 0);
    const y0 = Number(box.y || 0);
    const w = Number(box.w || 0);
    const h = Number(box.h || 0);
    const roundRect = opts?.roundRect;
    const accent = String(opts?.accent || '#cbb7ff');
    const alpha = Number.isFinite(opts?.alpha) ? opts.alpha : 0.85;

    if (!roundRect || w <= 2 || h <= 2) return;

    const clamp01 = (x) => Math.max(0, Math.min(1, Number.isFinite(x) ? x : 0));
    const lerp = (a, b, tt) => a + (b - a) * tt;
    const easeInOut = (tt) => {
        const x = clamp01(tt);
        return x < 0.5 ? 2 * x * x : 1 - Math.pow(-2 * x + 2, 2) / 2;
    };

    const drawPixelSprite = (sprite, pal, px, py, scale) => {
        const s = Math.max(1, Math.floor(Number(scale) || 1));
        ctx.save();
        ctx.imageSmoothingEnabled = false;
        for (let r = 0; r < sprite.length; r++) {
            const row = sprite[r];
            for (let c = 0; c < row.length; c++) {
                const ch = row[c];
                if (ch === '.' || ch === ' ') continue;
                const color = pal[ch];
                if (!color) continue;
                ctx.fillStyle = color;
                ctx.fillRect(Math.floor(px + c * s), Math.floor(py + r * s), s, s);
            }
        }
        ctx.restore();
        return { w: sprite[0].length * s, h: sprite.length * s };
    };

    ctx.save();
    ctx.globalAlpha = alpha;
    ctx.beginPath();
    ctx.rect(x0, y0, w, h);
    ctx.clip();

    // soft glow band
    const g = ctx.createLinearGradient(x0, y0, x0 + w, y0);
    g.addColorStop(0, 'rgba(0,0,0,0)');
    g.addColorStop(0.5, `${accent}33`);
    g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g;
    ctx.fillRect(x0, y0, w, h);

    const mode = String(opts?.mode || 'menu'); // menu | playing

    // In playing mode we keep FX only for Tetris.
    if (mode === 'playing' && gameId !== 2) {
        ctx.restore();
        return;
    }

    if (gameId === 2) {
        // Tetris: subtle falling tetrominoes (background hint; no pile/stack).
        const pal = ['#ff7ad9', '#7dffb1', '#ffd84a', '#cbb7ff', accent];
        // In menu we want a bigger, centered composition.
        const fxX0 = mode === 'menu' ? x0 + w * 0.18 : x0;
        const fxY0 = mode === 'menu' ? y0 + h * 0.08 : y0;
        const fxW = mode === 'menu' ? w * 0.64 : w;
        const fxH = mode === 'menu' ? h * 0.84 : h;

        const cell =
            mode === 'menu'
                ? Math.max(26, Math.round(Math.min(fxW / 14, fxH / 14)))
                : Math.max(18, Math.round(Math.min(w / 20, h / 20)));
        const gap = 2;
        const cols = Math.max(8, Math.floor(fxW / (cell + gap)));
        const rows = Math.max(10, Math.floor(fxH / (cell + gap)));

        const shapes = [
            // I
            [
                [0, 0, 0, 0],
                [1, 1, 1, 1],
                [0, 0, 0, 0],
                [0, 0, 0, 0],
            ],
            // J
            [
                [1, 0, 0],
                [1, 1, 1],
                [0, 0, 0],
            ],
            // L
            [
                [0, 0, 1],
                [1, 1, 1],
                [0, 0, 0],
            ],
            // O
            [
                [1, 1],
                [1, 1],
            ],
            // S
            [
                [0, 1, 1],
                [1, 1, 0],
                [0, 0, 0],
            ],
            // T
            [
                [0, 1, 0],
                [1, 1, 1],
                [0, 0, 0],
            ],
            // Z
            [
                [1, 1, 0],
                [0, 1, 1],
                [0, 0, 0],
            ],
        ];

        const rotCW = (m) => {
            const n = m.length;
            const res = Array.from({ length: n }, () => Array(n).fill(0));
            for (let r = 0; r < n; r++) {
                for (let c = 0; c < n; c++) {
                    res[c][n - r - 1] = m[r][c] ? 1 : 0;
                }
            }
            return res;
        };

        const shapeCells = (m) => {
            const cells = [];
            for (let r = 0; r < m.length; r++) {
                for (let c = 0; c < m[r].length; c++) {
                    if (m[r][c]) cells.push([r, c]);
                }
            }
            return cells;
        };

        const drawFalling = (cells, gx, gy, color) => {
            ctx.fillStyle = color;
            for (const [r, c] of cells) {
                const xx = fxX0 + (gx + c) * (cell + gap);
                const yy = fxY0 + (gy + r) * (cell + gap);
                roundRect(xx, yy, cell, cell, 5);
                ctx.fill();
            }
        };

        const pieces = mode === 'playing' ? 6 : 7;
        const lanes = Math.max(5, Math.floor(cols / 2));
        for (let i = 0; i < pieces; i++) {
            const kind = (i * 2 + 1) % shapes.length;
            let sh = shapes[kind];
            const rcount = (i * 3 + kind) % 4;
            for (let k = 0; k < rcount; k++) sh = rotCW(sh);
            const cells = shapeCells(sh);
            const color = pal[(i + kind) % pal.length];

            const lane = (i * 3 + kind) % lanes;
            const gxBase = Math.floor((lane * cols) / lanes);
            const gx = Math.min(cols - sh[0].length, Math.max(0, gxBase));

            const speed = mode === 'playing' ? 3.6 : 3.2;
            const phase = i * 1.35;
            const travel = (t * speed + phase) % (rows + 12);
            const gy = Math.floor(travel) - 6;
            drawFalling(cells, gx, gy, color);
        }
    } else if (gameId === 3) {
        // Snake: apple falls onto a worm head at bottom
        const ax = x0 + w * 0.72;
        const ay = y0 + ((t * 50) % (h + 50)) - 30;
        ctx.save();
        ctx.fillStyle = '#ff4d6d';
        ctx.beginPath();
        ctx.arc(ax, ay, 10, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = '#2eea6a';
        ctx.fillRect(ax + 6, ay - 14, 2, 8);
        ctx.restore();

        const baseY = y0 + h - 22;
        const headX = x0 + w * 0.62;
        ctx.save();
        ctx.fillStyle = '#7dffb1';
        for (let i = 0; i < 7; i++) {
            const px = headX - i * 14;
            const py = baseY + Math.sin(t * 4 - i * 0.7) * 2;
            ctx.beginPath();
            ctx.arc(px, py, 7, 0, Math.PI * 2);
            ctx.fill();
        }
        // eyes on head
        ctx.fillStyle = '#0b0b10';
        ctx.beginPath();
        ctx.arc(headX + 2, baseY - 2, 1.6, 0, Math.PI * 2);
        ctx.arc(headX - 2, baseY - 2, 1.6, 0, Math.PI * 2);
        ctx.fill();
        ctx.restore();
    } else if (gameId === 1) {
        // Racing: keep only the moving part (no static side parts).
        const p = (t * 0.22) % 1;
        const carW = Math.max(56, Math.round(w * 0.34));
        const carH = 26;
        const cx = x0 + lerp(-carW, w + carW, p);
        const cy = y0 + h * 0.62;
        ctx.save();
        ctx.fillStyle = '#ff7ad9';
        roundRect(cx, cy, carW, carH, 10);
        ctx.fill();
        ctx.fillStyle = 'rgba(255,255,255,0.35)';
        roundRect(cx + 10, cy + 6, Math.max(16, carW * 0.36), 10, 6);
        ctx.fill();
        ctx.fillStyle = '#0b0b10';
        ctx.beginPath();
        ctx.arc(cx + 14, cy + carH, 6, 0, Math.PI * 2);
        ctx.arc(cx + carW - 14, cy + carH, 6, 0, Math.PI * 2);
        ctx.fill();
        ctx.restore();
    } else if (gameId === 4) {
        // Arcanoid: keep only the moving part (rocket fly-by).
        const p = (t * 0.18) % 1;
        const rx = x0 + lerp(-80, w + 80, p);
        const ry = y0 + lerp(h + 40, -40, easeInOut(p));
        ctx.save();
        ctx.translate(rx, ry);
        ctx.rotate(-0.35);
        ctx.fillStyle = 'rgba(255,255,255,0.85)';
        roundRect(-22, -8, 44, 16, 8);
        ctx.fill();
        ctx.fillStyle = accent;
        roundRect(-10, -5, 20, 10, 5);
        ctx.fill();
        ctx.fillStyle = 'rgba(255,200,120,0.85)';
        ctx.beginPath();
        ctx.moveTo(-24, 0);
        ctx.lineTo(-44, -6);
        ctx.lineTo(-44, 6);
        ctx.closePath();
        ctx.fill();
        ctx.restore();
    }

    ctx.restore();
}

export function drawTetrisPile(ctx, opts) {
    if (!ctx) return;
    const roundRect = opts?.roundRect;
    if (!roundRect) return;
    const x0 = Number(opts?.x ?? 0);
    const y0 = Number(opts?.y ?? 0);
    const w = Number(opts?.w ?? 0);
    const h = Number(opts?.h ?? 0);
    const cell = Math.max(12, Number(opts?.cell ?? 14));
    const accent = String(opts?.accent || '#00e5ff');
    const pal = ['#ff7ad9', '#7dffb1', '#ffd84a', accent];
    const cols = Math.max(4, Math.floor(w / (cell + 2)));
    const rows = Math.max(2, Math.floor(h / (cell + 2)));

    ctx.save();
    ctx.globalAlpha = 0.9;
    for (let r = 0; r < rows; r++) {
        const blocks = cols - Math.floor((r * cols) / (rows + 1));
        const start = Math.floor((cols - blocks) / 2);
        for (let c = 0; c < blocks; c++) {
            const xx = x0 + (start + c) * (cell + 2);
            const yy = y0 + h - (r + 1) * (cell + 2);
            ctx.fillStyle = pal[(r + c) % pal.length];
            roundRect(xx, yy, cell, cell, 5);
            ctx.fill();
        }
    }
    ctx.restore();
}

