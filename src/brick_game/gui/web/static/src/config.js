export const GAME_BOARD_WIDTH = 10;
export const GAME_BOARD_HEIGHT = 20;

export const rootStyles = {
    '--tile-size': '20px',
    // ближе к "консольному" экрану: тёмный фон + яркий активный цвет
    '--tile-color': '#0f0f0f',
    '--tile-active-color': '#00e5ff',
    '--game-board-width': GAME_BOARD_WIDTH,
    '--game-board-height': GAME_BOARD_HEIGHT,
    '--game-board-gap': '2px',
    '--game-board-background': '#333',
};

export const keyCodes = {
    up: ['ArrowUp', 'KeyW', 'KeyI'],
    right: ['ArrowRight', 'KeyD', 'KeyL'],
    down: ['ArrowDown', 'KeyS', 'KeyK'],
    left: ['ArrowLeft', 'KeyA', 'KeyJ'],
};
