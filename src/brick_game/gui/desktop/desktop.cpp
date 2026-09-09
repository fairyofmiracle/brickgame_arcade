#include "desktop.h"

#include <QDir>
#include <QShortcut>

#include "race_widget.h"

// Реализация конструктора GameSelector
GameSelector::GameSelector(QWidget *parent) : QWidget(parent) {
  tetrisButton = new QPushButton("Запустить Тетрис", this);
  snakeButton = new QPushButton("Запустить Змейку", this);
  raceButton = new QPushButton("Запустить Гонки", this);
  arcanoidButton = new QPushButton("Запустить Арканоид", this);

  connect(tetrisButton, &QPushButton::clicked, this,
          &GameSelector::startTetrisGame);
  connect(snakeButton, &QPushButton::clicked, this,
          &GameSelector::startSnakeGame);
  connect(raceButton, &QPushButton::clicked, this,
          &GameSelector::startRaceGame);
  connect(arcanoidButton, &QPushButton::clicked, this,
          &GameSelector::startArcanoidGame);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(tetrisButton);
  layout->addWidget(snakeButton);
  layout->addWidget(raceButton);
  layout->addWidget(arcanoidButton);

  setLayout(layout);
}

// Деструктор GameSelector
GameSelector::~GameSelector() {}

// Метод для запуска игры Тетрис
void GameSelector::startTetrisGame() {
  TetrisWidget *tetrisWidget = new TetrisWidget();
  tetrisWidget->setWindowTitle("Tetris");
  // FIELD: 10x20 при BLOCK_SIZE=30 => 300x600, плюс панель справа ~150px.
  // Делаем ширину, чтобы панель и Next были видны без растягивания.
  tetrisWidget->resize(480, 600);
  tetrisWidget->show();
  tetrisWidget->setFocus();
}

// Метод для запуска игры Змейка
void GameSelector::startSnakeGame() {
  SnakeWidget *snakeWidget = new SnakeWidget();
  snakeWidget->setWindowTitle("Snake");
  // FIELD: 300px ширина + панель справа ~150px
  snakeWidget->resize(480, 600);
  snakeWidget->show();
  snakeWidget->setFocus();
}

void GameSelector::startRaceGame() {
  RaceWidget *raceWidget = new RaceWidget(1);
  raceWidget->setWindowTitle("BrickGame Racing");
  // Поле в гонках 10x20 с BLOCK_SIZE=30 => 300x600.
  // Делаем высоту ровно под поле, как у тетриса.
  // Ширины 400 не хватает на панель 150px справа, поэтому задаём больше.
  raceWidget->resize(500, 600);
  raceWidget->show();
  raceWidget->setFocus();
}

void GameSelector::startArcanoidGame() {
  ArcanoidWidget *w = new ArcanoidWidget();
  w->setWindowTitle("Arcanoid");
  w->resize(480, 600);
  w->show();
  w->setFocus();
}

// Реализация TetrisWidget
TetrisWidget::TetrisWidget(QWidget *parent) : QWidget(parent) {
  score = 0;
  gameState = new DesktopGameState_t();
  gameOver = false;
  initGame();
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &TetrisWidget::updateGame);
  timer->start(1000);  // Запуск таймера
  setFocusPolicy(Qt::StrongFocus);
  QTimer::singleShot(0, this, [this]() { this->setFocus(); });

  // Устойчивое управление даже если фокус иногда "уходит"
  auto *pauseSc = new QShortcut(QKeySequence(Qt::Key_P), this);
  connect(pauseSc, &QShortcut::activated, this, [this]() {
    isPaused = !isPaused;
    if (isPaused)
      timer->stop();
    else if (!gameOver)
      timer->start(1000);
    update();
  });

  auto *restartSc = new QShortcut(QKeySequence(Qt::Key_R), this);
  connect(restartSc, &QShortcut::activated, this, [this]() {
    initGame();
    gameOver = false;
    isPaused = false;
    timer->start(1000);
    update();
  });

  auto *menuSc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(menuSc, &QShortcut::activated, this, [this]() { close(); });
}

TetrisWidget::~TetrisWidget() {
  for (int i = 0; i < FIELD_HEIGHT; ++i) {
    delete[] gameState->field[i];
  }
  delete[] gameState->field;
  delete gameState;  // Освобождаем память для состояния игры
}

