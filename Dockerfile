FROM python:3.11-slim

WORKDIR /app

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1

# Устанавливаем uv для быстрого управления зависимостями
RUN pip install --no-cache-dir uv

# Устанавливаем зависимости проекта
COPY requirements.txt .
RUN uv pip install --system -r requirements.txt

# Копируем исходники (включая native_bridge)
COPY src ./src

# Общая папка для файлов рекордов (тетрис / змейка / гонки)
RUN mkdir -p /app/data

# Собираем C/C++-мост
WORKDIR /app/src/native_bridge
RUN apt-get update && apt-get install -y build-essential libncurses-dev \
    && make \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app

ENV PYTHONPATH=/app/src

EXPOSE 8007

# Запуск FastAPI-сервера
CMD ["uvicorn", "brick_game.server.app:app", "--host", "0.0.0.0", "--port", "8007"]