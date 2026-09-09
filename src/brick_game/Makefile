SHELL = /bin/sh
export PYTHONPATH ?= src

# Указываем компиляторы
CC = g++ 
CFLAGS = -Wall -Wextra -Werror -std=c++11 -g -fPIC
LDFLAGS = -lncurses `pkg-config --cflags --libs Qt5Widgets Qt5Network`
SRC_C = tetris/tetris.c gui/cli/interface.c brick_game.c
SRC_CPP = snake/snake.cpp gui/cli/race_cli.cpp
OBJ_C = $(SRC_C:.c=.o)
OBJ_CPP = $(SRC_CPP:.cpp=.o)
MOC_OBJ = moc_desktop.cpp  # Добавляем переменную для moc файла

.PHONY: all install console desktop clean test gcov_report clang_format clang_check valgrind clear venv python_deps python_console python_test python_test_arcanoid python_coverage python_coverage_arcanoid native_bridge runserver stopserver lint_python lint_python_strict docker

# Основная цель: собираем только native (console + desktop).
# Docker/web поднимается отдельно и не должен быть частью `make`.
all: clean moc console desktop

# Сборка только консольной версии
console: brick_game.c gui/cli/interface.c gui/cli/race_cli.cpp tetris/tetris.c snake/snake.cpp arcanoid/arcanoid.cpp
	mkdir -p build
	$(CC) $(CFLAGS) $^ -o build/console $(LDFLAGS)

# Сборка графической версии
desktop: brick_snake.cpp gui/desktop/desktop.cpp gui/desktop/race_widget.cpp gui/cli/interface.c tetris/tetris.c snake/snake.cpp arcanoid/arcanoid.cpp $(MOC_OBJ)  # Зависит от необходимых файлов
	mkdir -p build
	$(CC) $(CFLAGS) $^ -o build/desktop $(LDFLAGS)

# Генерация moc файла
$(MOC_OBJ): gui/desktop/desktop.h
	moc gui/desktop/desktop.h -o $(MOC_OBJ)

moc: $(MOC_OBJ)

# Общее правило для сборки объектных файлов
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Запуск обоих приложений
run: all
	./build/console  # Запуск CLI версии
	./build/desktop  # Запуск Desktop версии

# Запуск только консольной версии
run_console: console
	./build/console

# Запуск только графической версии
run_desktop: desktop
	./build/desktop

docker:
	cd ../.. && docker compose up --build -d

clean:
	-@$(MAKE) stopserver 2>/dev/null
	-@fuser -k 8007/tcp 2>/dev/null
	rm -rf build
	rm -rf gcov
	find . -name '*.o' -type f -exec rm -f {} +
	rm -f tests/test tests/high_score.txt
	rm -f ../../data/high_score.txt \
	    ../../data/high_score_snake.txt \
	    ../../data/high_score_race.txt
	find dvi ! -name 'documentation.tex' -type f -exec rm -f {} +
	rm -f tetris.tar.gz
	rm -f $(MOC_OBJ)

# Создать виртуальное окружение в корне репозитория (../..).
venv:
	@cd ../.. && if [ ! -x .venv/bin/python ]; then \
		echo "Creating .venv"; \
		python3 -m venv .venv; \
	fi

# Установить python-зависимости из requirements.txt (в .venv).
python_deps: venv
	@cd ../.. && .venv/bin/python -m pip install -U pip >/dev/null
	@cd ../.. && .venv/bin/python -m pip install -r requirements.txt

# Собираем python-тесты через pytest.
# Тесты лежат в `src/tests`, а мосты (ctypes) зависят от `src/native_bridge/libbrickgames.so`.
python_test: python_deps native_bridge
	cd ../.. && $(PYTHON_BIN) -m pytest -q

python_test_arcanoid:
	cd ../.. && $(PYTHON_BIN) -m pytest -q src/tests/test_arcanoid.py

python_coverage: python_deps native_bridge
	cd ../.. && $(PYTHON_BIN) -m coverage run --omit="*/src/tests/*" -m pytest -q
	cd ../.. && $(PYTHON_BIN) -m coverage report -m --omit="*/src/tests/*"
	cd ../.. && $(PYTHON_BIN) -m coverage html -d gcov/python_html

python_coverage_arcanoid: python_deps
	cd ../.. && $(PYTHON_BIN) -m coverage run --source=src/brick_game/arcanoid -m pytest -q src/tests/test_arcanoid.py
	cd ../.. && $(PYTHON_BIN) -m coverage report -m --include="*/src/brick_game/arcanoid/*"

# PEP8 / pycodestyle
# lint_python: проектный режим (line length 127 + игнор финальной пустой строки W391).
lint_python:
	cd ../.. && $(PYTHON_BIN) -m pycodestyle --max-line-length=127 --ignore=W391,W503 src

# lint_python_strict: строгий PEP8 (79 символов).
lint_python_strict:
	cd ../.. && $(PYTHON_BIN) -m pycodestyle src

# Основная цель `make test` теперь запускает python-тесты.
test: python_test

