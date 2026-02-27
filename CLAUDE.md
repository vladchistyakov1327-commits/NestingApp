# NestingApp — Раскладка листового металла

C++20/Qt6 приложение для оптимальной раскладки деталей на листах металла.

## Структура проекта

```
NestingApp/
  CMakeLists.txt          — CMake сборка (C++20, Qt6::Core + Qt6::Widgets)
  src/
    main.cpp              — точка входа
    MainWindow.h/cpp      — Qt6 GUI, тёмная тема
    Geometry.h/cpp        — Point, Polygon, NFP, Минковская сумма, SAT
    Part.h/cpp            — деталь (контур резки + гравировка)
    Sheet.h/cpp           — лист, Bottom-Left размещение
    NestingEngine.h/cpp   — Fast (жадный BL) и Optimal (GA) режимы
    GeneticAlgorithm.h/cpp — PMX-кроссовер, swap-мутация, турнирный отбор
    DXFLoader.h/cpp       — встроенный DXF-парсер (LINE, LWPOLYLINE, ARC, CIRCLE)
    LXDExporter.h/cpp     — экспорт в бинарный .lxd
  installer/
    NestingApp.iss        — Inno Setup скрипт (полный)
    NestingApp_ci.iss     — Inno Setup для CI
    build_windows.ps1     — PowerShell сборка для Windows
.github/workflows/
  build.yml               — GitHub Actions: сборка + инсталлятор Windows
```

## Сборка

### Windows (Visual Studio 2022)
```cmd
cd NestingApp
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Зависимости
- **Qt 6.5+** (обязательно): `vcpkg install qt6-base:x64-windows`
- **TBB** (опционально, для ускорения): `vcpkg install tbb:x64-windows`
- **libdxfrw** (опционально): `vcpkg install libdxfrw:x64-windows`

## Ключевые алгоритмы

- **NFP** — No-Fit Polygon через Минковскую сумму (выпуклые: точно; невыпуклые: convex fallback)
- **NFP-кеш** — по ключу (indexA, indexB, angle)
- **GA** — Optimal режим: 200-500 поколений, 60-80 особей, PMX + hill-climbing
- **BL** — Fast режим: жадный Bottom-Left

## CI/CD (GitHub Actions)

`build.yml` запускается на push в `main`:
1. Установка Qt 6.7.0 (msvc2019_64)
2. CMake Release сборка
3. windeployqt деплой DLL
4. Скачивание VC Redist
5. Inno Setup → `NestingApp_v2_Setup.exe`
6. Артефакты: `NestingApp_Setup` (30 дней) и `NestingApp_Portable` (14 дней)

## Форматы файлов

- **Вход**: DXF (LINE, LWPOLYLINE, ARC, CIRCLE), CSV (name,dxf_path,count)
- **Выход**: бинарный LXD (magic=0x4C584421, полигоны Cut/Mark)
