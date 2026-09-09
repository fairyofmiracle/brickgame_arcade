#include "snake.h"

void init_snake(GameInfo_t &game) {
  game.snake.clear();
  // Стартовая длина змейки = 4 клетки.
  // Голова в центре, тело тянется влево (при начальном направлении RIGHT).
  int r = FIELD_N / 2;
  int c = FIELD_M / 2;
  for (int i = 0; i < 4; ++i) {
    game.snake.push_back({r, c - i});
  }
  game.dir = Direction::RIGHT;
  game.game_over = false;
  game.paused = false;
  game.score = 0;
  game.win = false;
  // В веб/REST первая стрелка должна "запустить" движение.
  // До этого момента змейка стоит на месте.
  game.waiting_for_input = true;

  // Генерация еды
  srand(static_cast<unsigned int>(time(0)));
  do {
    game.food = {rand() % FIELD_N, rand() % FIELD_M};
  } while (std::find(game.snake.begin(), game.snake.end(), game.food) !=
           game.snake.end());
}

#ifndef BRICKGAME_WASM
void playSnake(GameInfo_t &game) {
  init_snake(game);

  while (true) {
    if (!game.game_over && !game.win) {
      if (!game.paused) {
        render_snake(game);

        int ch = getch();
        // Быстрые команды, работают независимо от режима.
        if (ch == 'q' || ch == 'Q') return;
        if (ch == 'r' || ch == 'R') {
          init_snake(game);
          continue;
        }

        process_input(game, ch);

        update_snake(game);
      } else {
        print_pause_menu();
        int ch = getch();
        if (ch == 'q' || ch == 'Q') return;
        if (ch == 'r' || ch == 'R') {
          init_snake(game);
          continue;
        }
        process_input(game, ch);
      }

      timeout(100);
    } else {
      GameInfo_t gi;
      gi.score = game.score;

      if (game.win) {
        print_win(gi);
      } else if (game.game_over) {
        print_game_over(gi);
      }

      int ch;
      while (true) {
        ch = getch();
        if (ch == 'q' || ch == 'Q') {
          return;  // Выход из игры Змейка
        } else if (ch == 'r' || ch == 'R') {
          init_snake(game);  // Перезапуск игры
          break;             // Выход из цикла ожидания
        }
      }
    }
  }
}
#endif

#ifndef BRICKGAME_WASM
void render_snake(GameInfo_t &game) {
  start_color();  // Инициализация цветов
  init_pair(10, COLOR_GREEN, COLOR_GREEN);  // Цвет для змеи
  init_pair(11, COLOR_RED, COLOR_RED);

  // Отрисовка игрового поля змейки
  WINDOW *snake_window = print_snake_game_field(game);
  wrefresh(snake_window);

  WINDOW *snake_info = print_snake_game_info(game);
  wrefresh(snake_info);

  // Проверка состояния игры
  if (game.game_over || game.win) {
    game.waiting_for_input = true;  // Устанавливаем флаг ожидания ввода
    GameInfo_t gi;  // Создаем временную структуру GameInfo_t
    gi.score = game.score;  // Передаем счет

    if (game.game_over) {
      print_game_over(gi);  // Используем функцию из Тетриса
    } else if (game.win) {
      print_win(gi);  // Используем функцию из Тетриса
    }
  }

  // Освобождение ресурсов
  delwin(snake_window);
  delwin(snake_info);
}
#endif

void update_snake(GameInfo_t &game) {
  if (game.paused) return;  // Если игра на паузе, не обновляем
  if (game.waiting_for_input) return;  // ждем первое направление

  auto head = game.snake.front();
  switch (game.dir) {
    case Direction::UP:
      head.first--;
      break;
    case Direction::DOWN:
      head.first++;
      break;
    case Direction::LEFT:
      head.second--;
      break;
    case Direction::RIGHT:
      head.second++;
      break;
  }

  // Проверка на столкновение с границами
  if (head.first < 0 || head.first >= FIELD_N || head.second < 0 ||
      head.second >= FIELD_M) {
    game.game_over = true;
    return;
  }

  // Проверка на столкновение с собой
  for (auto &part : game.snake) {
    if (part == head) {
      game.game_over = true;
      return;
    }
  }

  game.snake.push_front(head);

  // Проверка на поедание еды
  if (head == game.food) {
    game.score++;
    // Генерация новой еды с проверкой границ
    do {
      game.food = {rand() % FIELD_N, rand() % FIELD_M};
    } while (std::find(game.snake.begin(), game.snake.end(), game.food) !=
             game.snake.end());
  } else {
    game.snake.pop_back();  // Удаление последней части змеи
  }

  // Проверка условия победы
  if (game.score >= 200) {  // Условие победы
    game.win = true;
  }
}

#ifndef BRICKGAME_WASM
void process_input(GameInfo_t &game, int ch) {
  if (game.waiting_for_input) {
    // Первый ввод: разрешаем только смену направления стрелками.
    // Любые другие клавиши игнорируем (кроме 'q' в качестве выхода).
    if (ch == 'q' || ch == 'Q') {
      game.game_over = true;
      return;
    }
    switch (ch) {
      case KEY_UP:
        game.dir = Direction::UP;
        game.waiting_for_input = false;
        return;
      case KEY_DOWN:
        game.dir = Direction::DOWN;
        game.waiting_for_input = false;
        return;
      case KEY_LEFT:
        game.dir = Direction::LEFT;
        game.waiting_for_input = false;
        return;
      case KEY_RIGHT:
        game.dir = Direction::RIGHT;
        game.waiting_for_input = false;
        return;
      default:
        return;
    }
  }

  // Обработка других команд
  switch (ch) {
    case KEY_UP:
      if (game.dir != Direction::DOWN)  // Удалено лишнее двоеточие
        game.dir = Direction::UP;  // Удалено лишнее двоеточие
      break;
    case KEY_DOWN:
      if (game.dir != Direction::UP)  // Удалено лишнее двоеточие
        game.dir = Direction::DOWN;
      break;
    case KEY_LEFT:
      if (game.dir != Direction::RIGHT)  // Удалено лишнее двоеточие
        game.dir = Direction::LEFT;  // Удалено лишнее двоеточие
      break;
    case KEY_RIGHT:
      if (game.dir != Direction::LEFT)  // Удалено лишнее двоеточие
        game.dir = Direction::RIGHT;  // Удалено лишнее двоеточие
      break;
    case 'p':
      game.paused = !game.paused;  // Переключение паузы
      break;
    case 'P':
      game.paused = !game.paused;  // Переключение паузы
      break;
      // Добавьте другие команды по желанию
  }
}
#endif