gcov_report: clean moc  # Добавляем вызов moc перед генерацией отчета
	@if [ ! -f tests/test.c ]; then \
		echo "C gcov_report не запустился: файла tests/test.c нет в репозитории."; \
		echo "Сейчас корректнее использовать 'make python_coverage' (coverage.py по Python-коду)."; \
		exit 0; \
	fi; \
	mkdir -p gcov; \
	echo 0 > gcov/high_score.txt; \
	$(CC) $(CFLAGS) $(GCOV_FLAGS) tests/test.c $(SRC_C) $(SRC_CPP) $(MOC_OBJ) -lncurses -Lbuild -ltetris $(LDFLAGS) -o gcov/gcov_test; \
	cd gcov && ./gcov_test; \
	lcov --capture --directory . --output-file gcov/coverage.info; \
	lcov --remove gcov/coverage.info '*tests/*' --output-file gcov/coverage.info; \
	genhtml -o gcov/report gcov/coverage.info

clang_format:
	@files="$(CLANG_FORMAT_FILES)"; \
	if [ -n "$$files" ]; then \
		clang-format -i $$files; \
	else \
		echo "No C/C++ sources found for clang-format"; \
	fi

clang_check:
	@files="$(CLANG_FORMAT_FILES)"; \
	if [ -n "$$files" ]; then \
		clang-format -n $$files; \
	else \
		echo "No C/C++ sources found for clang-format check"; \
	fi

# All files clang-format should cover (C/C++ legacy tree).
CLANG_FORMAT_FILES = \
	$(wildcard *.c *.h *.cpp *.hpp) \
	$(wildcard gui/cli/*.c gui/cli/*.h gui/cli/*.cpp gui/cli/*.hpp) \
	$(wildcard gui/desktop/*.c gui/desktop/*.h gui/desktop/*.cpp gui/desktop/*.hpp) \
	$(wildcard tetris/*.c tetris/*.h) \
	$(wildcard snake/*.c snake/*.h snake/*.cpp snake/*.hpp) \
	$(wildcard tests/*.c tests/*.h tests/*.cpp tests/*.hpp)

valgrind:
	@echo "valgrind для C-тестов не настроен (в репозитории нет tests/test.c). Запускай `make test` вместо этого."

clear:
	clear

PYTHON_BIN ?= $(if $(wildcard ../../.venv/Scripts/python.exe),$(abspath ../../.venv/Scripts/python.exe),$(if $(wildcard ../../.venv/bin/python),$(abspath ../../.venv/bin/python),python3))
BASE_URL ?= http://127.0.0.1:8007
SERVER_HOST ?= 127.0.0.1
SERVER_PORT ?= 8007
SERVER_PID_FILE ?= build/server.pid
SERVER_LOG_FILE ?= build/server.log

python_console:
	PYTHONPATH=.. $(PYTHON_BIN) -m brick_game.console --base-url "$(BASE_URL)"

native_bridge:
	$(MAKE) -C ../native_bridge all

# Запуск FastAPI-сервера в фоне (Linux)
runserver:
	mkdir -p build
	@if [ -f "$(SERVER_PID_FILE)" ] && kill -0 $$(cat "$(SERVER_PID_FILE)") 2>/dev/null; then \
		echo "Server already running on http://$(SERVER_HOST):$(SERVER_PORT) (PID=$$(cat "$(SERVER_PID_FILE)"))"; \
	else \
		echo "Starting server on http://$(SERVER_HOST):$(SERVER_PORT)"; \
		PYTHONPATH=.. nohup $(PYTHON_BIN) -m uvicorn brick_game.server.app:app --host "$(SERVER_HOST)" --port "$(SERVER_PORT)" >"$(SERVER_LOG_FILE)" 2>&1 & echo $$! >"$(SERVER_PID_FILE)"; \
		sleep 1; \
		if kill -0 $$(cat "$(SERVER_PID_FILE)") 2>/dev/null; then \
			echo "Server started (PID=$$(cat "$(SERVER_PID_FILE)")), log: $(SERVER_LOG_FILE)"; \
		else \
			# PID мог завершиться, например, если порт уже занят другим экземпляром. \
			# Проверяем доступность порта: если отвечает — считаем сервер запущенным. \
			if $(PYTHON_BIN) -c "import socket,sys; s=socket.socket(); s.settimeout(0.5); ok=(s.connect_ex(('$(SERVER_HOST)', int('$(SERVER_PORT)')))==0); s.close(); sys.exit(0 if ok else 1)"; then \
				echo "Server already running on http://$(SERVER_HOST):$(SERVER_PORT)"; \
				rm -f "$(SERVER_PID_FILE)"; \
			else \
				echo "Server failed to start, check $(SERVER_LOG_FILE)"; \
				rm -f "$(SERVER_PID_FILE)"; \
				exit 1; \
			fi; \
		fi; \
	fi

stopserver:
	@if [ -f "$(SERVER_PID_FILE)" ] && kill -0 $$(cat "$(SERVER_PID_FILE)") 2>/dev/null; then \
		echo "Stopping server PID=$$(cat "$(SERVER_PID_FILE)")"; \
		kill $$(cat "$(SERVER_PID_FILE)"); \
		rm -f "$(SERVER_PID_FILE)"; \
	else \
		echo "Server is not running"; \
		rm -f "$(SERVER_PID_FILE)"; \
	fi