// Инициализация игры Тетрис
void TetrisWidget::initGame() {
  QDir().mkpath("data");
  gameState->high_score = 0;
  std::ifstream hin("data/high_score.txt");
  if (hin.is_open()) {
    int h = 0;
    if (hin >> h) {
      gameState->high_score = h;
    }
    hin.close();
  }
  gameState->score = 0;
  gameState->level = 1;
  gameState->speed = 1000;
  gameState->pause = false;
  gameState->is_playing = true;
  gameState->win = false;
  gameOver = false;
  isPaused = false;

  // подготовим "следующую" фигуру для превью
  gameState->next_figure = nullptr;
  gameState->next_figure_size = 0;

  gameState->field = new int *[FIELD_HEIGHT];
  for (int i = 0; i < FIELD_HEIGHT; ++i) {
    gameState->field[i] = new int[FIELD_WIDTH];
    std::fill(gameState->field[i], gameState->field[i] + FIELD_WIDTH,
              0);  // Заполнение поля нулями
  }

  // создаём первую следующую фигуру, а текущую получим через spawnNewFigure()
  gameState->next_figure = createFigure(gameState->next_figure_size);
  spawnNewFigure();  // Создание текущей фигуры (из next) и новой next
}

// Метод для создания новой фигуры
void TetrisWidget::spawnNewFigure() {
  // Текущая фигура становится "Next", затем генерируем новую "Next"
  if (gameState->next_figure != nullptr && gameState->next_figure_size > 0) {
    gameState->figure = gameState->next_figure;
    gameState->figure_size = gameState->next_figure_size;
  } else {
    // первый запуск/страховка
    gameState->figure = createFigure(gameState->figure_size);
  }

  // создаём следующую фигуру
  gameState->next_figure = createFigure(gameState->next_figure_size);

  gameState->x = (gameState->figure_size == 2) ? 4 : 3;
  gameState->y = 0;
}

// Привязка фигуры к игровому полю
void TetrisWidget::attachFigure() {
  for (int i = 0; i < gameState->figure_size; i++) {
    for (int j = 0; j < gameState->figure_size; j++) {
      if (gameState->figure[i][j] == 1) {
        int x = gameState->x + j;
        int y = gameState->y + i;
        gameState->field[y][x] = 1;
      }
    }
  }
}

// Проверка и удаление заполненных линий
int TetrisWidget::checkCompletedLines() {
  int completedLines = 0;
  for (int y = FIELD_HEIGHT - 1; y >= 0; y--) {
    bool isLineFull = true;
    for (int x = 0; x < FIELD_WIDTH; x++) {
      if (gameState->field[y][x] == 0) {
        isLineFull = false;
        break;
      }
    }
    if (isLineFull) {
      for (int i = y; i > 0; i--) {
        for (int x = 0; x < FIELD_WIDTH; x++) {
          gameState->field[i][x] = gameState->field[i - 1][x];
        }
      }
      for (int x = 0; x < FIELD_WIDTH; x++) {
        gameState->field[0][x] = 0;
      }
      completedLines++;
      y++;  // Повторная проверка текущей линии
    }
  }
  updateScoreAndLevel(completedLines);
  return completedLines;
}

// Обновление счета и уровня
void TetrisWidget::updateScoreAndLevel(int completedLines) {
  switch (completedLines) {
    case 1:
      gameState->score += 100;
      break;
    case 2:
      gameState->score += 300;
      break;
    case 3:
      gameState->score += 700;
      break;
    case 4:
      gameState->score += 1500;
      break;
    default:
      break;
  }

  if (gameState->score >= SCORE_PER_LEVEL) {
    gameState->level++;
    gameState->speed = DELAY_MS * pow(0.8, gameState->level);
  }

  if (gameState->score > gameState->high_score) {
    gameState->high_score = gameState->score;
    saveHighScore(gameState->high_score);
  }
}

// Проверка на конец игры
bool TetrisWidget::isGameOver() {
  for (int x = 0; x < FIELD_WIDTH; x++) {
    if (gameState->field[0][x] == 1) {
      return true;
    }
  }
  return false;
}

int **TetrisWidget::createFigure(int &size) {
  int randomType = rand() % NUM_FIGURES;  // Генерация случайного типа фигуры
  size = (randomType == 0)   ? 4
         : (randomType == 3) ? 2
                             : 3;  // Определение размера фигуры
  int **figure = new int *[size];
  for (int i = 0; i < size; i++) {
    figure[i] = new int[size];
    std::fill(figure[i], figure[i] + size, 0);  // Заполнение фигуры нулями
  }
  switch (randomType) {
    case 0:  // Прямоугольник
      figure[0][0] = figure[0][1] = figure[0][2] = figure[0][3] = 1;
      break;
    case 1:  // L-образная фигура
      figure[0][0] = figure[1][0] = figure[1][1] = figure[1][2] = 1;
      break;
    case 2:  // Z-образная фигура
      figure[0][2] = 1;
      figure[1][0] = figure[1][1] = figure[1][2] = 1;
      break;
    case 3:  // Квадрат
      figure[0][0] = figure[0][1] = figure[1][0] = figure[1][1] = 1;
      break;
    case 4:  // T-образная фигура
      figure[0][1] = figure[0][2] = 1;
      figure[1][0] = figure[1][1] = 1;
      break;
    case 5:  // L-образная фигура (обратная)
      figure[0][1] = 1;
      figure[1][0] = figure[1][1] = figure[1][2] = 1;
      break;
    case 6:  // S-образная фигура
      figure[0][0] = figure[0][1] = 1;
      figure[1][1] = figure[1][2] = 1;
      break;
    default:
      break;
  }
  return figure;
}

