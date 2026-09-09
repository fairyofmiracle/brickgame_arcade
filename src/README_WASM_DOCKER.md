## WebAssembly сборка (только через Docker)

Веб-клиент запускает игры **без ручного переключения движка**:

- **Тетрис (id=2)** и **Змейка (id=3)** запускаются из `brickgame.wasm` (Emscripten, C/C++).
- **Гонки (id=1)** и **Арканоид (id=4)** запускаются в браузере через **Pyodide** (Python выполняется внутри WebAssembly-рантайма Pyodide).
- Если браузерный движок не доступен (например, не собран wasm), используется **fallback на REST**.

### Артефакты WASM (C/C++)

Для тетриса/змейки используются артефакты Emscripten:

- `src/brick_game/gui/web/static/wasm/brickgame.js`
- `src/brick_game/gui/web/static/wasm/brickgame.wasm`

### Сборка

Из корня репозитория:

```bash
docker compose run --rm --build wasm-build
```

После команды файлы появятся в `src/brick_game/gui/web/static/wasm/`.

### Запуск веба

```bash
docker compose up --build brickgame
```

Откройте вручную `http://127.0.0.1:8007`.

### Маскот Алёна и рейтинг

- **Алёна** в меню — статические подсказки при наведении.
- **Ник игрока** — поле справа в меню (`латиница`, `3–20` символов).
- **ТОП-5** в меню слева; **ТОП-21** во время игры (панель размера игрового блока).
- После game over счёт и уровень уходят на сервер (`POST /api/leaderboard`).

Опционально: `POST /api/mascot/say` с LLM (см. `MASCOT_LLM_API_KEY`) — сейчас в UI не используется.

### Скриншоты веб-клиента

Скриншоты: `docs/screenshots/` (`01-menu.png` … `05-arcanoid.png`, `alena-talk.gif`).

Скрипт повторного снятия (нужен Node + playwright):

```bash
npm install playwright
node scripts/capture_screenshots.js
```

## Про “Microsoft Code Style” vs PEP8 в этом проекте

В требованиях встречается формулировка “Microsoft Code Style”. В Python‑части
этого репозитория фактически применяется PEP8‑проверка через `pycodestyle`:

- **проектный режим**: `--max-line-length=127` + игноры `W391,W503`
- **строгий режим**: стандартный `pycodestyle` (79 символов)

Это описано в `src/brick_game/Makefile` в целях `lint_python` и `lint_python_strict`.


