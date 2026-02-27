#include "Geometry.h"
#include <numeric>
#include <cassert>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <iomanip>

static constexpr double PI2 = 2.0 * M_PI;

// ═══════════════════════════════════════════════════════════════════════════════
// Polygon: базовые свойства
// ═══════════════════════════════════════════════════════════════════════════════

double Polygon::signedArea() const {
    if (verts.size() < 3) return 0;
    double a = 0;
    for (size_t i = 0, n = verts.size(); i < n; ++i)
        a += verts[i].cross(verts[(i+1)%n]);
    return a * 0.5;
}

Point Polygon::centroid() const {
    if (verts.empty()) return {};
    double A = signedArea();
    if (std::abs(A) < GEO_EPS) {
        Point c;
        for (auto& v : verts) { c.x += v.x; c.y += v.y; }
        c.x /= verts.size(); c.y /= verts.size();
        return c;
    }
    double cx=0, cy=0;
    size_t n = verts.size();
    for (size_t i = 0; i < n; ++i) {
        double f = verts[i].cross(verts[(i+1)%n]);
        cx += (verts[i].x + verts[(i+1)%n].x) * f;
        cy += (verts[i].y + verts[(i+1)%n].y) * f;
    }
    return {cx / (6*A), cy / (6*A)};
}

// Опорная точка: самая левая из самых нижних
int Polygon::refVertexIndex() const {
    if (verts.empty()) return 0;
    int idx = 0;
    for (int i = 1; i < (int)verts.size(); ++i) {
        if (verts[i].y < verts[idx].y ||
            (verts[i].y == verts[idx].y && verts[i].x < verts[idx].x))
            idx = i;
    }
    return idx;
}

