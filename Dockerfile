FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

# Обновляем списки пакетов и устанавливаем необходимые инструменты
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    gcc \
    g++ \
    make \
    ninja-build \
    pkg-config \
    zsh \
    curl \
    wget \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Установка Oh My Zsh для удобства использования
RUN sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

# Устанавливаем рабочую директорию
WORKDIR /labs

# Определяем точку монтирования для кода
VOLUME ["/labs"]

# Устанавливаем zsh как оболочку по умолчанию
SHELL ["/bin/zsh", "-c"]

# Запускаем zsh при старте контейнера
CMD ["/bin/zsh"]
