#pragma once
#include "Part.h"
#include <vector>
#include <unordered_map>
#include <string>

// ─── Кеш NFP ─────────────────────────────────────────────────────────────────
// Ключ: "partId_angle" — NFP для конкретной детали под конкретным углом
// (NFP с листом не зависит от позиции, только от формы + угла поворота)
// NFP между двумя деталями: "id1_ang1__id2_ang2"
using NFPCache = std::unordered_map<std::string, Polygon>;

// ─── Sheet ────────────────────────────────────────────────────────────────────
class Sheet {
public:
    double width  = 3000;
    double height = 1500;
    double margin = 10;
    double gap    = 5;        // минимальный зазор между деталями

    std::vector<PlacedPart> placed;

    Sheet() = default;
    Sheet(double w, double h, double m, double g)
        : width(w), height(h), margin(m), gap(g) {}

    // Рабочая область (с учётом отступов)
    Rect usableArea() const {
        return {margin, margin, width-2*margin, height-2*margin};
    }

    // ──────────────────────────────────────────────────────────────────────────
    // Главный метод: найти лучшую позицию для детали
    //
    // Алгоритм (NFP-based Bottom-Left):
    //  1. Вычисляем IFR — допустимую область для pos[0,0] детали на листе
    //  2. Для каждой уже размещённой детали вычисляем NFP(placed, new)
    //     — позиции опорной точки новой детали, при которых она касается placed
    //  3. Допустимое множество = IFR \ union(interior(NFP))
    //  4. Ищем точку в допустимом множестве с минимальным (y*W + x) — bottom-left
    //
    // Возвращает {pos, angle, valid=true} или {_,_,false} если нет места
    // ──────────────────────────────────────────────────────────────────────────
    struct Placement { Point pos; double angle; bool valid = false; };

    Placement findBestPlacement(
        const Part& part,
        const std::vector<double>& angles,
        NFPCache& cache
    ) const;

    // Разместить деталь
    void place(const Part& part, const Point& pos, double angleDeg);

    // Проверить конкретную позицию
    bool canPlace(const Polygon& transformedShape) const;

    // Статистика
    double utilization() const;
    bool verboseLog = false; // Подробный диагностический вывод в stderr
    double placedArea()  const;

    // Ключи кеша NFP
    static std::string nfpKeySheet(int partId, double angle);
    static std::string nfpKeyParts(int idA, double angA, int idB, double angB);

private:
    // Вычислить NFP(fixed_placed, moving_part) с учётом зазора gap.
    // Результат кешируется в локальных координатах, возвращается в глобальных.
    Polygon getNFP(const PlacedPart& placed, const Part& moving,
                   double movingAngle, NFPCache& cache) const;

    // IFR (Inner Fit Rectangle) — область допустимых позиций на листе
    Rect getIFR(const Part& part, double angle) const;

    // Кандидаты на позицию: точки на контурах NFP + углы IFR
    // (Bottom-Left: первый кандидат с минимальным y, потом x)
    std::vector<Point> getCandidatePoints(
        const Rect& ifr,
        const std::vector<Polygon>& nfps
    ) const;

    // Фильтр: точка внутри IFR и вне всех NFP
    bool isValidPosition(const Point& pos, const Rect& ifr,
                          const std::vector<Polygon>& nfps,
                          const Polygon& transformedShape) const;
};
