#ifndef INTERFACE_H
#define INTERFACE_H

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "../../brick_game.h"

class GameSelector : public QWidget {
  Q_OBJECT

 public:
  GameSelector(QWidget *parent = nullptr);

 private slots:
  void startSnakeGame();
  void startTetrisGame();
};

#endif  // INTERFACE_H
