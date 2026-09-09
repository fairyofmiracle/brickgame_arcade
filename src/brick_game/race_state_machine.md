# Конечный автомат игры «Гонки» (BrickGame v3.0)

## Состояния

| Состояние   | Описание                          |
|------------|-----------------------------------|
| `initial`  | Игра создана, до нажатия «Старт»  |
| `running`  | Идёт игра, тики симуляции активны |
| `paused`   | Пауза, поле не обновляется        |
| `game_over`| Конец игры (проигрыш / выход)     |

## Переходы

- **START** — из любого состояния при действии `Action.START`: новая сессия → `running`, сброс поля (рекорд `high_score` сохраняется).
- **PAUSE** из `running` → `paused`.
- **PAUSE** из `paused` → `running`.
- **TERMINATE** → `game_over`.
- **Столкновение** при `lives <= 0` после обработки удара → `game_over`.

```mermaid
stateDiagram-v2
    [*] --> initial: создание RaceGame

    initial --> running: START
    running --> paused: PAUSE
    paused --> running: PAUSE
    running --> game_over: TERMINATE
    running --> game_over: столкновение\n(lives = 0)
    paused --> game_over: TERMINATE
    initial --> game_over: TERMINATE

    game_over --> running: START\n(новая игра)
    paused --> running: START\n(опционально*, см. код)

    note right of running
        Ввод LEFT/RIGHT/UP (hold)
        только в running и не в pause
    end note
```

\*В текущей реализации `Action.START` полностью сбрасывает состояние в `RaceState(..., RUNNING)` и работает из любого статуса, включая `paused` и `game_over`.

## Упрощённая блок-схема обработки ввода

```mermaid
flowchart TD
    A[Вход: action, hold] --> B{action == START?}
    B -->|да| C[Новый RaceState RUNNING]
    B -->|нет| D{status == RUNNING и PAUSE?}
    D -->|да| E[status = PAUSED]
    D -->|нет| F{status == PAUSED и PAUSE?}
    F -->|да| G[status = RUNNING]
    F -->|нет| H{TERMINATE?}
    H -->|да| I[GAME_OVER]
    H -->|нет| J{RUNNING и не pause?}
    J -->|да| K[LEFT / RIGHT / UP]
    J -->|нет| L[игнор]
```