bool TetrisWidget::canMoveLeft() {
  for (int i = 0; i < gameState->figure_size; i++) {
    for (int j = 0; j < gameState->figure_size; j++) {
      if (gameState->figure[i][j] == 1) {
        int x = gameState->x + j - 1;
        int y = gameState->y + i;
        if (x < 0 || gameState->field[y][x] == 1) {
          return false;
        }
      }
    }
  }
  return true;
}

bool TetrisWidget::canMoveRight() {
  for (int i = 0; i < gameState->figure_size; i++) {
    for (int j = 0; j < gameState->figure_size; j++) {
      if (gameState->figure[i][j] == 1) {
        int x = gameState->x + j + 1;
        int y = gameState->y + i;
        if (x >= FIELD_WIDTH || gameState->field[y][x] == 1) {
          return false;
        }
      }
    }
  }
  return true;
}

bool TetrisWidget::canRotate(int **figure, int size) {
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      if (figure[i][j] == 1) {
        int x = gameState->x + j;
        int y = gameState->y + i;
        if (x < 0 || x >= FIELD_WIDTH || y < 0 || y >= FIELD_HEIGHT ||
            gameState->field[y][x] == 1) {
          return false;
        }
      }
    }
  }
  return true;
}

void TetrisWidget::rotateFigure() {
  int size = gameState->figure_size;
  int **temp = new int *[size];
  for (int i = 0; i < size; i++) {
    temp[i] = new int[size];
    std::fill(temp[i], temp[i] + size, 0);
  }

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      temp[j][size - 1 - i] = gameState->figure[i][j];
    }
  }

  if (canRotate(temp, size)) {
    for (int i = 0; i < size; i++) {
      std::copy(temp[i], temp[i] + size, gameState->figure[i]);
    }
  }

  for (int i = 0; i < size; i++) {
    delete[] temp[i];
  }
  delete[] temp;
}

// Сохранение рекорда в файл
void TetrisWidget::saveHighScore(int highScore) {
  QDir().mkpath("data");
  std::ofstream file("data/high_score.txt", std::ios::out | std::ios::trunc);
  if (file.is_open()) {
    file << highScore;
    file.close();
  }
}

void TetrisWidget::updateGame() {
  if (isPaused || gameOver) {
    render();  // перерисовать оверлей (pause/gameover)
    return;
  }

  if (canMoveDown()) {
    moveDown();
  } else {
    attachFigure();
    checkCompletedLines();
    spawnNewFigure();
    if (isGameOver()) {
      gameOver = true;
      timer->stop();
      render();
      return;
    }
  }
  render();
}

void TetrisWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setBrush(Qt::black);
  painter.drawRect(0, 0, width(), height());

  // Отрисовка игрового поля
  for (int i = 0; i < FIELD_HEIGHT; ++i) {
    for (int j = 0; j < FIELD_WIDTH; ++j) {
      QRect cell(j * BLOCK_SIZE, i * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);
      QRect inner =
          cell.adjusted(1, 1, -1, -1);  // тонкие линии сетки между клетками
      painter.setBrush(gameState->field[i][j] == 1 ? Qt::red : Qt::white);
      painter.drawRect(inner);
    }
  }

  // Отрисовка текущей фигуры
  for (int i = 0; i < gameState->figure_size; i++) {
    for (int j = 0; j < gameState->figure_size; j++) {
      if (gameState->figure[i][j] == 1) {
        int x = gameState->x + j;
        int y = gameState->y + i;
        if (y >= 0) {
          painter.setBrush(Qt::blue);
          QRect cell(x * BLOCK_SIZE, y * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);
          QRect inner = cell.adjusted(1, 1, -1, -1);
          painter.drawRect(inner);
        }
      }
    }
  }

  drawGameInfo(painter);

  // Оверлей паузы/проигрыша (центр как в остальных виджетах)
  const int boardW = FIELD_WIDTH * BLOCK_SIZE;
  const int boardH = FIELD_HEIGHT * BLOCK_SIZE;

  if (isPaused && !gameOver) {
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, 0, boardW, boardH, QColor(0, 0, 0, 120));
    painter.setPen(Qt::yellow);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(QRect(0, 0, boardW, boardH), Qt::AlignCenter, "PAUSED");
  }

  if (gameOver) {
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, 0, boardW, boardH, QColor(0, 0, 0, 160));
    painter.setPen(Qt::red);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(QRect(0, 0, boardW, boardH), Qt::AlignCenter, "GAME OVER");

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Normal));
    painter.drawText(QRect(0, 0, boardW, boardH),
                     Qt::AlignHCenter | Qt::AlignTop,
                     QString("Score: %1").arg(gameState->score));
    painter.drawText(QRect(0, 0, boardW, boardH),
                     Qt::AlignHCenter | Qt::AlignBottom, "Press R to restart");
  }
}

