# NestingApp — Раскладка листового металла

## Сборка (Windows, Visual Studio 2022)

```cmd
git clone <repo>
cd NestingApp
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

**Зависимости (vcpkg):**
```cmd
vcpkg install qt6-base:x64-windows
```

Опционально (для ускорения):
```cmd
vcpkg install tbb:x64-windows
vcpkg install libdxfrw:x64-windows
```

## Архитектура

| Файл | Назначение |
|------|-----------|
| `Geometry.h/cpp` | Классы Point, Polygon. Сумма Минковского, NFP, SAT-проверка пересечений |
| `Part.h/cpp` | Деталь с геометрией (контур резки + гравировка) |
| `Sheet.h/cpp` | Лист: размещение деталей, Bottom-Left поиск позиции |
| `GeneticAlgorithm.h/cpp` | GA: PMX-кроссовер, swap-мутация, турнирная селекция, hill-climbing |
| `NestingEngine.h/cpp` | Два режима: Fast (жадный BL) и Optimal (GA) |
| `DXFLoader.h/cpp` | Встроенный DXF-парсер (LINE, LWPOLYLINE, ARC, CIRCLE) без внешних зависимостей |
| `LXDExporter.h/cpp` | Бинарный экспорт в .lxd |
| `MainWindow.h/cpp` | Qt6 GUI с тёмной темой |

## Алгоритм NFP

NFP(A, B) = Минковская сумма(A, reflect(B))

- Для выпуклых полигонов: точная реализация через слияние отсортированных рёбер
- Для невыпуклых: fallback к выпуклой аппроксимации (можно расширить через convex decomposition)
- Кеш NFP по паре (indexA, indexB, angle) — вычисляется один раз

## Генетический алгоритм

| Параметр | Fast режим | Optimal режим |
|----------|-----------|--------------|
| Поколений | — | 200-500 |
| Популяция | — | 60-80 |
| Кроссовер | — | PMX (порядок) |
| Мутация | — | Swap + случайный угол |
| Углы | 0/90/180/270° | 0/90/180/270° |
| Целевой % | ~70-85% | 95-97% |

## Формат CSV для загрузки партии
```
name,dxf_path,count
Деталь_А,C:/parts/part_a.dxf,10
Деталь_Б,C:/parts/part_b.dxf,5
```

## Формат LXD (бинарный)
```
Заголовок (32 байт): magic=0x4C584421, version, units, bounds
Полигон: uint32 type (1=Cut/2=Mark), uint32 nVerts, float x[n], float y[n]
```
