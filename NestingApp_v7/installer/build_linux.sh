#!/usr/bin/env bash
# ============================================================
#  build_linux.sh  —  Сборка и установка NestingApp для Linux
#
#  Запуск:
#    chmod +x build_linux.sh
#    ./build_linux.sh
#
#  Поддерживаемые дистрибутивы:
#    Ubuntu / Debian 20.04+
#    Fedora / RHEL 8+
#    Arch Linux
# ============================================================

set -euo pipefail

# ── Настройки ────────────────────────────────────────────────────────────────
APP_NAME="NestingApp"
APP_VERSION="2.0"
INSTALL_DIR="/opt/nestingapp"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build_release"

# Цвета
RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'
YELLOW='\033[1;33m'; NC='\033[0m'

step()  { echo -e "\n${CYAN}==> $1${NC}"; }
ok()    { echo -e "${GREEN}✔ $1${NC}"; }
warn()  { echo -e "${YELLOW}⚠ $1${NC}"; }
error() { echo -e "${RED}✗ $1${NC}"; exit 1; }

# ── Определяем дистрибутив ───────────────────────────────────────────────────
detect_distro() {
    if   [ -f /etc/debian_version ]; then echo "debian"
    elif [ -f /etc/fedora-release ];  then echo "fedora"
    elif [ -f /etc/arch-release ];    then echo "arch"
    else echo "unknown"; fi
}

DISTRO=$(detect_distro)

# ── Установка зависимостей ───────────────────────────────────────────────────
step "Установка зависимостей (дистрибутив: $DISTRO)"

if [ "$DISTRO" = "debian" ]; then
    sudo apt-get update -qq
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        qt6-base-dev \
        qt6-base-private-dev \
        libqt6gui6 \
        libqt6widgets6 \
        libgl1-mesa-dev \
        libfontconfig1-dev \
        libfreetype-dev \
        libx11-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxrender-dev \
        libxcb1-dev \
        libx11-xcb-dev \
        libxcb-glx0-dev \
        libxcb-keysyms1-dev \
        libxcb-image0-dev \
        libxcb-shm0-dev \
        libxcb-icccm4-dev \
        libxcb-sync-dev \
        libxcb-render-util0-dev \
        libxcb-shape0-dev \
        libxcb-randr0-dev \
        libxcb-xfixes0-dev \
        libxcb-xkb-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-dev \
        libgomp1 || warn "Некоторые пакеты недоступны — продолжаем"

elif [ "$DISTRO" = "fedora" ]; then
    sudo dnf install -y \
        gcc-c++ cmake ninja-build \
        qt6-qtbase-devel \
        mesa-libGL-devel \
        libxcb-devel \
        libXi-devel \
        libXrender-devel \
        libxkbcommon-devel \
        libxkbcommon-x11-devel \
        libgomp || warn "Некоторые пакеты недоступны"

elif [ "$DISTRO" = "arch" ]; then
    sudo pacman -Sy --noconfirm \
        base-devel cmake ninja \
        qt6-base \
        libxcb libx11 libxrender libxi \
        libxkbcommon libxkbcommon-x11 || warn "Некоторые пакеты недоступны"

else
    warn "Неизвестный дистрибутив. Убедись что установлены: cmake g++ Qt6"
fi

ok "Зависимости установлены"

# ── Сборка ───────────────────────────────────────────────────────────────────
step "Конфигурация CMake"

mkdir -p "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$ROOT_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
      -G Ninja

step "Компиляция"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

ok "Сборка завершена: $BUILD_DIR/NestingApp"

# ── Установка ────────────────────────────────────────────────────────────────
step "Установка в $INSTALL_DIR"

sudo mkdir -p "$INSTALL_DIR/bin"
sudo cp "$BUILD_DIR/NestingApp" "$INSTALL_DIR/bin/"
sudo chmod +x "$INSTALL_DIR/bin/NestingApp"

# Примеры
if [ -d "$ROOT_DIR/examples" ]; then
    sudo mkdir -p "$INSTALL_DIR/examples"
    sudo cp -r "$ROOT_DIR/examples/"* "$INSTALL_DIR/examples/"
fi

ok "Файлы скопированы в $INSTALL_DIR"

# ── Иконка и .desktop файл ───────────────────────────────────────────────────
step "Создание ярлыка рабочего стола"

# Создаём иконку (SVG)
sudo mkdir -p /usr/share/icons/hicolor/scalable/apps
sudo tee /usr/share/icons/hicolor/scalable/apps/nestingapp.svg > /dev/null << 'SVGEOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect width="64" height="64" rx="8" fill="#1e3a5f"/>
  <rect x="8" y="8" width="48" height="48" rx="4" fill="none" stroke="#3498db" stroke-width="2"/>
  <polygon points="12,52 12,20 28,12 52,12 52,44 36,52" fill="#2475b0" opacity="0.8"/>
  <polygon points="22,44 22,30 32,22 44,22 44,36 34,44" fill="#46b4e8" opacity="0.9"/>
  <text x="32" y="58" font-size="7" fill="#aaa" text-anchor="middle" font-family="sans-serif">NEST</text>
