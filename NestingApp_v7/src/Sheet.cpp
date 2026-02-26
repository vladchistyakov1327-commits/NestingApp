#include "Sheet.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// Объявление вперёд
static Polygon addGapToPolygon(const Polygon& p, double dist);

// ─── Ключи кеша ───────────────────────────────────────────────────────────────
std::string Sheet::nfpKeySheet(int partId, double angle) {
    std::ostringstream s;
    s << "s_" << partId << "_" << std::fixed << std::setprecision(1) << angle;
    return s.str();
}

std::string Sheet::nfpKeyParts(int idA, double angA, int idB, double angB) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(1)
      << idA << "_" << angA << "__" << idB << "_" << angB;
    return s.str();
}

// ─── IFR (Inner Fit Rect) ────────────────────────────────────────────────────
// Допустимая область для позиции (нижний-левый угол bbox) новой детали
Rect Sheet::getIFR(const Part& part, double angle) const {
    Rect ua  = usableArea();
    Rect pbb = part.boundingBoxRotated(angle);

    double w = ua.w - pbb.w;
    double h = ua.h - pbb.h;
    if (w < -GEO_EPS || h < -GEO_EPS) return {}; // не влезает

    return {ua.x, ua.y, std::max(0.0, w), std::max(0.0, h)};
}

// ─── getNFP ───────────────────────────────────────────────────────────────────
// Возвращает NFP(placed_shape, moving) в ГЛОБАЛЬНЫХ координатах листа.
// Ключ кеша — локальные координаты (не зависят от позиции placed детали).
Polygon Sheet::getNFP(const PlacedPart& pp, const Part& moving,
                       double movingAngle, NFPCache& cache) const {
    std::string key = nfpKeyParts(pp.partId, pp.angle, moving.id, movingAngle);
    Rect pBB = pp.shape.boundingBox();

    auto it = cache.find(key);
    if (it != cache.end()) {
        // Кешировано в локальных координатах → смещаем в глобальные
        return it->second.translated(pBB.x, pBB.y);
    }

    // fixed = placed shape нормализованный (bbox.lower_left = 0,0)
    Polygon fixedLocal = pp.shape.translated(-pBB.x, -pBB.y);

    // moving = трансформированный контур (уже нормализован)
    Polygon movingLocal = moving.transformedShape(movingAngle);

    // Расширяем fixed на gap для учёта минимального зазора
    if (gap > GEO_EPS)
        fixedLocal = addGapToPolygon(fixedLocal, gap);

    auto nfps  = Polygon::computeNFP(fixedLocal, movingLocal);
    Polygon nfp = nfps.empty() ? Polygon{} : nfps[0];

    // NFP задаёт позиции refVertex moving'а (нижняя-левая вершина формы).
    // Нам нужна позиция bbox.lower_left = (0,0) после normalize.
    // Смещаем NFP так чтобы он давал позицию (0,0)-точки (= pos при размещении).
    Point refV = movingLocal.refVertex();
    nfp = nfp.translated(-refV.x, -refV.y);

    cache[key] = nfp; // в bbox.lower_left системе координат
    return nfp.translated(pBB.x, pBB.y);
}

// Оффсет контура наружу на dist (через аппроксимацию через нормали рёбер)
static Polygon addGapToPolygon(const Polygon& p, double dist) {
    // Простой подход: сдвигаем каждое ребро наружу, пересекаем соседние
    int n = (int)p.verts.size();
    if (n < 3) return p;

    // Убедимся что CCW (нормаль наружу = влево от ребра CCW = правостороннее)
    Polygon cp = p;
    cp.makeCCW();

    std::vector<Point> result;
    result.reserve(n);

    for (int i = 0; i < n; ++i) {
        const Point& a = cp.verts[i];
        const Point& b = cp.verts[cp.next(i)];
        // Нормаль к ребру наружу для CCW:
        // Ребро идёт из a в b: edge = (dx, dy)
        // Правая нормаль (наружу для CCW) = (-dy, dx)
        Point edge = b - a;
        double len = edge.length();
        if (len < GEO_EPS) continue;
        Point normal = Point{-edge.y, edge.x} * (dist / len);
        result.push_back(a + normal);
        result.push_back(b + normal);
    }

    if (result.empty()) return p;

    // Берём выпуклую оболочку оффсетных точек (консервативно)
    Polygon hull = Polygon::convexHull(result);
    hull.makeCCW();
    return hull;
}

// ─── Кандидаты на позицию ───────────────────────────────────────────────────
// Bottom-Left NFP: кандидаты — точки на контурах NFP, граничащие с IFR,
// плюс угловые точки IFR. Это все «особые» точки, где деталь будет
// касаться чего-либо (стены листа или другой детали).
std::vector<Point> Sheet::getCandidatePoints(
    const Rect& ifr,
    const std::vector<Polygon>& nfps
) const {
    std::vector<Point> candidates;

    // Углы IFR
    candidates.push_back({ifr.x,       ifr.y});
    candidates.push_back({ifr.right(),  ifr.y});
    candidates.push_back({ifr.x,        ifr.bottom()});
    candidates.push_back({ifr.right(),  ifr.bottom()});

    // Точки на контурах NFP, попадающие внутрь IFR
    for (const auto& nfp : nfps) {
        for (const auto& v : nfp.verts) {
            if (ifr.expanded(GEO_EPS).contains(v))
                candidates.push_back(v);
        }
        // Пересечения рёбер NFP с границей IFR
        // (упрощение: просто добавляем все вершины NFP — достаточно для практики)
    }

    return candidates;
}

