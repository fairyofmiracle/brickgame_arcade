#include "interface.h"

GameSelector::GameSelector(QWidget *parent) : QWidget(parent) {
  setWindowTitle("Выбор игры");
  QVBoxLayout *layout = new QVBoxLayout(this);

  QLabel *label = new QLabel("Выберите игру:", this);
  layout->addWidget(label);

  QPushButton *snakeButton = new QPushButton("Змейка", this);
  QPushButton *tetrisButton = new QPushButton("Тетрис", this);
  QPushButton *exitButton = new QPushButton("Выход", this);

  layout->addWidget(snakeButton);
  layout->addWidget(tetrisButton);
  layout->addWidget(exitButton);

  connect(snakeButton, &QPushButton::clicked, this,
          &GameSelector::startSnakeGame);
  connect(tetrisButton, &QPushButton::clicked, this,
          &GameSelector::startTetrisGame);
  connect(exitButton, &QPushButton::clicked, this, &QWidget::close);
}

void GameSelector::startSnakeGame() {
  SnakeGameInfo_t game;
  init_snake(game);  // Инициализация игры Змейка

  // Запуск игрового цикла Змейки
  playSnake();  // Убедитесь, что эта функция доступна и корректно работает
}

void GameSelector::startTetrisGame() {
  TetrisWidget *tetrisWidget =
      new TetrisWidget(this);  // Создание виджета Тетриса
  tetrisWidget->show();        // Показать виджет Тетриса

  connect(
      tetrisWidget, &TetrisWidget::gameOver, this,
      &GameSelector::showGameSelector);  // Подключение сигнала завершения игры
}
