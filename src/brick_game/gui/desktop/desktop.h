#ifndef DESKTOP_H
#define DESKTOP_H

#ifdef scroll
#undef scroll
#endif

#ifdef timeout
#undef timeout
#endif

#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <fstream>
#include <vector>

#include "../../brick_game.h"
#include "../cli/interface.h"
#include "game_structures.h"

#define FIELD_HEIGHT 20
#define FIELD_WIDTH 10
#define BLOCK_SIZE 30
#define NUM_FIGURES 7
#define SCORE_PER_LEVEL 600
#define DELAY_MS 1100

// Класс для игры Тетрис
class TetrisWidget : public QWidget {
  Q_OBJECT

 public:
  TetrisWidget(QWidget *parent = nullptr);
  ~TetrisWidget();

 protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private slots:
  void render();
  void updateGame();

 private:
  DesktopGameState_t *gameState;
  QTimer *timer;
  int score;  // Объявление переменной score
  bool isPaused;
  bool gameOver;

  void initGame();
  void spawnNewFigure();
  bool canMoveDown();
  void moveDown();
  void attachFigure();
  int checkCompletedLines();
  void updateScoreAndLevel(int completedLines);
  bool isGameOver();
  int **createFigure(int &size);
  bool canMoveLeft();
  bool canMoveRight();
  bool canRotate(int **figure, int size);
  void rotateFigure();
  void saveHighScore(int highScore);
  void drawGameInfo(QPainter &painter);
};

// Класс для игры Змейка
class SnakeWidget : public QWidget {
  Q_OBJECT

 public:
  SnakeWidget(QWidget *parent = nullptr);
  ~SnakeWidget();

 protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private slots:
  void render();
  void updateGame();

 private:
  void initGame();
  void generateFood();
  void moveSnake();
  void checkCollision();
  void drawGameInfo(QPainter &painter);

  std::deque<SnakePart> snake;  // Изменяем тип на SnakePart
  Point food;                   // Изменяем тип на Point
  Direction currentDirection;
  QTimer *timer;
  int score;
  bool gameOver;
  bool isPaused;
  bool gameStarted;
};

// Класс для игры Арканоид
class ArcanoidWidget : public QWidget {
  Q_OBJECT

 public:
  ArcanoidWidget(QWidget *parent = nullptr);
  ~ArcanoidWidget();

 protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private slots:
  void updateGame();

 private:
  void initGame();
  void resetRound();
  void stepBall();
  void loadHighScore();
  void saveHighScore();
  void drawGameInfo(QPainter &painter);

  QTimer *timer;
  std::vector<std::vector<bool>> bricks;
  int paddleCol;
  int ballRow;
  int ballCol;
  int ballDr;
  int ballDc;
  bool ballReleased;

  int score;
  int highScore;
  int level;
  int speed;
  bool isPaused;
  bool gameOver;
  bool win;
};

// Класс выбора игры
class GameSelector : public QWidget {
  Q_OBJECT

 public:
  GameSelector(QWidget *parent = nullptr);
  ~GameSelector();

 private slots:
  void startTetrisGame();
  void startSnakeGame();
  void startRaceGame();
  void startArcanoidGame();

 private:
  QPushButton *tetrisButton;
  QPushButton *snakeButton;
  QPushButton *raceButton;
  QPushButton *arcanoidButton;
};

#endif  // DESKTOP_H
