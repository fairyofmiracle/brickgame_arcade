#include <QApplication>
#include <QDebug>

#include "gui/cli/interface.h"
#include "gui/desktop/desktop.h"
#include "snake/snake.h"
#include "tetris/tetris.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  qDebug() << "Приложение запущено";

  GameSelector selector;
  qDebug() << "GameSelector created";
  selector.setWindowTitle("Выбор игры");
  selector.resize(200, 200);
  selector.show();  // Показать окно выбора игры
  qDebug() << "Окно показано";

  return app.exec();  // Запуск основного цикла приложения
}
