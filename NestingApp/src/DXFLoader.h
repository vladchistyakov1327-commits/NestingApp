#pragma once
#include "Part.h"
#include <string>
#include <vector>
#include <iosfwd>

// DXFLoader — универсальный парсер DXF R9–R2024
// Поддержка: LINE, LWPOLYLINE, POLYLINE+VERTEX, ARC, CIRCLE, ELLIPSE, SPLINE(NURBS), INSERT

class DXFLoader {
public:
    struct LoadResult {
        std::vector<Polygon> cutContours;   // Слой CUT / layer 0 / любой не-MARK
        std::vector<Polygon> markContours;  // Слой MARK / INTERIOR_PROFILES / синий
        std::vector<std::string> warnings;
        bool success = false;
    };

    static LoadResult loadFile(const std::string& filename);
    static bool isCutLayer (const std::string& layer, int color);
    static bool isMarkLayer(const std::string& layer, int color);

private:
    struct Entity {
        std::string type;
        std::string layer;
        int    color      = 256;
        bool   closed     = false;
        std::vector<Point> pts;
        double radius     = 0.0;
        double startAngle = 0.0;
        double endAngle   = 360.0;
    };

    using Segment = std::pair<Point, Point>;

    struct SplineData {
        int degree = 3;
        int nKnots = 0, nCtrl = 0;
        std::vector<double> knots, weights;
        std::vector<Point>  ctrl;
    };

    static std::vector<Entity> parseEntities (const std::string& content);
    static std::vector<Polygon> buildContours(const std::vector<Entity>& ents, bool cutLayer);
    static std::vector<Polygon> chainSegments(std::vector<Segment> segs, double tol = 1.0);
    static std::vector<Point>   arcToPoints  (const Point& center, double radius,
                                              double startDeg, double endDeg, int segments = 32);
    static std::vector<Point>   splineToPoints(const SplineData& spl, int segments = 64);
};
