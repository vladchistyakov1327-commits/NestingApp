#pragma once
#include "Part.h"
#include <string>
#include <vector>
#include <map>

// Встроенный DXF парсер (без внешних зависимостей).
// Поддерживает: LINE, LWPOLYLINE, POLYLINE+VERTEX, ARC, CIRCLE, SPLINE(approx)
class DXFLoader {
public:
    struct LoadResult {
        std::vector<Polygon> cutContours;   // Слой CUT / красный / 0
        std::vector<Polygon> markContours;  // Слой MARK / синий
        std::vector<std::string> warnings;
        bool success = false;
    };

    static LoadResult loadFile(const std::string& filename);

private:
    // Примитивная сущность DXF
    struct Entity {
        std::string type;
        std::string layer;
        int         color = 256; // 256 = BYLAYER
        bool        closed = false;

        // LINE: pts[0]=start, pts[1]=end
        // LWPOLYLINE/POLYLINE: pts = список вершин
        // ARC/CIRCLE: pts[0]=center
        std::vector<Point> pts;
        double radius     = 0;
        double startAngle = 0;   // градусы
        double endAngle   = 360; // градусы
    };

    using Segment = std::pair<Point, Point>;

    // Разбор файла на сущности
    static std::vector<Entity> parseEntities(std::istream& in);

    // Построение замкнутых контуров из сущностей одного слоя
    static std::vector<Polygon> buildContours(const std::vector<Entity>& ents,
                                               bool cutLayer);

    // ARC → набор точек
    static std::vector<Point> arcToPoints(const Point& center, double radius,
                                           double startDeg, double endDeg,
                                           int segments = 24);

    // Цепочки сегментов → замкнутые полигоны
    static std::vector<Polygon> chainSegments(std::vector<Segment> segs,
                                               double tol = 1.0);

    // Классификация слоя: cut или mark?
    static bool isCutLayer (const std::string& layer, int color);
    static bool isMarkLayer(const std::string& layer, int color);
};