void TetrisWidget::drawGameInfo(QPainter &painter) {
  painter.setPen(Qt::white);
  painter.setFont(QFont("Courier", 10));

  const int boardW = FIELD_WIDTH * BLOCK_SIZE;  // 300
  const int panelX = boardW + 10;
  const int panelW = 150;  // фикс: не масштабируем Next при растягивании окна

  if (width() < panelX + panelW) {
    return;
  }

  const int topMargin = 10;
  const int lineHeight = painter.fontMetrics().height();

  // Next (как в web): всегда 4x4 с центрированием внутри
  const int boxSize = 110;  // фикс: не масштабируем
  const int boxX = panelX + (panelW - boxSize) / 2;
  const int boxY = topMargin + lineHeight * 2;

  painter.setPen(QColor(120, 120, 120));
  painter.drawText(QRect(panelX, topMargin, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop, "Next");
  painter.drawRect(boxX, boxY, boxSize, boxSize);

  bool occ[4][4] = {{false}};
  int nextSize = gameState->next_figure_size;
  if (gameState->next_figure && nextSize > 0) {
    for (int r = 0; r < 4 && r < nextSize; ++r) {
      for (int c = 0; c < 4 && c < nextSize; ++c) {
        occ[r][c] = (gameState->next_figure[r][c] == 1);
      }
    }
  }

  int minR = 4, maxR = -1, minC = 4, maxC = -1;
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      if (occ[r][c]) {
        minR = std::min(minR, r);
        maxR = std::max(maxR, r);
        minC = std::min(minC, c);
        maxC = std::max(maxC, c);
      }
    }
  }

  bool centered[4][4] = {{false}};
  if (maxR >= 0) {
    const int h = maxR - minR + 1;
    const int w = maxC - minC + 1;
    const int offR = (4 - h) / 2;
    const int offC = (4 - w) / 2;
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        if (!occ[r][c]) continue;
        const int nr = r - minR + offR;
        const int nc = c - minC + offC;
        if (nr >= 0 && nr < 4 && nc >= 0 && nc < 4) {
          centered[nr][nc] = true;
        }
      }
    }
  }

  const int cellSize = boxSize / 4;
  painter.setPen(Qt::white);
  painter.setBrush(Qt::black);
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      QRect cell(boxX + c * cellSize, boxY + r * cellSize, cellSize, cellSize);
      if (centered[r][c]) {
        painter.setBrush(Qt::blue);
      } else {
        painter.setBrush(Qt::black);
      }
      painter.drawRect(cell);
    }
  }

  // Разделитель как в web после Next
  const int hr1Y = boxY + boxSize + 8;
  painter.setPen(QColor(120, 120, 120));
  painter.drawLine(panelX, hr1Y, panelX + panelW, hr1Y);

  // Score / Level / Speed
  painter.setPen(Qt::white);
  painter.setFont(QFont("Courier", 10));
  painter.drawText(QRect(panelX, hr1Y + lineHeight * 1, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString("Счёт %1").arg(gameState->score));
  painter.drawText(QRect(panelX, hr1Y + lineHeight * 2, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString("Уровень %1").arg(gameState->level));
  painter.drawText(QRect(panelX, hr1Y + lineHeight * 3, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString("Скорость %1").arg(gameState->speed));

  // Разделитель как в web после Speed
  const int hr2Y = hr1Y + lineHeight * 4 + 8;
  painter.setPen(QColor(120, 120, 120));
  painter.drawLine(panelX, hr2Y, panelX + panelW, hr2Y);

  // High score
  painter.setPen(Qt::white);
  painter.drawText(QRect(panelX, hr2Y + lineHeight * 1, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString("Рекорд %1").arg(gameState->high_score));
}

SnakeWidget::SnakeWidget(QWidget *parent) : QWidget(parent) {
  score = 0;
  gameOver = false;
  isPaused = false;
  gameStarted = false;
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &SnakeWidget::updateGame);
  initGame();
  setFocusPolicy(Qt::StrongFocus);
  QTimer::singleShot(0, this, [this]() { this->setFocus(); });

  auto *pauseSc = new QShortcut(QKeySequence(Qt::Key_P), this);
  connect(pauseSc, &QShortcut::activated, this, [this]() {
    isPaused = !isPaused;
    if (isPaused)
      timer->stop();
    else if (gameStarted && !gameOver)
      timer->start(100);
    update();
  });

  auto *restartSc = new QShortcut(QKeySequence(Qt::Key_R), this);
  connect(restartSc, &QShortcut::activated, this, [this]() {
    initGame();
    timer->stop();
    isPaused = false;
    gameOver = false;
    gameStarted = false;
    update();
  });

  auto *menuSc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(menuSc, &QShortcut::activated, this, [this]() { close(); });
}

SnakeWidget::~SnakeWidget() {}

void SnakeWidget::initGame() {
  snake.clear();
  gameOver = false;
  isPaused = false;
  gameStarted = false;
  snake.push_front({FIELD_WIDTH / 2, FIELD_HEIGHT / 2});
  currentDirection = Direction::RIGHT;
  score = 0;
  generateFood();
}

void SnakeWidget::generateFood() {
  bool foodInSnake;
  do {
    food = {rand() % FIELD_WIDTH, rand() % FIELD_HEIGHT};
    foodInSnake = false;
    for (const auto &segment : snake) {
      if (segment.x == food.x && segment.y == food.y) {
        foodInSnake = true;
        break;
      }
    }
  } while (foodInSnake);
}

void SnakeWidget::updateGame() {
  if (!gameOver && !isPaused) {
    moveSnake();
    checkCollision();
    update();
  }
}

void SnakeWidget::checkCollision() {
  const SnakePart &head = snake.front();
  if (head.x < 0 || head.x >= FIELD_WIDTH || head.y < 0 ||
      head.y >= FIELD_HEIGHT) {
    gameOver = true;
    timer->stop();
  }
  for (std::deque<SnakePart>::size_type i = 1; i < snake.size(); i++) {
    if (snake[i].x == head.x && snake[i].y == head.y) {
      gameOver = true;
      timer->stop();
      break;
    }
  }
}

void SnakeWidget::moveSnake() {
  SnakePart head = snake.front();
  switch (currentDirection) {
    case Direction::UP:
      head.y--;
      break;
    case Direction::DOWN:
      head.y++;
      break;
    case Direction::LEFT:
      head.x--;
      break;
    case Direction::RIGHT:
      head.x++;
      break;
  }

  snake.push_front(head);

  if (head.x == food.x && head.y == food.y) {
    score++;
    generateFood();
    // Level mechanic: +1 per 5 points, max 10. Increase speed with level.
    const int level = std::min(10, 1 + score / 5);
    const int intervalMs = std::max(40, 220 - (level - 1) * 18);
    if (gameStarted && !isPaused && !gameOver) {
      timer->start(intervalMs);
    }
  } else {
    snake.pop_back();
  }
}

void SnakeWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);

  painter.setBrush(Qt::black);
  painter.drawRect(0, 0, width(), height());

  for (int i = 0; i < FIELD_HEIGHT; ++i) {
    for (int j = 0; j < FIELD_WIDTH; ++j) {
      bool isSnakeSegment = false;
      for (const auto &segment : snake) {
        if (segment.x == j && segment.y == i) {
          isSnakeSegment = true;
          break;
        }
      }

      if (isSnakeSegment) {
        painter.setBrush(Qt::green);
      } else {
        painter.setBrush(Qt::white);
      }
      {
        QRect cell(j * BLOCK_SIZE, i * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);
        QRect inner =
            cell.adjusted(1, 1, -1, -1);  // линии сетки как у RaceWidget
        painter.drawRect(inner);
      }
    }
  }

  painter.setBrush(Qt::red);
  {
    QRect cell(food.x * BLOCK_SIZE, food.y * BLOCK_SIZE, BLOCK_SIZE,
               BLOCK_SIZE);
    QRect inner = cell.adjusted(1, 1, -1, -1);
    painter.drawRect(inner);
  }

  drawGameInfo(painter);

  // Центрированный оверлей GAME OVER / PAUSED
  const int boardW = FIELD_WIDTH * BLOCK_SIZE;
  const int boardH = FIELD_HEIGHT * BLOCK_SIZE;

  if (isPaused && !gameOver) {
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, 0, boardW, boardH, QColor(0, 0, 0, 120));
    painter.setPen(Qt::yellow);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(QRect(0, 0, boardW, boardH), Qt::AlignCenter, "PAUSED");
  }

  if (gameOver) {
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, 0, boardW, boardH, QColor(0, 0, 0, 160));
    painter.setPen(Qt::red);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(QRect(0, 0, boardW, boardH), Qt::AlignCenter, "GAME OVER");

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Normal));
    painter.drawText(QRect(0, 0, boardW, boardH),
                     Qt::AlignHCenter | Qt::AlignTop,
                     QString("Score: %1").arg(score));
    painter.drawText(QRect(0, 0, boardW, boardH),
                     Qt::AlignHCenter | Qt::AlignBottom, "Press R to restart");
  }
}

