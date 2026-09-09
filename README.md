![Алёна — маскот BrickGame Arcade](docs/screenshots/alena-talk.gif)

# BrickGame Arcade

**Четыре классические аркады** — Тетрис · Змейка · Гонки · Арканоид  
в одном проекте с тремя интерфейсами: **Web**, **Desktop** и **Console**

![Docker](https://img.shields.io/badge/run-Docker-2496ED?style=flat-square&logo=docker&logoColor=white) ![MIT](https://img.shields.io/badge/license-MIT-green?style=flat-square) ![Python](https://img.shields.io/badge/Python-FastAPI-3776AB?style=flat-square&logo=python&logoColor=white) ![C++](https://img.shields.io/badge/C%2FC%2B%2B-WASM%20%7C%20Qt-00599C?style=flat-square&logo=cplusplus&logoColor=white)

---



## О проекте

**BrickGame Arcade** — четыре классические аркады в одном проекте:


| Игра         | Логика       |
| ------------ | ------------ |
| **Тетрис**   | C            |
| **Змейка**   | C++          |
| **Гонки**    | Python (FSM) |
| **Арканоид** | Python (FSM) |


Клиенты:


| Интерфейс   | Технологии                             | Где удобнее запускать            |
| ----------- | -------------------------------------- | -------------------------------- |
| **Web**     | Canvas, FastAPI, WebAssembly, Pyodide  | Windows / macOS / Linux (Docker) |
| **Desktop** | Qt5 (C++)                              | Linux / WSL                      |
| **Console** | ncurses (C/C++) + Python curses-клиент | Linux / WSL                      |


---



## Веб-интерфейс

<p align="center">
  <img src="docs/screenshots/01-menu.png" alt="Главное меню" width="520"/><br/>
  <em>Главное меню</em>
</p>

<table align="center">
  <tr>
    <td align="center"><img src="docs/screenshots/02-tetris.png" alt="Тетрис" width="220"/></td>
    <td align="center"><img src="docs/screenshots/03-snake.png" alt="Змейка" width="220"/></td>
    <td align="center"><img src="docs/screenshots/04-race.png" alt="Гонки" width="220"/></td>
    <td align="center"><img src="docs/screenshots/05-arcanoid.png" alt="Арканоид" width="220"/></td>
  </tr>
</table>

---


## Возможности

- Единая игровая логика для нескольких клиентов
- Веб-UI на Canvas с пиксельной эстетикой
- WASM (Тетрис / Змейка) и Pyodide (Гонки / Арканоид) в браузере
- Общий рейтинг: **ТОП-5** в меню, **ТОП-21** в игре (очки + уровень)
- Qt-десктоп и консольный клиент (ncurses / Python)
- Сборка и запуск веба через Docker

---

## Быстрый старт

### Web (рекомендуется)

```bash
docker compose up --build -d
```

Откройте в браузере: [http://127.0.0.1:8007](http://127.0.0.1:8007)

Локально без Docker:

```powershell
# PowerShell
$env:PYTHONPATH = "src"
python -m uvicorn brick_game.server.app:app --host 127.0.0.1 --port 8007
```

Подробнее про WASM: `[src/README_WASM_DOCKER.md](src/README_WASM_DOCKER.md)`.

### Console (Linux / WSL)

```bash
cd src/brick_game
make console
./build/console
```

Python-консоль (нужен запущенный сервер на `:8007`):

```bash
# терминал 1
PYTHONPATH=src python -m uvicorn brick_game.server.app:app --host 127.0.0.1 --port 8007

# терминал 2
PYTHONPATH=src python -m brick_game.console --base-url http://127.0.0.1:8007
```



### Desktop / Qt (Linux / WSL)

Зависимости: `g++`, Qt5 (`Qt5Widgets`, `Qt5Network`), `moc`, ncurses.

```bash
cd src/brick_game
make desktop
./build/desktop
```

Собрать console + desktop сразу:

```bash
cd src/brick_game
make all
```

---

## Управление (Web)


| Клавиша        | Действие           |
| -------------- | ------------------ |
| ← → ↑ ↓        | Движение / поворот |
| Space / Action | Действие игры      |
| `P`            | Пауза              |
| `R`            | Рестарт            |
| `Esc`          | Меню               |


---

## Структура репозитория

```text
├── docker-compose.yml          # веб-сервис
├── docs/screenshots/           # скриншоты для README
├── src/brick_game/
│   ├── server/                 # FastAPI + рейтинг
│   ├── gui/web/                # Canvas web-клиент
│   ├── gui/desktop/            # Qt UI
│   ├── gui/cli/                # ncurses UI
│   ├── console/                # Python console-клиент
│   ├── tetris/ snake/          # C / C++
│   ├── race/ arcanoid/         # Python + FSM
│   └── Makefile                # console / desktop / тесты
└── LICENSE                     # MIT
```

---

## Стек

- **Python 3** — FastAPI, uvicorn, игровая логика (race / arcanoid), тесты
- **C / C++** — tetris, snake, native bridge
- **WebAssembly / Emscripten** — браузерный запуск C/C++ игр
- **Pyodide** — Python-игры в браузере
- **Qt5** — десктопный клиент
- **ncurses** — консольный клиент
- **Docker** — сборка и деплой веба
- **SQLite** — таблица рейтинга

---



## Лицензия

Распространяется на условиях **[MIT License](LICENSE)**.

Copyright © [fairyofmiracle](https://github.com/fairyofmiracle) 