#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static constexpr double GEO_EPS = 1e-9;

// ═══════════════════════════════════════════════════════════════════════════════
// Point
// ═══════════════════════════════════════════════════════════════════════════════
struct Point {
    double x = 0, y = 0;
    Point() = default;
    Point(double ax, double ay) : x(ax), y(ay) {}

    Point operator+(const Point& o) const { return {x+o.x, y+o.y}; }
    Point operator-(const Point& o) const { return {x-o.x, y-o.y}; }
    Point operator*(double s)        const { return {x*s,   y*s};   }
    Point operator-()                const { return {-x, -y};       }
    Point& operator+=(const Point& o){ x+=o.x; y+=o.y; return *this; }

    double cross(const Point& o) const { return x*o.y - y*o.x; }
    double dot  (const Point& o) const { return x*o.x + y*o.y; }
    double length()   const { return std::sqrt(x*x + y*y); }
    double lengthSq() const { return x*x + y*y; }
    double distanceTo(const Point& o) const { return (*this - o).length(); }

    bool nearlyEqual(const Point& o, double tol = 1e-6) const {
        return std::abs(x-o.x) < tol && std::abs(y-o.y) < tol;
    }
    bool operator==(const Point& o) const { return nearlyEqual(o); }

    Point rotated(double rad) const {
        double c = std::cos(rad), s = std::sin(rad);
        return {x*c - y*s, x*s + y*c};
    }
    Point normalized() const {
        double l = length();
        return l < GEO_EPS ? Point{} : Point{x/l, y/l};
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Rect
// ═══════════════════════════════════════════════════════════════════════════════
struct Rect {
    double x=0, y=0, w=0, h=0;
    double right()  const { return x+w; }
    double bottom() const { return y+h; }
    bool   isValid() const { return w >= -GEO_EPS && h >= -GEO_EPS && w + h > -GEO_EPS; }

    bool contains(const Point& p) const {
        return p.x >= x-GEO_EPS && p.x <= right()+GEO_EPS
            && p.y >= y-GEO_EPS && p.y <= bottom()+GEO_EPS;
    }
    bool intersects(const Rect& o) const {
        return x < o.right()-GEO_EPS && right() > o.x+GEO_EPS
            && y < o.bottom()-GEO_EPS && bottom() > o.y+GEO_EPS;
    }
    Rect expanded(double d) const { return {x-d, y-d, w+2*d, h+2*d}; }

    static Rect fromPoints(const std::vector<Point>& pts) {
        if (pts.empty()) return {};
        double minx=pts[0].x, miny=pts[0].y, maxx=minx, maxy=miny;
        for (auto& p: pts) {
            minx=std::min(minx,p.x); miny=std::min(miny,p.y);
            maxx=std::max(maxx,p.x); maxy=std::max(maxy,p.y);
        }
        return {minx, miny, maxx-minx, maxy-miny};
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Polygon — произвольный полигон (может быть невыпуклым)
// ═══════════════════════════════════════════════════════════════════════════════
class Polygon {
public:
    std::vector<Point> verts;

    Polygon() = default;
    explicit Polygon(std::vector<Point> v) : verts(std::move(v)) {}

    bool   empty()  const { return verts.empty(); }
    size_t size()   const { return verts.size();  }
    const std::vector<Point>& getVertices() const { return verts; }

    // Индекс следующей / предыдущей вершины
    int next(int i) const { return (i+1) % (int)verts.size(); }
    int prev(int i) const { return (i-1+(int)verts.size()) % (int)verts.size(); }

    // ─── Свойства ────────────────────────────────────────────────────────────
    double signedArea() const;          // + = CCW
    double area()       const { return std::abs(signedArea()); }
    bool   isCCW()      const { return signedArea() > 0; }
    Point  centroid()   const;
    Rect   boundingBox() const { return Rect::fromPoints(verts); }

    // «Опорная точка» — левая нижняя вершина (для NFP)
    int    refVertexIndex() const;
    Point  refVertex()      const { return verts[refVertexIndex()]; }

    // ─── Нормализация ────────────────────────────────────────────────────────
    void makeCCW() { if (!isCCW()) std::reverse(verts.begin(), verts.end()); }
    void makeCW()  { if (isCCW())  std::reverse(verts.begin(), verts.end()); }
    void removeDuplicates(double tol = 1e-6);

    // ─── Трансформации ───────────────────────────────────────────────────────
    Polygon translated(double dx, double dy) const;
    Polygon translated(const Point& d) const { return translated(d.x, d.y); }
    Polygon rotatedAround(double angleDeg, const Point& pivot) const;
    Polygon rotated(double angleDeg) const { return rotatedAround(angleDeg, centroid()); }
    Polygon reflected() const; // (x,y)→(-x,-y)

    // ─── Запросы ─────────────────────────────────────────────────────────────
    bool containsPoint(const Point& p) const; // ray-casting

    // Пересечение двух отрезков (параметрически)
    static bool segmentsIntersect(const Point& a1, const Point& a2,
                                   const Point& b1, const Point& b2);

    // Пересечение полигонов через разбиение на треугольники
    bool intersects(const Polygon& other) const;

    // Минимальное расстояние между контурами
    double distanceTo(const Polygon& other) const;

    // ─── Выпуклая оболочка (Graham scan) ─────────────────────────────────────
    static Polygon convexHull(std::vector<Point> pts);
    Polygon toConvexHull() const { return convexHull(verts); }
    bool    isConvex()     const;

    // ─── Упрощение (Douglas-Peucker) ─────────────────────────────────────────
    Polygon simplified(double eps = 0.5) const;

    // ─── Minkowski sum двух ВЫПУКЛЫХ полигонов ────────────────────────────────
    // Оба должны быть CCW. O(n+m).
    static Polygon minkowskiSumConvex(Polygon A, Polygon B);

    // ═══════════════════════════════════════════════════════════════════════════
    // NFP (No-Fit Polygon) — ОРБИТАЛЬНЫЙ МЕТОД
    // ═══════════════════════════════════════════════════════════════════════════
    static std::vector<Polygon> computeNFP(const Polygon& fixed,
                                            const Polygon& moving);

    // ─── Inner Fit Rectangle ─────────────────────────────────────────────────
    static Rect innerFitRect(const Rect& sheet, const Polygon& part);
};

// ─── Свободные функции ────────────────────────────────────────────────────────
namespace GeoUtil {
    // Расстояние точки до отрезка
    double pointSegDist(const Point& p, const Point& a, const Point& b);

    // Угол вектора в [0, 2π)
    inline double vecAngle2PI(const Point& v) {
        double a = std::atan2(v.y, v.x);
        return a < 0 ? a + 2*M_PI : a;
    }

    // Знак с допуском
    inline int sign(double v, double eps = GEO_EPS) {
        return (v > eps) - (v < -eps);
    }
}
