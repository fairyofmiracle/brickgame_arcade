#!/usr/bin/env bash
set -euo pipefail

# Build BrickGame legacy WASM module (Tetris C + Snake C++).
#
# Requirements:
# - Emscripten SDK installed and `emcc` available in PATH
#
# Output:
# - ../static/wasm/brickgame.js
# - ../static/wasm/brickgame.wasm

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
WEB_STATIC="$ROOT_DIR/src/brick_game/gui/web/static"
OUT_DIR="$WEB_STATIC/wasm"

mkdir -p "$OUT_DIR"

emcc \
  -DBRICKGAME_WASM=1 \
  -O2 \
  -sWASM=1 \
  -sMODULARIZE=1 \
  -sEXPORT_NAME='createBrickgameModule' \
  -sENVIRONMENT='web' \
  -sEXPORTED_FUNCTIONS='[
    "_bg_init",
    "_bg_input",
    "_bg_tick",
    "_bg_field_ptr",
    "_bg_colors_ptr",
    "_bg_next_ptr",
    "_bg_next_size",
    "_bg_meta_ptr"
  ]' \
  -sEXPORTED_RUNTIME_METHODS='["cwrap","getValue"]' \
  "$ROOT_DIR/src/brick_game/gui/web/wasm/brickgame_wasm.cpp" \
  "$ROOT_DIR/src/native_bridge/tetris_bridge.c" \
  "$ROOT_DIR/src/native_bridge/snake_bridge.cpp" \
  "$ROOT_DIR/src/brick_game/tetris/tetris.c" \
  "$ROOT_DIR/src/brick_game/snake/snake.cpp" \
  -o "$OUT_DIR/brickgame.js"

echo "WASM built into: $OUT_DIR"