void SnakeWidget::drawGameInfo(QPainter &painter) {
  painter.setPen(Qt::white);
  painter.setFont(QFont("Courier", 10));

  const int boardW = FIELD_WIDTH * BLOCK_SIZE;  // поле 300
  const int panelX = boardW + 10;
  const int panelW = 150;  // фикс: не разъезжается при ресайзе

  if (width() < panelX + panelW) {
    return;
  }

  const int topMargin = 10;
  const int lineHeight = painter.fontMetrics().height();

  // как в web: счёт + hr + рекорд
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 0, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, QString("Счёт %1").arg(score));

  const int hrY = topMargin + lineHeight * 1 + 6;
  painter.setPen(QColor(120, 120, 120));
  painter.drawLine(panelX, hrY, panelX + panelW, hrY);
  painter.setPen(Qt::white);

  const int level = std::min(10, 1 + score / 5);
  const int speed = level;
  painter.drawText(QRect(panelX, hrY + lineHeight * 1, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString("Уровень %1").arg(level));
  painter.drawText(QRect(panelX, hrY + lineHeight * 2, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop,
                   QString("Скорость %1").arg(speed));
  painter.drawText(QRect(panelX, hrY + lineHeight * 3, panelW, lineHeight),
                   Qt::AlignLeft | Qt::AlignTop, "Рекорд 0");
}

void TetrisWidget::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
    case Qt::Key_Left:
      if (canMoveLeft()) {
        gameState->x--;
      }
      break;
    case Qt::Key_Right:
      if (canMoveRight()) {
        gameState->x++;
      }
      break;
    case Qt::Key_Down:
      moveDown();
      break;
    case Qt::Key_Space:
      rotateFigure();
      break;
    case Qt::Key_P:
      isPaused = !isPaused;
      if (isPaused)
        timer->stop();
      else if (!gameOver)
        timer->start(1000);
      break;
    case Qt::Key_R:
      initGame();
      gameOver = false;
      isPaused = false;
      timer->start(1000);
      break;
  }
  update();
}

