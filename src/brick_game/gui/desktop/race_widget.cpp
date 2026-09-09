#include "race_widget.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QShortcut>
#include <cstdlib>

RaceWidget::RaceWidget(int gameId, QWidget *parent)
    : QWidget(parent), _gameId(gameId) {
  setWindowTitle("BrickGame Racing");
  setFocusPolicy(Qt::StrongFocus);
  QTimer::singleShot(0, this, [this]() { this->setFocus(); });

  _net = new QNetworkAccessManager(this);
  _pollTimer = new QTimer(this);
  connect(_pollTimer, &QTimer::timeout, this, [this]() { fetchState(); });

  setDefaultState();
  selectGame(_gameId);

  auto *pauseSc = new QShortcut(QKeySequence(Qt::Key_P), this);
  connect(pauseSc, &QShortcut::activated, this,
          [this]() { sendAction(PAUSE, false); });

  auto *restartSc = new QShortcut(QKeySequence(Qt::Key_R), this);
  connect(restartSc, &QShortcut::activated, this, [this]() { restartGame(); });

  auto *menuSc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(menuSc, &QShortcut::activated, this, [this]() {
    sendAction(TERMINATE, false);
    _pollTimer->stop();
    close();
  });
}

RaceWidget::~RaceWidget() = default;

void RaceWidget::setDefaultState() {
  _state.field.assign(RACE_FIELD_HEIGHT,
                      std::vector<bool>(RACE_FIELD_WIDTH, false));
  _state.score = 0;
  _state.highScore = 0;
  _state.level = 1;
  _state.speed = 0;
  _state.pause = false;
  _state.nitro = false;
  _state.lives = 0;
  _state.gameOver = false;
  _state.gameId = _gameId;
}

QString RaceWidget::stripTrailingSlash(QString s) {
  while (s.endsWith('/')) {
    s.chop(1);
  }
  return s;
}

QString RaceWidget::envBaseUrl() {
  // Можно переопределять, если клиенты запускаются не на том же хосте.
  // Пример: BRICKGAME_API_BASE_URL="http://localhost:8005"
  QByteArray v = qgetenv("BRICKGAME_API_BASE_URL");
  if (!v.isEmpty()) {
    return QString::fromUtf8(v);
  }
  return "http://127.0.0.1:8005";
}

QString RaceWidget::makeAbsBaseUrl() const {
  return stripTrailingSlash(_baseUrl.isEmpty() ? envBaseUrl() : _baseUrl);
}

QUrl RaceWidget::makeUrl(const QString &path) const {
  return QUrl(makeAbsBaseUrl() + path);
}

void RaceWidget::selectGame(int gameId) {
  // POST /games/{gameId} - сервер сам выполнит Action.START.
  _gameId = gameId;
  setDefaultState();
  _gameSelected = false;
  _pollTimer->stop();

  // Важно: здесь синхронно (curl) — просто ограничиваем повторные запросы
  // через `_inFlight`.
  _inFlight = true;
  const bool ok = _rest_client.select_game(_gameId);
  _inFlight = false;

  if (!ok) {
    return;
  }

  _gameSelected = true;
  _pollTimer->start(200);  // каждые ~200мс делаем /state
  fetchState();
}

void RaceWidget::restartGame() {
  // Как и в вебе: просто снова выбираем игру по /games/{id}.
  selectGame(_gameId);
}

void RaceWidget::sendAction(ActionId actionId, bool hold) {
  if (!_gameSelected) {
    return;
  }
  (void)_rest_client.send_action(static_cast<int>(actionId), hold);
}

void RaceWidget::fetchState() {
  if (!_gameSelected || _inFlight) {
    return;
  }

  _inFlight = true;
  race_rest::RaceStateDto dto{};
  const bool ok = _rest_client.fetch_state(dto);
  _inFlight = false;

  if (!ok) {
    return;
  }

  _state.score = dto.score;
  _state.highScore = dto.high_score;
  _state.level = dto.level;
  _state.speed = dto.speed;
  _state.pause = dto.pause;
  _state.nitro = dto.nitro;
  _state.lives = dto.lives;
  _state.gameOver = dto.game_over;

  // field: сервер отдаёт 20x10 bool
  _state.field.assign(RACE_FIELD_HEIGHT,
                      std::vector<bool>(RACE_FIELD_WIDTH, false));
  for (int r = 0; r < RACE_FIELD_HEIGHT; ++r) {
    for (int c = 0; c < RACE_FIELD_WIDTH; ++c) {
      _state.field[r][c] = dto.field[r][c];
    }
  }

  if (_state.gameOver) {
    _pollTimer->stop();
  }

  update();  // перерисовать
}

void RaceWidget::handleStateJson(const QJsonObject &obj) {
  // field: 20x10 bool
  QJsonArray fieldArr = obj.value("field").toArray();
  if (fieldArr.size() == RACE_FIELD_HEIGHT) {
    _state.field.assign(RACE_FIELD_HEIGHT,
                        std::vector<bool>(RACE_FIELD_WIDTH, false));
    for (int r = 0; r < RACE_FIELD_HEIGHT; ++r) {
      QJsonArray rowArr = fieldArr.at(r).toArray();
      for (int c = 0; c < RACE_FIELD_WIDTH && c < rowArr.size(); ++c) {
        _state.field[r][c] = rowArr.at(c).toBool();
      }
    }
  }

  _state.score = obj.value("score").toInt();
  _state.highScore = obj.value("high_score").toInt();
  _state.level = obj.value("level").toInt();
  _state.speed = obj.value("speed").toInt();
  _state.pause = obj.value("pause").toBool();
  _state.gameOver = obj.value("game_over").toBool();

  // render_layers (RaceGame): нитро и жизни
  if (obj.contains("nitro")) {
    _state.nitro = obj.value("nitro").toBool();
  }
  if (obj.contains("lives")) {
    _state.lives = obj.value("lives").toInt();
  }

  if (_state.gameOver) {
    _pollTimer->stop();
  }

  update();  // перерисовать
}

