#ifndef SNAKE_H
#define SNAKE_H

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <deque>

#include "../brick_game.h"
// ncurses UI is not used in WebAssembly builds
#ifndef BRICKGAME_WASM
#include "../gui/cli/interface.h"
#endif

#define SNAKE_PART 1
#define APPLE_PART 2

void init_snake(GameInfo_t &game);
void playSnake(GameInfo_t &game);
void render_snake(GameInfo_t &game);
void update_snake(GameInfo_t &game);
void process_input(GameInfo_t &game, int ch);

#endif