void SnakeWidget::keyPressEvent(QKeyEvent *event) {
  const int k = event->key();
  const bool isArrow = (k == Qt::Key_Up || k == Qt::Key_Down ||
                        k == Qt::Key_Left || k == Qt::Key_Right);

  if (!gameStarted && isArrow) {
    // Не начинаем движение мгновенно: ждём как по веб-таймеру.
    gameStarted = true;
    isPaused = false;
    timer->stop();

    QTimer::singleShot(200, this, [this]() {
      if (gameStarted && !gameOver && !isPaused && timer) {
        timer->start(100);
      }
    });
  }

  switch (k) {
    case Qt::Key_P:
      isPaused = !isPaused;
      if (isPaused) {
        timer->stop();
      } else if (gameStarted && !gameOver) {
        timer->start(100);
      }
      update();
      break;
    case Qt::Key_R:
      if (gameOver) {
        initGame();
        // После рестарта тоже ждём первого движения (как "пока я похожу").
        update();
      }
      break;
    case Qt::Key_Up:
      if (currentDirection != Direction::DOWN) currentDirection = Direction::UP;
      break;
    case Qt::Key_Down:
      if (currentDirection != Direction::UP) currentDirection = Direction::DOWN;
      break;
    case Qt::Key_Left:
      if (currentDirection != Direction::RIGHT)
        currentDirection = Direction::LEFT;
      break;
    case Qt::Key_Right:
      if (currentDirection != Direction::LEFT)
        currentDirection = Direction::RIGHT;
      break;
    default:
      break;
  }
}

void TetrisWidget::render() { update(); }

void SnakeWidget::render() { update(); }

bool TetrisWidget::canMoveDown() {
  for (int i = 0; i < gameState->figure_size; i++) {
    for (int j = 0; j < gameState->figure_size; j++) {
      if (gameState->figure[i][j] == 1) {
        int x = gameState->x + j;
        int y = gameState->y + i + 1;
        if (y >= FIELD_HEIGHT || gameState->field[y][x] == 1) {
          return false;
        }
      }
    }
  }
  return true;
}

void TetrisWidget::moveDown() {
  if (canMoveDown()) {
    gameState->y++;
  }
}

// ===================== ArcanoidWidget =====================

namespace {
constexpr int ARC_PADDLE_WIDTH = 3;
constexpr int ARC_BRICKS_TOP = 2;
constexpr int ARC_BRICKS_ROWS = 4;
constexpr int ARC_POINTS_PER_LEVEL = 5;
constexpr int ARC_MAX_LEVEL = 10;

static int arcClamp(int v, int lo, int hi) {
  return std::max(lo, std::min(hi, v));
}
}  // namespace