</svg>
SVGEOF

# .desktop файл
sudo tee /usr/share/applications/nestingapp.desktop > /dev/null << DESKEOF
[Desktop Entry]
Version=1.0
Type=Application
Name=NestingApp
Name[ru]=NestingApp — Раскладка деталей
GenericName=Nesting Software
GenericName[ru]=Программа раскладки деталей
Comment=2D nesting for sheet metal — NFP + Genetic Algorithm
Comment[ru]=Оптимальная раскладка листового металла
Exec=$INSTALL_DIR/bin/NestingApp %F
Icon=nestingapp
Terminal=false
Categories=Engineering;Science;
MimeType=application/x-dxf;
Keywords=nesting;dxf;metal;laser;cutting;
StartupNotify=true
DESKEOF

# Симлинк в PATH
if [ ! -f /usr/local/bin/nestingapp ]; then
    sudo ln -sf "$INSTALL_DIR/bin/NestingApp" /usr/local/bin/nestingapp
fi

# Обновляем базы данных иконок
sudo update-desktop-database /usr/share/applications/ 2>/dev/null || true
sudo gtk-update-icon-cache /usr/share/icons/hicolor/ 2>/dev/null || true

ok "Ярлык создан: /usr/share/applications/nestingapp.desktop"

# ── Создание .deb пакета (только для Debian/Ubuntu) ──────────────────────────
if [ "$DISTRO" = "debian" ] && command -v dpkg-deb &> /dev/null; then
    step "Создание .deb пакета"

    DEB_DIR="$BUILD_DIR/nestingapp_${APP_VERSION}_amd64"
    mkdir -p "$DEB_DIR/DEBIAN"
    mkdir -p "$DEB_DIR/opt/nestingapp/bin"
    mkdir -p "$DEB_DIR/usr/share/applications"
    mkdir -p "$DEB_DIR/usr/share/icons/hicolor/scalable/apps"
    mkdir -p "$DEB_DIR/usr/local/bin"

    cp "$BUILD_DIR/NestingApp" "$DEB_DIR/opt/nestingapp/bin/"
    cp /usr/share/applications/nestingapp.desktop "$DEB_DIR/usr/share/applications/"
    cp /usr/share/icons/hicolor/scalable/apps/nestingapp.svg "$DEB_DIR/usr/share/icons/hicolor/scalable/apps/"
    ln -sf /opt/nestingapp/bin/NestingApp "$DEB_DIR/usr/local/bin/nestingapp"

    cat > "$DEB_DIR/DEBIAN/control" << CTRL
Package: nestingapp
Version: ${APP_VERSION}
Architecture: amd64
Maintainer: MetalShop <info@metalshop.ru>
Depends: libqt6widgets6 (>= 6.2), libqt6gui6, libgl1, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-shape0, libxcb-xkb1, libxkbcommon-x11-0
Suggests: qtwayland5
Description: Professional 2D nesting software for sheet metal
 NestingApp оптимально располагает детали на листах металла
 с использованием NFP (No-Fit Polygon) и генетического алгоритма
 с островной моделью. Поддержка DXF, экспорт LXD/DXF.
Homepage: https://github.com/metalshop/nestingapp
CTRL

    cat > "$DEB_DIR/DEBIAN/postinst" << POSTINST
#!/bin/bash
update-desktop-database /usr/share/applications/ || true
gtk-update-icon-cache /usr/share/icons/hicolor/ || true
POSTINST
    chmod +x "$DEB_DIR/DEBIAN/postinst"

    dpkg-deb --build "$DEB_DIR" "$BUILD_DIR/nestingapp_${APP_VERSION}_amd64.deb"
    ok ".deb пакет создан: $BUILD_DIR/nestingapp_${APP_VERSION}_amd64.deb"
fi

# ── Итог ─────────────────────────────────────────────────────────────────────
echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "${GREEN} УСТАНОВКА ЗАВЕРШЕНА УСПЕШНО!${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""
echo " Запуск из терминала:  nestingapp"
echo " Или через меню приложений: NestingApp"
echo ""
echo " Установлено в: $INSTALL_DIR"
[ -f "$BUILD_DIR/nestingapp_${APP_VERSION}_amd64.deb" ] && \
    echo " .deb пакет:   $BUILD_DIR/nestingapp_${APP_VERSION}_amd64.deb"
echo ""