// ─── canPlace ─────────────────────────────────────────────────────────────────
// Проверяем что трансформированный контур (уже в глобальных координатах):
//  - целиком внутри рабочей области
//  - не пересекается ни с одной уже размещённой деталью (с учётом gap)
bool Sheet::canPlace(const Polygon& shape) const {
    Rect ua = usableArea();
    Rect bb = shape.boundingBox();

    // Быстрая проверка AABB
    if (bb.x < ua.x - GEO_EPS || bb.y < ua.y - GEO_EPS ||
        bb.right()  > ua.right()  + GEO_EPS ||
        bb.bottom() > ua.bottom() + GEO_EPS)
        return false;

    // Точная проверка всех вершин внутри рабочей области
    for (const auto& v : shape.verts)
        if (!ua.expanded(GEO_EPS).contains(v))
            return false;

    // Для невыпуклых деталей: проверяем что рёбра не выходят за пределы листа.
    // Достаточно проверить пересечение каждого ребра с границами ua.
    // (Если обе вершины внутри и контур выпуклый — проверка уже выполнена выше.
    //  Для вогнутых — ребро может выступать наружу при вершинах внутри.)
    {
        // Четыре границы листа как отрезки
        const Point corners[4] = {
            {ua.x, ua.y}, {ua.right(), ua.y},
            {ua.right(), ua.bottom()}, {ua.x, ua.bottom()}
        };
        int n = (int)shape.verts.size();
        for (int i = 0; i < n; ++i) {
            const Point& a = shape.verts[i];
            const Point& b = shape.verts[(i+1)%n];
            for (int e = 0; e < 4; ++e) {
                if (Polygon::segmentsIntersect(a, b, corners[e], corners[(e+1)%4]))
                    return false;
            }
        }
    }

    // Проверяем пересечение с каждой размещённой деталью.
    // Gap уже учтён в NFP (addGapToPolygon применяется к fixed перед computeNFP),
    // поэтому здесь проверяем только физическое пересечение контуров.
    for (const auto& pp : placed) {
        // Быстрая AABB
        Rect pbb = pp.shape.boundingBox();
        if (!bb.intersects(pbb)) continue;

        // Точное пересечение
        if (shape.intersects(pp.shape)) return false;
    }
    return true;
}

// ─── isValidPosition ─────────────────────────────────────────────────────────
bool Sheet::isValidPosition(const Point& pos, const Rect& ifr,
                              const std::vector<Polygon>& nfps,
                              const Polygon& shape) const {
    // 1. Позиция внутри IFR
    if (!ifr.expanded(GEO_EPS).contains(pos)) return false;

    // 2. Позиция не внутри ни одного NFP (NFP = запрещённая зона)
    for (const auto& nfp : nfps) {
        if (nfp.containsPoint(pos)) return false;
    }

    // 3. Точная проверка контура
    return canPlace(shape);
}

// ─── findBestPlacement ────────────────────────────────────────────────────────
Sheet::Placement Sheet::findBestPlacement(
    const Part& part,
    const std::vector<double>& angles,
    NFPCache& cache
) const {
    Placement best{{0,0}, 0, false};
    // Bottom-Left: минимизируем (y * bigNumber + x)
    double bestScore = std::numeric_limits<double>::max();

    for (double angle : angles) {
        // IFR для этого угла
        Rect ifr = getIFR(part, angle);
        if (!ifr.isValid()) continue;

        // Трансформированный контур детали (нормализован, в (0,0))
        Polygon partShape = part.transformedShape(angle);

        // Собираем NFP для всех уже размещённых деталей через getNFP
        std::vector<Polygon> nfps;
        nfps.reserve(placed.size());
        for (const auto& pp : placed) {
            Polygon nfp = getNFP(pp, part, angle, cache);
            if (!nfp.empty())
                nfps.push_back(std::move(nfp));
        }

        // Кандидаты на позицию
        auto candidates = getCandidatePoints(ifr, nfps);

        // Добавляем grid-сетку для надёжности (грубая, шаг = мин. сторона / 20)
        if (ifr.w > GEO_EPS && ifr.h > GEO_EPS) {
            double step = std::min(ifr.w, ifr.h) / 20.0;
            step = std::max(step, 1.0); // не меньше 1мм
            for (double y = ifr.y; y <= ifr.bottom()+GEO_EPS; y += step)
                for (double x = ifr.x; x <= ifr.right()+GEO_EPS; x += step)
                    candidates.push_back({x, y});
        }

        // Перебираем кандидатов — ищем наилучший (Bottom-Left)
        for (const auto& pos : candidates) {
            if (!ifr.expanded(GEO_EPS).contains(pos)) continue;

            // Строим трансформированный контур в позиции pos
            Polygon shapeAtPos = partShape.translated(pos.x, pos.y);

            if (isValidPosition(pos, ifr, nfps, shapeAtPos)) {
                double score = pos.y * 1e7 + pos.x;
                if (score < bestScore) {
                    bestScore = score;
                    best = {pos, angle, true};
                }
                // Нашли допустимую позицию для этого угла — достаточно
                // (уже Bottom-Left, дальше только хуже по y)
                // Но продолжаем — могут быть кандидаты с меньшим y
            }
        }
    }

    return best;
}

// ─── place ────────────────────────────────────────────────────────────────────
void Sheet::place(const Part& part, const Point& pos, double angleDeg) {
    placed.push_back(part.place(pos, angleDeg));
}

// ─── Статистика ───────────────────────────────────────────────────────────────
double Sheet::placedArea() const {
    double a = 0;
    for (const auto& pp : placed) a += pp.shape.area();
    return a;
}

double Sheet::utilization() const {
    double ua = (width - 2*margin) * (height - 2*margin);
    return ua > 0 ? placedArea() / ua : 0;
}
