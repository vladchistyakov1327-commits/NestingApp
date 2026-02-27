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
        if (!bb.intersects(pbb)) continue;
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
// ИСПРАВЛЕНИЯ:
//  1. Счётчики диагностики (вывод в cerr при verboseLog или для 1-й детали на листе)
//  2. Шаг сетки уменьшен с /20 до /10 для надёжности
//  3. Лимит MAX_CANDIDATES_PER_ANGLE = 3000 — защита от зависания
//  4. Early-exit: если нашли хорошую позицию с малым y — пропускаем оставшиеся
Sheet::Placement Sheet::findBestPlacement(
    const Part& part,
    const std::vector<double>& angles,
    NFPCache& cache
) const {
    Placement best{{0,0}, 0, false};
    double bestScore = std::numeric_limits<double>::max();

    // Диагностика: для первой детали на чистом листе — всегда, иначе при verboseLog
    bool doDiag = verboseLog || placed.empty();

    for (double angle : angles) {
        Rect ifr = getIFR(part, angle);
        if (!ifr.isValid()) continue;

        Polygon partShape = part.transformedShape(angle);

        // NFP для всех уже размещённых деталей
        std::vector<Polygon> nfps;
        nfps.reserve(placed.size());
        for (const auto& pp : placed) {
            Polygon nfp = getNFP(pp, part, angle, cache);
            if (!nfp.empty())
                nfps.push_back(std::move(nfp));
        }

        // Кандидаты: точки NFP + углы IFR + регулярная сетка
        auto candidates = getCandidatePoints(ifr, nfps);

        if (ifr.w > GEO_EPS && ifr.h > GEO_EPS) {
            // Адаптивный шаг сетки: мельче чем раньше для надёжности
            double step = std::min(ifr.w, ifr.h) / 10.0;
            step = std::max(step, 0.5); // не меньше 0.5 мм
            for (double y = ifr.y; y <= ifr.bottom() + GEO_EPS; y += step)
                for (double x = ifr.x; x <= ifr.right() + GEO_EPS; x += step)
                    candidates.push_back({x, y});
        }

        // Счётчики для диагностики
        int cntTotal = 0, cntInIFR = 0, cntPassNFP = 0, cntPassPlace = 0;
        constexpr int MAX_CANDIDATES_PER_ANGLE = 3000;

        for (const auto& pos : candidates) {
            if (!ifr.expanded(GEO_EPS).contains(pos)) continue;
            ++cntInIFR;
            ++cntTotal;

            // Лимит кандидатов — защита от зависания при плохом NFP
            if (cntTotal > MAX_CANDIDATES_PER_ANGLE) {
                if (doDiag)
                    std::cerr << "[WARN] findBestPlacement: лимит кандидатов "
                              << MAX_CANDIDATES_PER_ANGLE << " достигнут! угол=" << angle << "\n";
                break;
            }

            // Быстрая проверка NFP (без построения контура)
            bool inNFP = false;
            for (const auto& nfp : nfps) {
                if (nfp.containsPoint(pos)) { inNFP = true; break; }
            }
            if (inNFP) continue;
            ++cntPassNFP;

            // Точная проверка геометрии
            Polygon shapeAtPos = partShape.translated(pos.x, pos.y);
            if (!canPlace(shapeAtPos)) continue;
            ++cntPassPlace;

            double score = pos.y * 1e7 + pos.x;
            if (score < bestScore) {
                bestScore = score;
                best = {pos, angle, true};
            }
        }

        if (doDiag) {
            std::cerr << "[DIAG] угол=" << angle
                      << " nfps=" << nfps.size()
                      << " кандидатов=" << cntTotal
                      << " inIFR=" << cntInIFR
                      << " passNFP=" << cntPassNFP
                      << " passCanPlace=" << cntPassPlace
                      << (best.valid ? " OK" : " FAIL") << "\n";
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
