const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

const OUT = path.join(__dirname, '..', 'docs', 'screenshots');
const URL = 'http://127.0.0.1:8007/';
const VW = 1280;
const VH = 720;

function uiGeom() {
  const tile = Math.round(20 * 1.35);
  const gap = 2;
  const hudW = 170;
  const w = Math.round(10 * tile + 11 * gap + hudW);
  const h = Math.round(20 * tile + 21 * gap);
  const ox = Math.round((VW - w) / 2);
  const oy = Math.round((VH - h) / 2);
  const btnW = Math.min(280, w - 60);
  const btnH = 42;
  const btnGap = 10;
  const firstY = 112;
  const girlScale = 7;
  const girlW = 17 * girlScale;
  const girlGap = 26;
  const groupW = btnW + girlGap + girlW;
  const bx = Math.max(8, Math.round((w - groupW) / 2));
  return { ox, oy, btnW, btnH, btnGap, firstY, bx };
}

async function typeNick(page) {
  const g = uiGeom();
  const nickX = VW - g.btnW - 20 + g.btnW / 2;
  const nickY = g.oy + g.firstY + 30;
  await page.mouse.click(nickX, nickY);
  await page.waitForTimeout(150);
  for (let i = 0; i < 24; i++) await page.keyboard.press('Backspace');
  await page.keyboard.type('demo_player');
  await page.keyboard.press('Enter');
  await page.waitForTimeout(250);
}

async function openGame(page, index) {
  const g = uiGeom();
  const cx = g.ox + g.bx + g.btnW / 2;
  const cy = g.oy + g.firstY + index * (g.btnH + g.btnGap) + g.btnH / 2;
  await typeNick(page);
  await page.mouse.click(cx, cy);
  await page.waitForTimeout(4000);
}

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: VW, height: VH } });

  await page.goto(URL, { waitUntil: 'networkidle' });
  await page.waitForTimeout(2500);
  await page.screenshot({ path: path.join(OUT, 'menu.png') });
  console.log('saved menu.png');

  const games = [
    ['tetris.png', 0],
    ['snake.png', 1],
    ['race.png', 2],
    ['arcanoid.png', 3],
  ];

  for (const [file, idx] of games) {
    await page.goto(URL, { waitUntil: 'networkidle' });
    await page.waitForTimeout(2000);
    await openGame(page, idx);
    await page.screenshot({ path: path.join(OUT, file) });
    console.log('saved', file);
  }

  await browser.close();
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
