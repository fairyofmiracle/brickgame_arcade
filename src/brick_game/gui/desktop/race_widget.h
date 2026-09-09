#ifndef RACE_WIDGET_H
#define RACE_WIDGET_H

#include <QJsonObject>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QPaintEvent>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <vector>

#include "../common/race_rest_client.h"

class RaceWidget : public QWidget {
 public:
  explicit RaceWidget(int gameId, QWidget *parent = nullptr);
  ~RaceWidget();

 protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;

 private:
  struct RaceStateView {
    std::vector<std::vector<bool>> field;  // [20][10]
    int score = 0;
    int highScore = 0;
    int level = 1;
    int speed = 0;
    bool pause = false;
    bool nitro = false;
    int lives = 0;
    bool gameOver = false;
    int gameId = 1;
  };

  static constexpr int RACE_FIELD_HEIGHT = 20;
  static constexpr int RACE_FIELD_WIDTH = 10;
  static constexpr int CELL_SIZE =
      30;  // must match BLOCK_SIZE in other desktop widgets
  static constexpr int INFO_Y = RACE_FIELD_HEIGHT * CELL_SIZE + 60;

  enum ActionId : int {
    START = 0,
    PAUSE = 1,
    TERMINATE = 2,
    LEFT = 3,
    RIGHT = 4,
    UP = 5,
    DOWN = 6,
    ACTION = 7
  };

  bool _gameSelected = false;
  bool _inFlight = false;
  bool _nitroHeld = false;

  race_rest::RaceRestClient _rest_client;
  int _gameId = 1;
  QString _baseUrl;

  QNetworkAccessManager *_net = nullptr;
  QTimer *_pollTimer = nullptr;

  RaceStateView _state;

  void setDefaultState();
  void selectGame(int gameId);
  void restartGame();
  void sendAction(ActionId actionId, bool hold);
  void fetchState();

  void handleStateJson(const QJsonObject &obj);
  static QString envBaseUrl();

  QUrl makeUrl(const QString &path) const;
  QString makeAbsBaseUrl() const;
  static QString stripTrailingSlash(QString s);
};

#endif  // RACE_WIDGET_H