ArcanoidWidget::ArcanoidWidget(QWidget *parent) : QWidget(parent) {
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &ArcanoidWidget::updateGame);
  initGame();

  setFocusPolicy(Qt::StrongFocus);
  QTimer::singleShot(0, this, [this]() { this->setFocus(); });
  timer->start(100);

  auto *pauseSc = new QShortcut(QKeySequence(Qt::Key_P), this);
  connect(pauseSc, &QShortcut::activated, this, [this]() {
    isPaused = !isPaused;
    update();
  });

  auto *restartSc = new QShortcut(QKeySequence(Qt::Key_R), this);
  connect(restartSc, &QShortcut::activated, this, [this]() {
    initGame();
    update();
  });

  auto *menuSc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(menuSc, &QShortcut::activated, this, [this]() { close(); });
}

ArcanoidWidget::~ArcanoidWidget() {}

void ArcanoidWidget::loadHighScore() {
  QDir().mkpath("data");
  highScore = 0;
  std::ifstream in("data/high_score_arcanoid.txt");
  if (in.is_open()) {
    int h = 0;
    if (in >> h) highScore = h;
    in.close();
  }
}

void ArcanoidWidget::saveHighScore() {
  QDir().mkpath("data");
  std::ofstream out("data/high_score_arcanoid.txt",
                    std::ios::out | std::ios::trunc);
  if (out.is_open()) {
    out << highScore;
    out.close();
  }
}

void ArcanoidWidget::initGame() {
  loadHighScore();
  resetRound();
  isPaused = false;
  gameOver = false;
  win = false;
}

void ArcanoidWidget::resetRound() {
  score = 0;
  level = 1;
  speed = 1;

  paddleCol = FIELD_WIDTH / 2;
  ballRow = FIELD_HEIGHT - 2;
  ballCol = paddleCol;
  ballDr = -1;
  ballDc = 1;
  ballReleased = false;

  bricks.assign(FIELD_HEIGHT, std::vector<bool>(FIELD_WIDTH, false));
  const int endRow =
      std::min(FIELD_HEIGHT - 3, ARC_BRICKS_TOP + ARC_BRICKS_ROWS);
  for (int r = ARC_BRICKS_TOP; r < endRow; ++r) {
    for (int c = 0; c < FIELD_WIDTH; ++c) bricks[r][c] = true;
  }
}

void ArcanoidWidget::stepBall() {
  if (!ballReleased) {
    ballCol = paddleCol;
    ballRow = FIELD_HEIGHT - 2;
    return;
  }

  int nr = ballRow + ballDr;
  int nc = ballCol + ballDc;

  if (nc < 0 || nc >= FIELD_WIDTH) {
    ballDc *= -1;
    nc = ballCol + ballDc;
  }
  if (nr < 0) {
    ballDr *= -1;
    nr = ballRow + ballDr;
  }

  bool hitAny = false;
  const int currR = ballRow;
  const int currC = ballCol;

  auto hasBrick = [this](int r, int c) -> bool {
    return (r >= 0 && r < FIELD_HEIGHT && c >= 0 && c < FIELD_WIDTH &&
            bricks[r][c]);
  };
  auto removeBrick = [this](int r, int c) {
    if (r >= 0 && r < FIELD_HEIGHT && c >= 0 && c < FIELD_WIDTH)
      bricks[r][c] = false;
  };

  if (hasBrick(nr, currC)) {
    removeBrick(nr, currC);
    ballDr *= -1;
    nr = currR + ballDr;
    hitAny = true;
  }
  if (hasBrick(currR, nc)) {
    removeBrick(currR, nc);
    ballDc *= -1;
    nc = currC + ballDc;
    hitAny = true;
  }
  if (!hitAny && hasBrick(nr, nc)) {
    removeBrick(nr, nc);
    ballDr *= -1;
    nr = currR + ballDr;
    hitAny = true;
  }

  if (hitAny) {
    score += 1;
    level = std::min(ARC_MAX_LEVEL, 1 + score / ARC_POINTS_PER_LEVEL);
    speed = level;
    if (score > highScore) {
      highScore = score;
      saveHighScore();
    }
  }

  const int paddleRow = FIELD_HEIGHT - 1;
  if (nr == paddleRow) {
    const int half = ARC_PADDLE_WIDTH / 2;
    const int minC = paddleCol - half;
    const int maxC = paddleCol + half;
    const bool inside = (nc >= minC && nc <= maxC);
    const bool forgive = (nc == minC - 1 || nc == maxC + 1);
    if (inside || forgive) {
      ballDr = -1;
      if (nc < paddleCol)
        ballDc = -1;
      else if (nc > paddleCol)
        ballDc = 1;
      nc = arcClamp(nc, minC, maxC);
      nr = ballRow + ballDr;
    } else {
      gameOver = true;
      win = false;
      ballReleased = false;
      return;
    }
  }

  ballRow = arcClamp(nr, 0, FIELD_HEIGHT - 1);
  ballCol = arcClamp(nc, 0, FIELD_WIDTH - 1);

  bool anyBricks = false;
  for (int r = 0; r < FIELD_HEIGHT && !anyBricks; ++r) {
    for (int c = 0; c < FIELD_WIDTH; ++c) {
      if (bricks[r][c]) {
        anyBricks = true;
        break;
      }
    }
  }
  if (!anyBricks) {
    gameOver = true;
    win = true;
    ballReleased = false;
  }
}

