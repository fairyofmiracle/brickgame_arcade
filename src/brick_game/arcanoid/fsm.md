## Диаграмма конечного автомата (Арканоид)

Ниже — диаграмма КА для `ArcanoidGame` из `src/brick_game/arcanoid/engine.py`.

```mermaid
stateDiagram-v2
    [*] --> INITIAL

    INITIAL --> RUNNING: Action.START\n(reset round)
    INITIAL --> GAME_OVER: Action.TERMINATE

    RUNNING --> PAUSED: Action.PAUSE
    PAUSED --> RUNNING: Action.PAUSE

    RUNNING --> GAME_OVER: Action.TERMINATE
    PAUSED --> GAME_OVER: Action.TERMINATE

    RUNNING --> WON: last brick destroyed\n(game_over=true)
    RUNNING --> GAME_OVER: ball missed paddle\n(game_over=true)

    WON --> RUNNING: Action.START\n(new game)
    GAME_OVER --> RUNNING: Action.START\n(new game)
```

### Подсостояние “мяч не выпущен”

Внутри состояния `RUNNING` есть дополнительный флаг `ball_released`:

- **`ball_released = false`**: мяч “приклеен” к платформе и следует за ней при `LEFT/RIGHT`.
- **`ball_released = true`**: физика мяча обновляется на каждом `update_current_state()`.

Переход `ball_released: false -> true` происходит при:
- `Action.ACTION` или `Action.UP` (в состоянии `RUNNING`).

