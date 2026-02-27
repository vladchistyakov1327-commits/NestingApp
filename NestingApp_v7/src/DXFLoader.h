#pragma once
#include "Part.h"
#include <string>
#include <vector>
#include <iosfwd>

// DXFLoader — универсальный парсер DXF R9–R2024
// Поддерживает: LINE, LWPOLYLINE, POLYLINE+VERTEX, ARC, CIRCLE,
//               ELLIPSE, SPLINE, INSERT (блоки)

class DXFLoader {
public:
    // ── Результат загрузки ────────────────────────────────────────────────────
    struct LoadResult {
        std::vector<Polygon> cutContours;    // Слой CUT / красный / Layer 0
        std::vector<Polygon> markContours;   // Слой MARK / синий / гравировка
        std::vector<std::string> warnings;
        bool success = false;
    };

    static LoadResult loadFile(const std::string& filename);

    // ── Вспомогательные (нужны для тестов и расширений) ───────────────────────
    static bool isCutLayer (const std::string& layer, int color);
    static bool isMarkLayer(const std::string& layer, int color);

private:
    // ── Внутренняя сущность DXF ───────────────────────────────────────────────
    struct Entity {
        std::string type;
        std::string layer;
        int    color      = 256;   // 256 = BYLAYER
        bool   closed     = false;

        // LINE:             pts[0]=start, pts[1]=end
        // LWPOLYLINE/POLY:  pts = вершины
        // ARC/CIRCLE:       pts[0]=center
        // ELLIPSE:          pts[0]=center, pts[1]=major endpoint (относительный)
        // SPLINE:           pts = контрольные точки
        std::vector<Point> pts;
        double radius     = 0.0;
        double startAngle = 0.0;   // градусы (ARC) или радианы (ELLIPSE)
        double endAngle   = 360.0; // градусы (ARC) или радианы (ELLIPSE)
    };

    using Segment = std::pair<Point, Point>;

    // ── Парсеры ───────────────────────────────────────────────────────────────
    // Основной (R2000+): читает пары строк group/value
    static std::vector<Entity> parseEntities(std::istream& in);

    // Fallback (R9-R12, нестандартные экспортёры)
    static std::vector<Entity> parseEntitiesR12(const std::string& content);

    // ── Построение контуров ───────────────────────────────────────────────────
    static std::vector<Polygon> buildContours(const std::vector<Entity>& ents,
                                               bool cutLayer);

    // ARC → набор точек (аппроксимация)
    static std::vector<Point> arcToPoints(const Point& center, double radius,
                                           double startDeg, double endDeg,
                                           int segments = 32);

    // Цепочки сегментов → замкнутые полигоны (O(n) через spatial hash)
    static std::vector<Polygon> chainSegments(std::vector<Segment> segs,
                                               double tol = 1.0);
};