void ArcanoidWidget::updateGame() {
  if (isPaused || gameOver) {
    update();
    return;
  }
  stepBall();
  const int intervalMs = std::max(40, 220 - (level - 1) * 18);
  if (timer) timer->start(intervalMs);
  update();
}

void ArcanoidWidget::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
    case Qt::Key_Left: {
      paddleCol = arcClamp(paddleCol - 1, ARC_PADDLE_WIDTH / 2,
                           FIELD_WIDTH - 1 - ARC_PADDLE_WIDTH / 2);
      if (!ballReleased) ballCol = paddleCol;
      break;
    }
    case Qt::Key_Right: {
      paddleCol = arcClamp(paddleCol + 1, ARC_PADDLE_WIDTH / 2,
                           FIELD_WIDTH - 1 - ARC_PADDLE_WIDTH / 2);
      if (!ballReleased) ballCol = paddleCol;
      break;
    }
    case Qt::Key_Up:
    case Qt::Key_Space:
      if (!isPaused && !gameOver) ballReleased = true;
      break;
    case Qt::Key_P:
      isPaused = !isPaused;
      break;
    case Qt::Key_R:
      initGame();
      break;
    default:
      break;
  }
  update();
}

void ArcanoidWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setBrush(Qt::black);
  painter.drawRect(0, 0, width(), height());

  // bricks + paddle + ball
  for (int r = 0; r < FIELD_HEIGHT; ++r) {
    for (int c = 0; c < FIELD_WIDTH; ++c) {
      QRect cell(c * BLOCK_SIZE, r * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE);
      QRect inner = cell.adjusted(1, 1, -1, -1);
      bool filled = false;
      QColor color = Qt::white;

      if (r >= 0 && r < (int)bricks.size() && c >= 0 &&
          c < (int)bricks[r].size() && bricks[r][c]) {
        filled = true;
        color = QColor(80, 160, 255);
      }

      const int paddleRow = FIELD_HEIGHT - 1;
      const int half = ARC_PADDLE_WIDTH / 2;
      if (r == paddleRow && c >= paddleCol - half && c <= paddleCol + half) {
        filled = true;
        color = QColor(80, 255, 120);
      }

      if (r == ballRow && c == ballCol) {
        filled = true;
        color = QColor(255, 230, 80);
      }

      painter.setBrush(filled ? color : Qt::white);
      painter.drawRect(inner);
    }
  }

  drawGameInfo(painter);

  const int boardW = FIELD_WIDTH * BLOCK_SIZE;
  const int boardH = FIELD_HEIGHT * BLOCK_SIZE;

  if (isPaused && !gameOver) {
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, 0, boardW, boardH, QColor(0, 0, 0, 120));
    painter.setPen(Qt::yellow);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(QRect(0, 0, boardW, boardH), Qt::AlignCenter, "PAUSED");
  }

  if (gameOver) {
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, 0, boardW, boardH, QColor(0, 0, 0, 160));
    painter.setPen(win ? Qt::green : Qt::red);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(QRect(0, 0, boardW, boardH), Qt::AlignCenter,
                     win ? "YOU WIN" : "GAME OVER");
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12, QFont::Normal));
    painter.drawText(QRect(0, 0, boardW, boardH),
                     Qt::AlignHCenter | Qt::AlignBottom, "Press R to restart");
  }
}

void ArcanoidWidget::drawGameInfo(QPainter &painter) {
  painter.setPen(Qt::white);
  painter.setFont(QFont("Courier", 10));

  const int boardW = FIELD_WIDTH * BLOCK_SIZE;
  const int panelX = boardW + 10;
  const int panelW = 150;
  if (width() < panelX + panelW) return;

  const int topMargin = 10;
  const int lineHeight = painter.fontMetrics().height();

  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 0, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, QString("Счёт %1").arg(score));
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 1, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, QString("Рекорд %1").arg(highScore));
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 3, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, QString("Уровень %1").arg(level));
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 4, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, QString("Скорость %1").arg(speed));
  painter.setPen(QColor(120, 120, 120));
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 6, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, "← → move");
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 7, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, "Space/↑ start");
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 8, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, "P pause");
  painter.drawText(
      QRect(panelX, topMargin + lineHeight * 9, panelW, lineHeight),
      Qt::AlignLeft | Qt::AlignTop, "R restart");
}
