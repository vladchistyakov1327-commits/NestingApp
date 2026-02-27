# Установка NestingApp v2.0

## Содержимое пакета

```
installer/
  NestingApp.iss       ← Скрипт Inno Setup (Windows Setup.exe)
  build_windows.ps1    ← Автоматическая сборка для Windows
  build_linux.sh       ← Автоматическая сборка для Linux
CMakeLists.txt         ← Проект CMake
src/                   ← Исходный код C++20
```

---

## Windows (рекомендуемый способ)

### Шаг 1 — Установи инструменты (один раз)

| Программа | Где скачать | Примечание |
|-----------|-------------|------------|
| **Visual Studio 2022** | https://visualstudio.microsoft.com/ | Выбери компонент «Разработка классических приложений на C++» |
| **Qt 6.5+** | https://www.qt.io/download-qt-installer | При установке выбери `MSVC 2019 64-bit` |
| **CMake 3.20+** | https://cmake.org/download/ | Добавь в PATH |
| **Inno Setup 6** | https://jrsoftware.org/isinfo.php | Для создания Setup.exe |

### Шаг 2 — Открой «Developer PowerShell for VS 2022»

Меню Пуск → Visual Studio 2022 → **Developer PowerShell**

### Шаг 3 — Укажи путь к Qt

Открой `installer\build_windows.ps1` в Блокноте и исправь строку:
```powershell
$QtDir = "C:\Qt\6.5.3\msvc2019_64"   # ← твой путь
```

### Шаг 4 — Запусти сборку

```powershell
cd путь\к\NestingApp_v5
.\installer\build_windows.ps1
```

Скрипт автоматически:
1. Скомпилирует программу
2. Скопирует все нужные Qt DLL через `windeployqt`
3. Скачает `vc_redist.x64.exe`
4. Создаст `dist\NestingApp_v2_Setup.exe`

### Шаг 5 — Запусти `dist\NestingApp_v2_Setup.exe`

Далее — обычный мастер установки. После завершения ярлык появится на рабочем столе.

---

## Linux (Ubuntu / Debian)

```bash
cd NestingApp_v5
chmod +x installer/build_linux.sh
./installer/build_linux.sh
```

Скрипт:
- Установит все зависимости через `apt-get`
- Скомпилирует программу
- Создаст ярлык в меню приложений
- (Ubuntu/Debian) Создаст `.deb` пакет в `build_release/`

После установки запуск: `nestingapp` или через меню.

---

## Linux (Fedora / RHEL)

```bash
./installer/build_linux.sh
```
Скрипт определит дистрибутив и использует `dnf` автоматически.

---

## Linux (Arch)

```bash
./installer/build_linux.sh
```
Использует `pacman`.

---

## Ручная сборка (любая платформа)

```bash
# 1. Конфигурация
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/путь/к/Qt6

# 2. Сборка
cmake --build build --config Release --parallel

# 3. Запуск
./build/NestingApp          # Linux
build\Release\NestingApp.exe  # Windows
```

---

## Требования к системе

| | Минимум | Рекомендуется |
|--|---------|--------------|
| **ОС** | Windows 10 x64 / Ubuntu 20.04 | Windows 11 / Ubuntu 22.04+ |
| **CPU** | 4 ядра, 2.0 GHz | 8+ ядер (ГА использует все) |
| **RAM** | 2 GB | 8 GB |
| **Диск** | 200 MB | 500 MB |
| **Дисплей** | 1280×720 | 1920×1080+ |

---

## Часто задаваемые вопросы

**Q: Ошибка "Qt6 not found"**  
A: Укажи путь явно: `-DCMAKE_PREFIX_PATH=C:\Qt\6.5.3\msvc2019_64`

**Q: Ошибка "gomp not found" (Linux)**  
A: `sudo apt install libgomp1` или удали `-fopenmp` из CMakeLists.txt

**Q: Программа запускается но нет окна (Linux Wayland)**  
A: `QT_QPA_PLATFORM=xcb nestingapp`

**Q: Как проверить что программа работает?**  
A: Меню → О программе покажет версию. Загрузи любой DXF файл и нажми «БЫСТРО».
