#include "Sheet.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

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
Rect Sheet::getIFR(const Part& part, double angle) const {
    Rect ua  = usableArea();
    Rect pbb = part.boundingBoxRotated(angle);

    double w = ua.w - pbb.w;
    double h = ua.h - pbb.h;
    if (w < -GEO_EPS || h < -GEO_EPS) return {};

    return {ua.x, ua.y, std::max(0.0, w), std::max(0.0, h)};
}

// ─── getNFP ───────────────────────────────────────────────────────────────────
Polygon Sheet::getNFP(const PlacedPart& pp, const Part& moving,
                       double movingAngle, NFPCache& cache) const {
    std::string key = nfpKeyParts(pp.partId, pp.angle, moving.id, movingAngle);
    Rect pBB = pp.shape.boundingBox();

    auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second.translated(pBB.x, pBB.y);
    }

    Polygon fixedLocal = pp.shape.translated(-pBB.x, -pBB.y);
    Polygon movingLocal = moving.transformedShape(movingAngle);

    if (gap > GEO_EPS)
        fixedLocal = addGapToPolygon(fixedLocal, gap);

    auto nfps  = Polygon::computeNFP(fixedLocal, movingLocal);
    Polygon nfp = nfps.empty() ? Polygon{} : nfps[0];

    if (!nfp.empty()) {
        Point refV = movingLocal.refVertex();
        nfp = nfp.translated(-refV.x, -refV.y);
    }

    cache[key] = nfp;
    return nfp.translated(pBB.x, pBB.y);
}

// ─── addGapToPolygon ─────────────────────────────────────────────────────────
// ИСПРАВЛЕНИЕ: убран convexHull — он уничтожал вогнутость контура.
// Теперь используем мягкий оффсет через усреднение нормалей соседних рёбер.
static Polygon addGapToPolygon(const Polygon& p, double dist) {
    int n = (int)p.verts.size();
    if (n < 3) return p;

    Polygon cp = p;
    cp.makeCCW();

    std::vector<Point> offsetVerts;
    offsetVerts.reserve(n);

    for (int i = 0; i < n; ++i) {
        int prev = (i - 1 + n) % n;
        int next = (i + 1) % n;
        const Point& A = cp.verts[prev];
        const Point& B = cp.verts[i];
        const Point& C = cp.verts[next];

        // Нормали к рёбрам AB и BC (для CCW нормаль наружу = левая нормаль ребра)
        Point e1 = B - A; double l1 = e1.length();
        Point e2 = C - B; double l2 = e2.length();

        if (l1 < GEO_EPS || l2 < GEO_EPS) {
            offsetVerts.push_back(B);
            continue;
        }

        // Нормаль к ребру (наружу для CCW): перпендикуляр влево от направления движения
        Point n1 = Point{-e1.y / l1,  e1.x / l1};
        Point n2 = Point{-e2.y / l2,  e2.x / l2};

        // Усредняем нормали и нормализуем
        Point avg = Point{(n1.x + n2.x) * 0.5, (n1.y + n2.y) * 0.5};
        double alen = avg.length();
        if (alen < GEO_EPS) avg = n1;
        else avg = avg * (1.0 / alen);

        offsetVerts.push_back(B + avg * dist);
    }

    if (offsetVerts.empty()) return p;

    Polygon result;
    result.verts = offsetVerts;
    result.removeDuplicates();
    if (result.verts.size() < 3) return p;
    result.makeCCW();
    return result;
}

// ─── Кандидаты на позицию ────────────────────────────────────────────────────
std::vector<Point> Sheet::getCandidatePoints(
    const Rect& ifr,
    const std::vector<Polygon>& nfps
) const {
    std::vector<Point> candidates;

    // Углы IFR (bottom-left эвристика — нижний левый угол приоритетный)
    candidates.push_back({ifr.x,         ifr.y});
    candidates.push_back({ifr.right(),    ifr.y});
    candidates.push_back({ifr.x,          ifr.bottom()});
    candidates.push_back({ifr.right(),    ifr.bottom()});

    // Точки на контурах NFP, попадающие в IFR
    for (const auto& nfp : nfps) {
        for (const auto& v : nfp.verts) {
            if (ifr.expanded(GEO_EPS).contains(v))
                candidates.push_back(v);
        }
    }

    return candidates;
}