void RaceWidget::paintEvent(QPaintEvent *event) {
  (void)event;
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);

  const int boardX = 0;
  const int boardY = 0;
  // Как и у тетриса: чёрный фон + большое белое поле.
  p.fillRect(0, 0, width(), height(), Qt::black);

  const QColor carColor = Qt::red;
  const QColor emptyColor = Qt::white;
  p.setPen(Qt::black);

  // draw cells
  for (int r = 0; r < RACE_FIELD_HEIGHT; ++r) {
    for (int c = 0; c < RACE_FIELD_WIDTH; ++c) {
      const bool on = _state.field[r][c];
      const QRect cell(boardX + c * CELL_SIZE, boardY + r * CELL_SIZE,
                       CELL_SIZE, CELL_SIZE);
      const QRect inner = cell.adjusted(1, 1, -1, -1);
      p.setBrush(on ? carColor : emptyColor);
      p.drawRect(inner);
    }
  }

  // Информация справа (как в TetrisWidget::drawGameInfo)
  p.setPen(Qt::white);
  p.setFont(QFont("Courier", 10));
  const int rightMargin = 5;
  const int topMargin = 5;
  const int lineHeight = p.fontMetrics().height();

  // Правая панель как в тетрисе (чтобы не налезало на поле)
  const int panelW = 150;
  const int panelX = width() - rightMargin - panelW;

  // Линии-разделители (как в web/тетрисе)
  p.setPen(QColor(120, 120, 120));
  p.drawLine(panelX, topMargin + lineHeight * 4 + 2, panelX + panelW,
             topMargin + lineHeight * 4 + 2);
  p.drawLine(panelX, topMargin + lineHeight * 7 + 2, panelX + panelW,
             topMargin + lineHeight * 7 + 2);
  p.setPen(Qt::white);

  p.drawText(QRect(panelX, topMargin + lineHeight * 0, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop, "Game Status");
  p.drawText(QRect(panelX, topMargin + lineHeight * 1, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop,
             QString("High: %1").arg(_state.highScore));
  p.drawText(QRect(panelX, topMargin + lineHeight * 2, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop,
             QString("Score: %1").arg(_state.score));
  p.drawText(QRect(panelX, topMargin + lineHeight * 3, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop,
             QString("Level: %1").arg(_state.level));
  p.drawText(QRect(panelX, topMargin + lineHeight * 4, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop,
             QString("Speed: %1").arg(_state.speed));
  p.drawText(QRect(panelX, topMargin + lineHeight * 5, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop, "'P' to pause");
  p.drawText(QRect(panelX, topMargin + lineHeight * 6, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop, "'R' to restart");
  p.drawText(QRect(panelX, topMargin + lineHeight * 7, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop,
             QString("Nitro: %1").arg(_state.nitro ? "ON" : "OFF"));
  p.drawText(QRect(panelX, topMargin + lineHeight * 8, panelW, lineHeight),
             Qt::AlignLeft | Qt::AlignTop,
             QString("Lives: %1").arg(_state.lives));

  if (_state.pause && !_state.gameOver) {
    p.setPen(Qt::NoPen);
    p.fillRect(0, 0, width(), height(), QColor(0, 0, 0, 120));
    p.setPen(Qt::yellow);
    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, "PAUSED");
  }

  if (_state.gameOver) {
    p.setPen(Qt::NoPen);
    p.fillRect(0, 0, width(), height(), QColor(0, 0, 0, 160));
    p.setPen(Qt::red);
    p.setFont(QFont("Arial", 18, QFont::Bold));
    p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, "GAME OVER");
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 12, QFont::Normal));
    p.drawText(QRect(0, 0, width(), height()), Qt::AlignHCenter | Qt::AlignTop,
               QString("Score: %1").arg(_state.score));
    p.drawText(QRect(0, 0, width(), height()),
               Qt::AlignHCenter | Qt::AlignBottom, "Press R to restart");
  }
}

void RaceWidget::keyPressEvent(QKeyEvent *event) {
  if (!_gameSelected) {
    QWidget::keyPressEvent(event);
    return;
  }

  if (event->isAutoRepeat()) {
    return;
  }

  switch (event->key()) {
    case Qt::Key_Left:
      sendAction(LEFT, false);
      break;
    case Qt::Key_Right:
      sendAction(RIGHT, false);
      break;
    case Qt::Key_Up:
      if (!_nitroHeld) {
        _nitroHeld = true;
        sendAction(UP, true);
      }
      break;
    case Qt::Key_P:
      sendAction(PAUSE, false);
      break;
    case Qt::Key_R:
      restartGame();
      break;
    case Qt::Key_Escape:
      sendAction(TERMINATE, false);
      _pollTimer->stop();
      close();
      break;
    default:
      break;
  }
}

void RaceWidget::keyReleaseEvent(QKeyEvent *event) {
  if (!_gameSelected) {
    QWidget::keyReleaseEvent(event);
    return;
  }

  switch (event->key()) {
    case Qt::Key_Up:
      if (_nitroHeld) {
        _nitroHeld = false;
        sendAction(UP, false);
      }
      break;
    default:
      break;
  }
}
