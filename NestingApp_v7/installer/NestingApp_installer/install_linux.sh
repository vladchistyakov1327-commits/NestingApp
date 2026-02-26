#!/bin/bash
# ══════════════════════════════════════════════════════════════
#  NestingApp v2.0 — Установщик Linux
#  Поддержка: Ubuntu 20.04+, Debian 11+, Fedora 35+, Arch
# ══════════════════════════════════════════════════════════════

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build_release"
INSTALL_DIR="$HOME/.local/share/NestingApp"
BIN_DIR="$HOME/.local/bin"
DESKTOP_DIR="$HOME/.local/share/applications"

echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║        NestingApp v2.0 — Установщик Linux           ║${NC}"
echo -e "${CYAN}║       Раскладка деталей из листового металла        ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════╝${NC}"
echo ""

# ─── Определяем дистрибутив ───────────────────────────────────────────────────
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif command -v lsb_release &>/dev/null; then
        lsb_release -si | tr '[:upper:]' '[:lower:]'
    else
        echo "unknown"
    fi
}

DISTRO=$(detect_distro)
echo -e "${YELLOW}[INFO]${NC} Дистрибутив: $DISTRO"

# ─── Установка зависимостей ───────────────────────────────────────────────────
echo ""
echo -e "${CYAN}[1/5] Установка зависимостей...${NC}"

install_deps_ubuntu() {
    sudo apt-get update -q
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        qt6-base-dev \
        qt6-base-private-dev \
        libqt6core6 \
        libqt6gui6 \
        libqt6widgets6 \
        qt6-tools-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-dev \
        libfontconfig1-dev \
        libfreetype-dev \
        libtbb-dev \
        pkg-config \
        git
}

install_deps_fedora() {
    sudo dnf install -y \
        gcc-c++ \
        cmake \
        ninja-build \
        qt6-qtbase-devel \
        qt6-qtbase-private-devel \
        mesa-libGL-devel \
        mesa-libGLU-devel \
        libxkbcommon-devel \
        tbb-devel \
        pkgconfig
}

install_deps_arch() {
    sudo pacman -Sy --noconfirm \
        base-devel \
        cmake \
        ninja \
        qt6-base \
        mesa \
        libxkbcommon \
        tbb \
        pkgconf
}

case "$DISTRO" in
    ubuntu|debian|linuxmint|pop)
        install_deps_ubuntu
        ;;
    fedora|rhel|centos|rocky|almalinux)
        install_deps_fedora
        ;;
    arch|manjaro|endeavouros)
        install_deps_arch
        ;;
    *)
        echo -e "${YELLOW}[WARN]${NC} Неизвестный дистрибутив: $DISTRO"
        echo "       Устанавливаем через apt (если доступен)..."
        if command -v apt-get &>/dev/null; then
            install_deps_ubuntu
        elif command -v dnf &>/dev/null; then
            install_deps_fedora
        elif command -v pacman &>/dev/null; then
            install_deps_arch
        else
            echo -e "${RED}[ERROR]${NC} Не найден пакетный менеджер."
            echo "Вручную установите: cmake, Qt6 base dev, g++ (C++20)"
            exit 1
        fi
        ;;
esac

echo -e "${GREEN}[OK]${NC} Зависимости установлены."

# ─── Проверка версий ──────────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}[2/5] Проверка версий...${NC}"

CMAKE_VER=$(cmake --version | head -1 | awk '{print $3}')
echo -e "  CMake:  ${GREEN}$CMAKE_VER${NC}"

GCC_VER=$(g++ --version | head -1 | awk '{print $NF}')
echo -e "  G++:    ${GREEN}$GCC_VER${NC}"

# Проверяем Qt6
QT_VERSION=$(qmake6 --version 2>/dev/null | grep "Qt version" | awk '{print $4}' || \
             qmake --version 2>/dev/null | grep "Qt version" | awk '{print $4}' || \
             echo "не найден")
echo -e "  Qt6:    ${GREEN}$QT_VERSION${NC}"

if [[ "$QT_VERSION" == "не найден" ]]; then
    echo -e "${RED}[ERROR]${NC} Qt6 не найден. Проверьте установку."
    exit 1
fi

# ─── Сборка ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}[3/5] Сборка NestingApp (Release)...${NC}"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Автопоиск Qt6
QT6_CMAKE_DIR=""
for dir in \
    /usr/lib/x86_64-linux-gnu/cmake/Qt6 \
    /usr/lib64/cmake/Qt6 \
    /usr/lib/cmake/Qt6 \
    /usr/local/lib/cmake/Qt6 \
    /opt/Qt/6.*/gcc_64/lib/cmake/Qt6
do
    if [ -d "$dir" ]; then
        QT6_CMAKE_DIR="$dir"
        break
    fi
done

CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20"
if [ -n "$QT6_CMAKE_DIR" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DQt6_DIR=$QT6_CMAKE_DIR"
    echo "  Qt6 cmake dir: $QT6_CMAKE_DIR"
fi

cmake "$PROJECT_DIR/src" $CMAKE_ARGS -G Ninja 2>&1 | tail -5

NPROC=$(nproc 2>/dev/null || echo 4)
echo -e "  Компиляция на $NPROC ядрах..."
ninja -j"$NPROC"

echo -e "${GREEN}[OK]${NC} Сборка успешна."

# ─── Установка ────────────────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}[4/5] Установка в $INSTALL_DIR...${NC}"

mkdir -p "$INSTALL_DIR"
mkdir -p "$BIN_DIR"
mkdir -p "$DESKTOP_DIR"

# Копируем бинарник
EXE_PATH="$BUILD_DIR/NestingApp"
if [ ! -f "$EXE_PATH" ]; then
    EXE_PATH=$(find "$BUILD_DIR" -name "NestingApp" -type f | head -1)
fi

if [ -z "$EXE_PATH" ] || [ ! -f "$EXE_PATH" ]; then
    echo -e "${RED}[ERROR]${NC} Бинарник не найден после сборки."
    exit 1
fi

cp "$EXE_PATH" "$INSTALL_DIR/NestingApp"
chmod +x "$INSTALL_DIR/NestingApp"

# Симлинк в ~/.local/bin (обычно в PATH)
ln -sf "$INSTALL_DIR/NestingApp" "$BIN_DIR/nestingapp"

# .desktop файл для меню приложений
cat > "$DESKTOP_DIR/nestingapp.desktop" << EOF
[Desktop Entry]
Version=2.0
Type=Application
Name=NestingApp
Name[ru]=NestingApp — Раскладка металла
Comment=2D Nesting for sheet metal parts
Comment[ru]=Оптимальная раскладка деталей из листового металла
Exec=$INSTALL_DIR/NestingApp %f
Icon=$INSTALL_DIR/icon.png
Categories=Engineering;Science;
MimeType=application/x-dxf;
Keywords=nesting;metal;DXF;laser;cutting;
StartupNotify=true
EOF

# Создаём простую иконку (PNG placeholder если нет настоящей)
if [ ! -f "$INSTALL_DIR/icon.png" ]; then
    # Создаём минимальный PNG через python если доступен
    python3 -c "
try:
    from PIL import Image, ImageDraw, ImageFont
    img = Image.new('RGBA', (64,64), (30,30,40,255))
    d = ImageDraw.Draw(img)
    d.rectangle([8,8,56,56], outline=(100,200,100), width=2)
    d.rectangle([12,32,32,52], fill=(52,152,219,200))
    d.rectangle([36,20,52,52], fill=(231,76,60,200))
    img.save('$INSTALL_DIR/icon.png')
except: pass
" 2>/dev/null || true
fi

# Обновляем БД приложений
update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true

echo -e "${GREEN}[OK]${NC} Установлено."

# ─── Добавление в PATH если нужно ─────────────────────────────────────────────
echo ""
echo -e "${CYAN}[5/5] Настройка окружения...${NC}"

SHELL_RC=""
if [ -f "$HOME/.bashrc" ]; then SHELL_RC="$HOME/.bashrc"; fi
if [ -n "$ZSH_VERSION" ] && [ -f "$HOME/.zshrc" ]; then SHELL_RC="$HOME/.zshrc"; fi

if [ -n "$SHELL_RC" ]; then
    if ! grep -q "$BIN_DIR" "$SHELL_RC" 2>/dev/null; then
        echo "" >> "$SHELL_RC"
        echo "# NestingApp" >> "$SHELL_RC"
        echo "export PATH=\"\$HOME/.local/bin:\$PATH\"" >> "$SHELL_RC"
        echo -e "  Добавлен $BIN_DIR в PATH (${SHELL_RC})"
    fi
fi

# ─── Готово ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║              Установка завершена!                    ║${NC}"
echo -e "${GREEN}╠══════════════════════════════════════════════════════╣${NC}"
echo -e "${GREEN}║${NC}  Запустить:  nestingapp                              ${GREEN}║${NC}"
echo -e "${GREEN}║${NC}  Или через меню приложений: NestingApp               ${GREEN}║${NC}"
echo -e "${GREEN}║${NC}  Папка:      $INSTALL_DIR  ${GREEN}║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════╝${NC}"
echo ""

read -p "Запустить NestingApp сейчас? [Y/n]: " LAUNCH
LAUNCH=${LAUNCH:-Y}
if [[ "$LAUNCH" =~ ^[Yy]$ ]]; then
    "$INSTALL_DIR/NestingApp" &
    echo -e "${GREEN}Запущено!${NC}"
fi