// ─── canPlace ─────────────────────────────────────────────────────────────────
bool Sheet::canPlace(const Polygon& shape) const {
    Rect ua = usableArea();
    Rect bb = shape.boundingBox();

    if (bb.x < ua.x - GEO_EPS || bb.y < ua.y - GEO_EPS ||
        bb.right()  > ua.right()  + GEO_EPS ||
        bb.bottom() > ua.bottom() + GEO_EPS)
        return false;

    for (const auto& v : shape.verts)
        if (!ua.expanded(GEO_EPS).contains(v))
            return false;

    // Для невыпуклых деталей: проверяем рёбра на выход за пределы листа
    {
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

    for (const auto& pp : placed) {
        Rect pbb = pp.shape.boundingBox();
        // Быстрая AABB-проверка: если bbox'ы не пересекаются с запасом gap → пропуск O(1)
        if (!bb.intersects(pbb.expanded(gap + GEO_EPS))) continue;

        // Точная геометрическая проверка пересечения контуров O(n*m)
        // Зазор gap уже учтён в NFP через addGapToPolygon — здесь достаточно intersects
        if (shape.intersects(pp.shape)) return false;
    }
    return true;
}

// ─── isValidPosition ─────────────────────────────────────────────────────────
bool Sheet::isValidPosition(const Point& pos, const Rect& ifr,
                              const std::vector<Polygon>& nfps,
                              const Polygon& shape) const {
    if (!ifr.expanded(GEO_EPS).contains(pos)) return false;

    for (const auto& nfp : nfps) {
        if (nfp.containsPoint(pos)) return false;
    }

    return canPlace(shape);
}

// ─── findBestPlacement ────────────────────────────────────────────────────────
// Улучшения:
//  1. Early-exit: пустой лист → сразу нижний левый угол (оптимально для BL)
//  2. Сетка: шаг = max(gap, partBB/8), но не более MAX_GRID = 30x30 точек
//  3. NFP AABB: быстрая AABB-проверка перед containsPoint O(k) → O(1)
//  4. Early-exit: нашли позицию на минимальном y → пропускаем оставшиеся углы
//  5. Лимит кандидатов: 10000 для защиты от зависания
Sheet::Placement Sheet::findBestPlacement(
    const Part& part,
    const std::vector<double>& angles,
    NFPCache& cache
) const {
    Placement best{{0,0}, 0, false};
    double bestScore = std::numeric_limits<double>::max();

    bool doDiag = verboseLog;

    // ── Пустой лист: первая деталь всегда идёт в нижний левый угол ────────────
    if (placed.empty()) {
        for (double angle : angles) {
            Rect ifr = getIFR(part, angle);
            if (!ifr.isValid()) continue;
            Point pos{ifr.x, ifr.y};
            Polygon shapeAtPos = part.transformedShape(angle).translated(pos.x, pos.y);
            if (canPlace(shapeAtPos)) {
                best = {pos, angle, true};
                if (doDiag)
                    std::cerr << "[DIAG] пустой лист, угол=" << angle
                              << " → BL (" << pos.x << "," << pos.y << ")\n";
                return best;  // BL — глобально оптимально на пустом листе
            }
        }
        return best;
    }

    // ── Предвычисляем AABB NFP для быстрой фильтрации ─────────────────────────
    // Для каждого NFP храним его bbox → O(1) reject перед O(k) containsPoint
    struct NFPEntry { Polygon nfp; Rect bb; };

    for (double angle : angles) {
        Rect ifr = getIFR(part, angle);
        if (!ifr.isValid()) continue;

        Polygon partShape = part.transformedShape(angle);
        Rect partBB = partShape.boundingBox();

        // Строим NFP для всех размещённых деталей + их bbox
        std::vector<NFPEntry> nfpEntries;
        nfpEntries.reserve(placed.size());
        for (const auto& pp : placed) {
            Polygon nfp = getNFP(pp, part, angle, cache);
            if (!nfp.empty())
                nfpEntries.push_back({std::move(nfp), nfpEntries.empty()
                    ? Rect{} : nfpEntries.back().bb});
        }
        // Пересчитываем bbox для каждого NFP
        for (auto& e : nfpEntries)
            e.bb = e.nfp.boundingBox();

        // Собираем полигоны для getCandidatePoints (без изменений API)
        std::vector<Polygon> nfpPolys;
        nfpPolys.reserve(nfpEntries.size());
        for (auto& e : nfpEntries) nfpPolys.push_back(e.nfp);

        // Кандидаты: точки NFP + углы IFR
        auto candidates = getCandidatePoints(ifr, nfpPolys);

        // ── Сетка: адаптивный шаг ────────────────────────────────────────────
        // Шаг = max(gap, partBB/8), но сетка не более 30×30 точек
        if (ifr.w > GEO_EPS && ifr.h > GEO_EPS) {
            const int MAX_GRID = 30;
            double minStep = std::max(gap > GEO_EPS ? gap : 1.0,
                                      std::min(partBB.w, partBB.h) * 0.125);
            double stepX = std::max(minStep, ifr.w  / MAX_GRID);
            double stepY = std::max(minStep, ifr.h  / MAX_GRID);
            for (double y = ifr.y; y <= ifr.bottom() + GEO_EPS; y += stepY)
                for (double x = ifr.x; x <= ifr.right() + GEO_EPS; x += stepX)
                    candidates.push_back({x, y});
        }

        int cntTotal = 0, cntPassNFP = 0, cntPassPlace = 0;
        constexpr int MAX_CANDIDATES_PER_ANGLE = 10000;

        // Порог early-exit: позиция на y ≤ ifr.y + 1мм считается «идеальной»
        const double earlyExitY = ifr.y + 1.0;

        for (const auto& pos : candidates) {
            if (!ifr.expanded(GEO_EPS).contains(pos)) continue;
            if (++cntTotal > MAX_CANDIDATES_PER_ANGLE) {
                if (doDiag)
                    std::cerr << "[WARN] лимит кандидатов, угол=" << angle << "\n";
                break;
            }

            // ── NFP-проверка с AABB pre-filter ────────────────────────────────
            bool inNFP = false;
            for (const auto& e : nfpEntries) {
                // Быстрый AABB reject: если pos вне bbox NFP — не внутри O(1)
                if (!e.bb.expanded(GEO_EPS).contains(pos)) continue;
                if (e.nfp.containsPoint(pos)) { inNFP = true; break; }
            }
            if (inNFP) continue;
            ++cntPassNFP;

            // Точная геометрическая проверка
            Polygon shapeAtPos = partShape.translated(pos.x, pos.y);
            if (!canPlace(shapeAtPos)) continue;
            ++cntPassPlace;

            // BL-эвристика: y × 10 + x × 0.7
            double score = pos.y * 10.0 + pos.x * 0.7;
            if (score < bestScore) {
                bestScore = score;
                best = {pos, angle, true};

                // Early-exit: позиция у нижнего края — лучше не найти
                if (pos.y <= earlyExitY) break;
            }
        }

        if (doDiag) {
            std::cerr << "[DIAG] угол=" << angle
                      << " nfps=" << nfpEntries.size()
                      << " кандидатов=" << cntTotal
                      << " passNFP=" << cntPassNFP
                      << " passPlace=" << cntPassPlace
                      << (best.valid ? " OK" : " FAIL") << "\n";
        }

        // Early-exit между углами: нашли позицию на минимальном y → нет смысла
        // проверять остальные углы (только если она у самого дна IFR)
        if (best.valid && best.pos.y <= ifr.y + 1.0) break;
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
