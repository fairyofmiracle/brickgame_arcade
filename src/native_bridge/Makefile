CC   = gcc
CXX  = g++
# Ищем заголовки как в текущей папке (QString), так и в ../native_bridge
CFLAGS   = -Wall -Wextra -O2 -fPIC -I. -I../native_bridge
CXXFLAGS = $(CFLAGS)
LDFLAGS  = -shared
LDLIBS   = -lncurses

TETRIS_SRC = ../brick_game/tetris/tetris.c tetris_bridge.c
SNAKE_SRC  = ../brick_game/snake/snake.cpp snake_bridge.cpp
GUI_CLI_SRC = ../brick_game/gui/cli/interface.c

all: libbrickgames.so

libbrickgames.so: $(TETRIS_SRC) $(SNAKE_SRC)
	$(CXX) $(CXXFLAGS) -x c++ -c ../brick_game/tetris/tetris.c -o tetris.o
	$(CXX) $(CXXFLAGS) -x c++ -c tetris_bridge.c -o tetris_bridge.o
	$(CXX) $(CXXFLAGS) -x c++ -c ../brick_game/gui/cli/interface.c -o interface.o
	$(CXX) $(CXXFLAGS) -c ../brick_game/snake/snake.cpp -o snake.o
	$(CXX) $(CXXFLAGS) -c snake_bridge.cpp -o snake_bridge.o
	$(CXX) tetris.o tetris_bridge.o interface.o snake.o snake_bridge.o -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f libbrickgames.so *.o