void Polygon::removeDuplicates(double tol) {
    if (verts.size() < 2) return;
    std::vector<Point> res;
    res.push_back(verts[0]);
    for (size_t i = 1; i < verts.size(); ++i)
        if (!verts[i].nearlyEqual(res.back(), tol))
            res.push_back(verts[i]);
    if (res.size() > 1 && res.back().nearlyEqual(res.front(), tol))
        res.pop_back();
    verts = std::move(res);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Трансформации
// ═══════════════════════════════════════════════════════════════════════════════

Polygon Polygon::translated(double dx, double dy) const {
    Polygon r = *this;
    for (auto& p : r.verts) { p.x += dx; p.y += dy; }
    return r;
}

Polygon Polygon::rotatedAround(double angleDeg, const Point& pivot) const {
    double rad = angleDeg * M_PI / 180.0;
    double c = std::cos(rad), s = std::sin(rad);
    Polygon r = *this;
    for (auto& p : r.verts) {
        double x = p.x - pivot.x, y = p.y - pivot.y;
        p.x = x*c - y*s + pivot.x;
        p.y = x*s + y*c + pivot.y;
    }
    return r;
}

Polygon Polygon::reflected() const {
    Polygon r = *this;
    for (auto& p : r.verts) { p.x = -p.x; p.y = -p.y; }
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Запросы
// ═══════════════════════════════════════════════════════════════════════════════

bool Polygon::containsPoint(const Point& p) const {
    bool inside = false;
    int n = (int)verts.size();
    for (int i = 0, j = n-1; i < n; j = i++) {
        if (((verts[i].y > p.y) != (verts[j].y > p.y)) &&
            (p.x < (verts[j].x-verts[i].x)*(p.y-verts[i].y)/(verts[j].y-verts[i].y)+verts[i].x))
            inside = !inside;
    }
    return inside;
}

bool Polygon::segmentsIntersect(const Point& a1, const Point& a2,
                                  const Point& b1, const Point& b2) {
    Point da = a2-a1, db = b2-b1;
    double denom = da.cross(db);
    if (std::abs(denom) < GEO_EPS) return false;

    double t = (b1-a1).cross(db) / denom;
    double u = (b1-a1).cross(da) / denom;
    return t > GEO_EPS && t < 1-GEO_EPS &&
           u > GEO_EPS && u < 1-GEO_EPS;
}

bool Polygon::intersects(const Polygon& other) const {
    if (empty() || other.empty()) return false;

    Rect bb1 = boundingBox(), bb2 = other.boundingBox();
    if (!bb1.intersects(bb2)) return false;

    // ИСПРАВЛЕНИЕ: быстрая проверка на совпадение bbox с высокой точностью
    // (идентичные полигоны наложены — рёбра параллельны, segmentsIntersect их не видит)
    if (std::abs(bb1.x - bb2.x) < 0.5 && std::abs(bb1.y - bb2.y) < 0.5 &&
        std::abs(bb1.w - bb2.w) < 0.5 && std::abs(bb1.h - bb2.h) < 0.5) {
        // bbox почти совпадают — считаем что полигоны перекрываются
        return true;
    }

    int n1 = (int)verts.size(), n2 = (int)other.verts.size();
    for (int i = 0; i < n1; ++i)
        for (int j = 0; j < n2; ++j)
            if (segmentsIntersect(verts[i], verts[(i+1)%n1],
                                   other.verts[j], other.verts[(j+1)%n2]))
                return true;

    // Проверяем точку внутри полигона (один может быть внутри другого)
    if (!verts.empty()       && other.containsPoint(verts[0]))       return true;
    if (!other.verts.empty() && containsPoint(other.verts[0]))       return true;

    // Проверяем несколько точек для надёжности при невыпуклых полигонах
    for (int i = 1; i < std::min(n1, 5); ++i)
        if (other.containsPoint(verts[i])) return true;
    for (int j = 1; j < std::min(n2, 5); ++j)
        if (containsPoint(other.verts[j])) return true;

    return false;
}

double GeoUtil::pointSegDist(const Point& p, const Point& a, const Point& b) {
    Point ab = b-a;
    double len2 = ab.lengthSq();
    if (len2 < GEO_EPS*GEO_EPS) return p.distanceTo(a);
    double t = std::clamp((p-a).dot(ab)/len2, 0.0, 1.0);
    return p.distanceTo(a + ab*t);
}

double Polygon::distanceTo(const Polygon& other) const {
    if (intersects(other)) return 0;
    double minDist = 1e18;
    int n1 = (int)verts.size(), n2 = (int)other.verts.size();
    for (int i = 0; i < n1; ++i)
        for (int j = 0; j < n2; ++j) {
            minDist = std::min(minDist, GeoUtil::pointSegDist(verts[i], other.verts[j], other.verts[(j+1)%n2]));
            minDist = std::min(minDist, GeoUtil::pointSegDist(other.verts[j], verts[i], verts[(i+1)%n1]));
        }
    return minDist;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Convex Hull — Graham scan
// ═══════════════════════════════════════════════════════════════════════════════

Polygon Polygon::convexHull(std::vector<Point> pts) {
    int n = (int)pts.size();
    if (n < 3) return Polygon(pts);

    int bot = 0;
    for (int i = 1; i < n; ++i)
        if (pts[i].y < pts[bot].y || (pts[i].y == pts[bot].y && pts[i].x < pts[bot].x))
            bot = i;
    std::swap(pts[0], pts[bot]);
    Point pivot = pts[0];

    std::sort(pts.begin()+1, pts.end(), [&](const Point& a, const Point& b) {
        Point da = a-pivot, db = b-pivot;
        double c = da.cross(db);
        if (std::abs(c) > GEO_EPS) return c > 0;
        return da.lengthSq() < db.lengthSq();
    });

    std::vector<Point> hull;
    hull.reserve(n);
    for (auto& p : pts) {
        while (hull.size() >= 2) {
            Point a = hull[hull.size()-2], b = hull[hull.size()-1];
            if ((b-a).cross(p-a) <= GEO_EPS) hull.pop_back();
            else break;
        }
        hull.push_back(p);
    }
    return Polygon(hull);
}

bool Polygon::isConvex() const {
    int n = (int)verts.size();
    if (n < 3) return false;
    int sign = 0;
    for (int i = 0; i < n; ++i) {
        double c = (verts[(i+1)%n]-verts[i]).cross(verts[(i+2)%n]-verts[(i+1)%n]);
        if (std::abs(c) < GEO_EPS) continue;
        int s = c > 0 ? 1 : -1;
        if (sign == 0) sign = s;
        else if (s != sign) return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Douglas-Peucker
// ═══════════════════════════════════════════════════════════════════════════════

static void dpStep(const std::vector<Point>& pts, int s, int e, double eps,
                    std::vector<bool>& keep) {
    if (e <= s+1) return;
    double maxD = 0; int idx = s;
    Point a = pts[s], b = pts[e];
    Point ab = b-a; double len = ab.length();
    for (int i = s+1; i < e; ++i) {
        double d = len < GEO_EPS ? (pts[i]-a).length()
                                 : std::abs(ab.cross(pts[i]-a)) / len;
        if (d > maxD) { maxD = d; idx = i; }
    }
    if (maxD > eps) {
        keep[idx] = true;
        dpStep(pts, s, idx, eps, keep);
        dpStep(pts, idx, e, eps, keep);
    }
}

Polygon Polygon::simplified(double eps) const {
    int n = (int)verts.size();
    if (n <= 4) return *this;
    std::vector<bool> keep(n, false);
    keep[0] = keep[n-1] = true;
    dpStep(verts, 0, n-1, eps, keep);
    std::vector<Point> res;
    for (int i = 0; i < n; ++i)
        if (keep[i]) res.push_back(verts[i]);
    return Polygon(res);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Minkowski Sum (выпуклые, оба CCW)
// ═══════════════════════════════════════════════════════════════════════════════

Polygon Polygon::minkowskiSumConvex(Polygon A, Polygon B) {
    A.makeCCW(); B.makeCCW();

    auto botIdx = [](const Polygon& P) {
        int idx = 0;
        for (int i = 1; i < (int)P.verts.size(); ++i)
            if (P.verts[i].y < P.verts[idx].y ||
                (P.verts[i].y == P.verts[idx].y && P.verts[i].x < P.verts[idx].x))
                idx = i;
        return idx;
    };

    int ia = botIdx(A), ib = botIdx(B);
    int na = (int)A.verts.size(), nb = (int)B.verts.size();

    std::vector<Point> result;
    result.reserve(na + nb);

    int i = 0, j = 0;
    while (i < na || j < nb) {
        int ci = std::min(i, na-1), cj = std::min(j, nb-1);
        result.push_back(A.verts[(ia+ci)%na] + B.verts[(ib+cj)%nb]);
        Point ea = A.verts[(ia+ci+1)%na] - A.verts[(ia+ci)%na];
        Point eb = B.verts[(ib+cj+1)%nb] - B.verts[(ib+cj)%nb];
        double c = ea.cross(eb);
        if      (i >= na)      ++j;
        else if (j >= nb)      ++i;
        else if (c > GEO_EPS)  ++i;
        else if (c < -GEO_EPS) ++j;
        else               { ++i; ++j; }
    }

    Polygon r(result);
    r.removeDuplicates();
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════════
// NFP — ОРБИТАЛЬНЫЙ МЕТОД
// ═══════════════════════════════════════════════════════════════════════════════
//
// Для ВЫПУКЛЫХ полигонов: точный орбитальный алгоритм.
// Для НЕВЫПУКЛЫХ: гибридный подход:
//   1. Пробуем орбитальный метод напрямую (работает для «почти выпуклых»)
//   2. Если результат вырожденный — fallback на convexHull с логированием
//
// Консервативный fallback: NFP от convexHull всегда перекрывает «лишние»
// позиции (запрещает больше чем нужно), что безопасно — никакого пересечения
// деталей не произойдёт, просто некоторые позиции будут недоступны.
// ═══════════════════════════════════════════════════════════════════════════════

static double edgeAngle2PI(const Point& v) {
    double a = std::atan2(v.y, v.x);
    return a < 0 ? a + PI2 : a;
}

// Орбитальный NFP для двух ВЫПУКЛЫХ полигонов (точный, O(n+m))
static Polygon nfpConvexOrbital(const Polygon& A, const Polygon& B) {
    int na = (int)A.verts.size(), nb = (int)B.verts.size();
    if (na < 3 || nb < 3) return {};

    // Нижняя вершина
    auto botIdx = [](const Polygon& P) {
        int idx = 0;
        for (int i = 1; i < (int)P.verts.size(); ++i)
            if (P.verts[i].y < P.verts[idx].y ||
               (P.verts[i].y == P.verts[idx].y && P.verts[i].x < P.verts[idx].x))
                idx = i;
        return idx;
    };

    int startA = botIdx(A);
    int startB = botIdx(B);

    // Стартовая позиция опорной точки moving
    Point startPos = A.verts[startA] - B.verts[startB];

    // Рёберные векторы: fixed CCW + moving с отрицательными рёбрами
    struct EdgeVec { Point v; double angle; int poly; };
    std::vector<EdgeVec> edges;
    edges.reserve(na + nb);

    for (int i = 0; i < na; ++i) {
        Point v = A.verts[(startA+i+1)%na] - A.verts[(startA+i)%na];
        if (v.lengthSq() < GEO_EPS * GEO_EPS) continue;
        edges.push_back({v, edgeAngle2PI(v), 0});
    }
    for (int j = 0; j < nb; ++j) {
        Point v = -(B.verts[(startB+j+1)%nb] - B.verts[(startB+j)%nb]);
        if (v.lengthSq() < GEO_EPS * GEO_EPS) continue;
        edges.push_back({v, edgeAngle2PI(v), 1});
    }

    // Сортируем по углу
    std::sort(edges.begin(), edges.end(), [](const EdgeVec& a, const EdgeVec& b) {
        if (std::abs(a.angle - b.angle) > 1e-9) return a.angle < b.angle;
        return a.poly < b.poly;
    });

    // Строим NFP
    std::vector<Point> nfpVerts;
    nfpVerts.reserve(na + nb + 1);
    nfpVerts.push_back(startPos);
    Point cur = startPos;

    for (auto& e : edges) {
        cur = cur + e.v;
        nfpVerts.push_back(cur);
    }

    Polygon nfp(nfpVerts);
    nfp.removeDuplicates();
    return nfp;
}

std::vector<Polygon> Polygon::computeNFP(const Polygon& fixed, const Polygon& moving) {
    Polygon A = fixed;  A.makeCCW(); A.removeDuplicates();
    Polygon B = moving; B.makeCCW(); B.removeDuplicates();

    if (A.verts.size() < 3 || B.verts.size() < 3) return {};

    bool aConvex = A.isConvex();
    bool bConvex = B.isConvex();

    if (aConvex && bConvex) {
        // Оба выпуклые — точный орбитальный алгоритм
        Polygon nfp = nfpConvexOrbital(A, B);
        nfp.makeCCW();
        return {nfp};
    }

    // ─── Невыпуклые детали: Minkowski sum через декомпозицию ─────────────────
    // 
    // Стратегия: разложим каждый полигон на выпуклые части (fan triangulation),
    // вычислим NFP для каждой пары выпуклых частей, объединим через выпуклую
    // оболочку всех результирующих вершин.
    // Это КОНСЕРВАТИВНОЕ приближение (запрещает несколько больше позиций),
    // но гарантирует отсутствие пересечений деталей.
    //
    // Ключевое отличие от предыдущей версии:
    //   - Стартовая точка вычисляется правильно для каждой выпуклой части
    //   - Итоговый NFP строится как объединение всех частичных NFP
    //   - Нет риска получить слишком маленький NFP который не перекрывает занятые позиции

    // Функция: выпуклая декомпозиция через fan triangulation от центроида
    auto fanDecompose = [](const Polygon& P) -> std::vector<Polygon> {
        int n = (int)P.verts.size();
        if (n < 3) return {};
        if (P.isConvex()) return {P};

        Point c = P.centroid();
        std::vector<Polygon> parts;
        for (int i = 0; i < n; ++i) {
            Polygon tri;
            tri.verts = {c, P.verts[i], P.verts[(i+1)%n]};
            if (tri.area() > GEO_EPS) {
                tri.makeCCW();
                parts.push_back(tri);
            }
        }
        return parts;
    };

    auto partsA = fanDecompose(A);
    auto partsB = fanDecompose(B);

    if (partsA.empty()) partsA = {A};
    if (partsB.empty()) partsB = {B};

    // Вычисляем NFP для всех пар выпуклых частей
    std::vector<Point> allNFPVerts;

    for (const auto& pa : partsA) {
        for (const auto& pb : partsB) {
            Polygon paConv = pa.toConvexHull();
            Polygon pbConv = pb.toConvexHull();
            paConv.makeCCW(); pbConv.makeCCW();
            if (paConv.verts.size() < 3 || pbConv.verts.size() < 3) continue;

            Polygon partNFP = nfpConvexOrbital(paConv, pbConv);
            for (const auto& v : partNFP.verts)
                allNFPVerts.push_back(v);
        }
    }

    if (allNFPVerts.empty()) {
        // Последний fallback: просто convexHull всего
        std::cerr << "[NFP] WARN: все частичные NFP пусты, fallback convexHull\n";
        Polygon Ah = A.toConvexHull();
        Polygon Bh = B.toConvexHull();
        Ah.makeCCW(); Bh.makeCCW();
        Polygon nfp = nfpConvexOrbital(Ah, Bh);
        nfp.makeCCW();
        return {nfp};
    }

    // Строим выпуклую оболочку всех вершин частичных NFP
    // Это консервативная оценка «зоны запрета»
    Polygon nfp = Polygon::convexHull(allNFPVerts);
    nfp.makeCCW();

    if (nfp.empty()) {
        std::cerr << "[NFP] WARN: итоговый NFP пуст для невыпуклых деталей\n";
        return {};
    }

    return {nfp};
}

Rect Polygon::innerFitRect(const Rect& sheet, const Polygon& part) {
    Rect pbb = part.boundingBox();
    double x = sheet.x;
    double y = sheet.y;
    double w = sheet.w - pbb.w;
    double h = sheet.h - pbb.h;
    if (w < 0 || h < 0) return {};
    return {x, y, w, h};
}
