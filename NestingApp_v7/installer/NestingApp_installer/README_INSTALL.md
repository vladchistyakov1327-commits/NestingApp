# NestingApp v2.0 — Установка

## Быстрый старт

### Windows (рекомендуется)

```
Правая кнопка на install_windows.ps1 → «Выполнить с помощью PowerShell»
```

Или через командную строку (от имени администратора):
```bat
install_windows.bat
```

### Linux (Ubuntu / Debian / Fedora / Arch)

```bash
chmod +x install_linux.sh
./install_linux.sh
```

---

## Что устанавливается автоматически

| Зависимость | Windows | Linux |
|-------------|---------|-------|
| Visual C++ Redistributable 2022 | ✅ winget | — |
| CMake 3.20+ | ✅ winget | ✅ apt/dnf/pacman |
| Qt 6.5+ | ⚠️ нужен вручную | ✅ apt/dnf/pacman |
| Компилятор C++20 | ⚠️ Visual Studio 2022 | ✅ g++ |

---

## Системные требования

### Windows
- Windows 10 / 11 (64-бит)
- Visual Studio 2022 с компонентом «Разработка C++»
- Qt 6.5+ MSVC 2022 x64 (`https://www.qt.io/download-qt-installer`)
- CMake 3.20+ (`https://cmake.org/download/`)
- ≥ 4 ГБ RAM, ≥ 500 МБ диска

### Linux
- Ubuntu 22.04 / Debian 12 / Fedora 38+ / Arch
- g++ 12+ с поддержкой C++20
- Qt 6.2+ (устанавливается через пакетный менеджер)
- CMake 3.20+
- ≥ 2 ГБ RAM, ≥ 200 МБ диска

---

## Структура папки после установки

```
NestingApp/
├── NestingApp.exe          ← исполняемый файл
├── Qt6Core.dll             ← Qt библиотеки (Windows)
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── platforms/              ← Qt плагины
├── config/
│   ├── settings.ini        ← настройки программы
│   ├── materials.json      ← библиотека материалов
│   └── machines.json       ← параметры станков
├── examples/               ← примеры DXF файлов
└── exports/                ← папка для экспорта
```

---

## Создание Setup.exe (для распространения)

Если хотите создать единый установочный `Setup.exe`:

1. Установите [Inno Setup](https://jrsoftware.org/isinfo.php)
2. Соберите проект и запустите `windeployqt`
3. Скачайте `vc_redist.x64.exe` в папку `installer/components/`
4. Откройте `NestingApp_installer.iss` в Inno Setup Compiler
5. Нажмите `Build → Compile`

Результат: `installer/output/NestingApp_v2.0_Setup.exe`

---

## Ручная сборка (без скриптов)

```bash
# 1. Клонируем / копируем исходники
cd NestingApp

# 2. Конфигурируем
cmake -B build -S src \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR=/path/to/Qt6/lib/cmake/Qt6

# 3. Компилируем
cmake --build build --config Release --parallel

# 4. Linux: запускаем
./build/NestingApp

# 4. Windows: деплоим DLL и запускаем
windeployqt build/Release/NestingApp.exe
./build/Release/NestingApp.exe
```

---

## Возможные проблемы

### «Qt не найден» (Windows)
Установите Qt через официальный установщик: https://www.qt.io/download-qt-installer  
При установке обязательно выберите: **Qt 6.x → MSVC 2022 64-bit**

### «error: 'concepts' file not found» 
Обновите компилятор: g++ 12+ или MSVC 2022. Нужна поддержка C++20.

### Пустое окно / чёрный экран (Linux)
```bash
export QT_QPA_PLATFORM=xcb
./NestingApp
```
Или установите дополнительно: `sudo apt install libxcb-xinerama0 libxcb-icccm4`

### Программа не запускается — «DLL не найдена» (Windows)
Запустите из папки с исполняемым файлом, или добавьте папку в PATH.
Убедитесь что `windeployqt` был выполнен после сборки.
