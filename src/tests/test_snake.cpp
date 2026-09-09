#include <gtest/gtest.h>
#include "test_snake.h"

TEST(SnakeGameTests, InitSnake) {
    SnakeGameInfo_t game;
    init_snake(game);
    EXPECT_EQ(game.snake.size(), 1);
    EXPECT_EQ(game.snake.front(), std::make_pair(FIELD_N / 2, FIELD_M / 2));
    EXPECT_FALSE(game.game_over);
    EXPECT_FALSE(game.paused);
}

TEST(SnakeGameTests, UpdateSnake) {
    SnakeGameInfo_t game;
    init_snake(game);
    game.dir = RIGHT;
    update_snake(game);
    EXPECT_EQ(game.snake.front(), std::make_pair(FIELD_N / 2, FIELD_M / 2 + 1));
}

TEST(SnakeGameTests, ProcessInput) {
    SnakeGameInfo_t game;
    init_snake(game);
    process_input(game, KEY_UP);
    EXPECT_EQ(game.dir, UP);
    process_input(game, 'p');
    EXPECT_TRUE(game.paused);
    process_input(game, 'p');
    EXPECT_FALSE(game.paused);
    process_input(game, 'q');
    EXPECT_TRUE(game.game_over);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
