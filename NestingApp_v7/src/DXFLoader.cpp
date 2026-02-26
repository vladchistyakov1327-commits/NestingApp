#include "DXFLoader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <unordered_map>
#include <cstdint>

static constexpr double PI = M_PI;

// ─── Строковые утилиты ────────────────────────────────────────────────────────
static std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    auto e = s.find_last_not_of(" \t\r\n");
    if (e != std::string::npos) s.erase(e+1);
    return s;
}
static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}
static double toD(const std::string& s) {
    // stod использует текущую локаль, но DXF всегда пишет точку.
    // Используем strtod с C-локалью через локальный setlocale не трогая глобальную.
    try {
        // Быстрый путь: пробуем stod — работает если локаль C/POSIX
        size_t pos;
        double v = std::stod(s, &pos);
        return v;
    } catch (...) {
        // Fallback: ручной парсинг через sscanf (всегда с точкой)
        double v = 0;
        std::sscanf(s.c_str(), "%lf", &v);
        return v;
    }
}
static int toI(const std::string& s) {
    try { return std::stoi(s); } catch (...) { return 0; }
}

// ─── Классификация слоёв ─────────────────────────────────────────────────────
bool DXFLoader::isCutLayer(const std::string& layer, int color) {
    std::string up = toUpper(layer);
    if (up.find("CUT")   != std::string::npos) return true;
    if (up.find("RED")   != std::string::npos) return true;
    if (up.find("ЛАЗЕР") != std::string::npos) return true;
    if (up == "0")  return true;
    if (color == 1) return true;
    if (color == 7) return true;
    return false;
}

bool DXFLoader::isMarkLayer(const std::string& layer, int color) {
    std::string up = toUpper(layer);
    if (up.find("MARK")    != std::string::npos) return true;
    if (up.find("ENGRAVE") != std::string::npos) return true;
    if (up.find("ГРАВИР")  != std::string::npos) return true;
    if (up.find("BLUE")    != std::string::npos) return true;
    if (color == 5) return true;
    return false;
}

// ─── ARC → точки ─────────────────────────────────────────────────────────────
std::vector<Point> DXFLoader::arcToPoints(const Point& center, double radius,
                                            double startDeg, double endDeg,
                                            int segments) {
    std::vector<Point> pts;
    if (radius <= 0) return pts;
    double end = endDeg;
    // Если start и end практически совпадают — это точка, не дуга
    // (в отличие от настоящей 360° окружности через CIRCLE)
    if (std::abs(end - startDeg) < GEO_EPS) return pts;
    if (end < startDeg) end += 360.0;
    double range = end - startDeg;
    if (range < GEO_EPS) return pts;
    pts.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        double ang = (startDeg + range * i / segments) * PI / 180.0;
        pts.push_back({center.x + radius * std::cos(ang),
                       center.y + radius * std::sin(ang)});
    }
    return pts;
}

// ─── Парсер DXF ──────────────────────────────────────────────────────────────
std::vector<DXFLoader::Entity> DXFLoader::parseEntities(std::istream& in) {
    std::vector<Entity> result;
    std::string gLine, vLine;

    bool inEntities = false;
    bool inVertex   = false;

    Entity cur;
    Entity polyline;

    // LINE: отдельные переменные для start/end
    double lx1=0, ly1=0, lx2=0, ly2=0;
    bool   lHasX1=false, lHasX2=false;

    // LWPOLYLINE: буфер текущей точки
    double lwX=0, lwY=0;
    bool   lwPending=false;

    // Текущая вершина VERTEX
    double vtxX=0, vtxY=0;
    bool   vtxHasX=false, vtxHasY=false;

    auto flushLW = [&]() {
        if (lwPending) {
            polyline.pts.push_back({lwX, lwY});
            lwPending = false;
        }
    };

    auto flushVtx = [&]() {
        if (vtxHasX && vtxHasY) {
            polyline.pts.push_back({vtxX, vtxY});
        }
        vtxHasX = vtxHasY = false;
        vtxX = vtxY = 0;
    };

    auto flushCurrent = [&]() {
        if (cur.type.empty()) return;
        if (cur.type == "LINE") {
            cur.pts.clear();
            cur.pts.push_back({lx1, ly1});
            cur.pts.push_back({lx2, ly2});
        }
        result.push_back(cur);
        cur = Entity{};
        lx1=ly1=lx2=ly2=0;
        lHasX1=lHasX2=false;
    };

    auto flushPolyline = [&]() {
        flushLW();
        flushVtx();
        if (!polyline.type.empty() && polyline.pts.size() >= 2)
            result.push_back(polyline);
        polyline = Entity{};
        inVertex = false;
    };

    while (std::getline(in, gLine) && std::getline(in, vLine)) {
        int   group = toI(trim(gLine));
        std::string val = trim(vLine);

        if (group == 0) {
            flushLW();

            if (val == "SEQEND") {
                // Конец POLYLINE — сбрасываем последнюю вершину и polyline
                flushVtx();
                flushPolyline();
                inVertex = false;
                continue;
            }

            if (inVertex) {
                // Конец текущей вершины — flush её
                flushVtx();
                inVertex = false;
            } else {
                flushCurrent();
            }

            if (val == "ENTITIES") { inEntities = true; }
            else if (val == "ENDSEC" || val == "EOF") {
                flushPolyline();
                inEntities = false;
            }
            else if (inEntities) {
                if (val == "VERTEX") {
                    inVertex = true;
                    vtxX = vtxY = 0;
                    vtxHasX = vtxHasY = false;
                } else if (val == "POLYLINE") {
                    // Начало нового POLYLINE — сбрасываем предыдущий
                    flushPolyline();
                    polyline.type = "POLYLINE";
                } else if (val == "LWPOLYLINE") {
                    flushPolyline();
                    polyline.type = "LWPOLYLINE";
                } else {
                    // Встретили новую сущность — сбросим polyline если был незакрытый
                    if (!polyline.type.empty()) flushPolyline();
                    cur.type = val;
                }
            }
            continue;
        }

        if (!inEntities) continue;

        auto applyCommon = [&](Entity& e) {
            if (group == 8)  e.layer = val;
            if (group == 62) e.color = toI(val);
            if (group == 70) {
                int flags = toI(val);
                e.closed = (flags & 1) != 0;
            }
        };

        if (!polyline.type.empty()) {
            // Внутри POLYLINE или LWPOLYLINE (включая VERTEX)
            if (!inVertex) applyCommon(polyline);

            if (polyline.type == "LWPOLYLINE") {
                if (group == 10) {
                    flushLW();
                    lwX = toD(val);
                    lwPending = true;
                } else if (group == 20 && lwPending) {
                    lwY = toD(val);
                    // Флашим сразу — у нас есть X и Y
                    flushLW();
                }
            } else if (polyline.type == "POLYLINE" && inVertex) {
                // VERTEX: группы 10=X, 20=Y — собираем вершину
                if (group == 10) {
                    vtxX    = toD(val);
                    vtxHasX = true;
                    vtxHasY = false; // сброс Y-флага для новой вершины
                    vtxY    = 0;
                } else if (group == 20) {
                    vtxY    = toD(val);
                    vtxHasY = true;
                }
            }
        } else if (!cur.type.empty()) {
            applyCommon(cur);

            if (cur.type == "LINE") {
                if      (group == 10) { lx1 = toD(val); lHasX1 = true; }
                else if (group == 20 && lHasX1) { ly1 = toD(val); }
                else if (group == 11) { lx2 = toD(val); lHasX2 = true; }
                else if (group == 21 && lHasX2) { ly2 = toD(val); }
            }
            else if (cur.type == "ARC" || cur.type == "CIRCLE") {
                if      (group == 10) { cur.pts.push_back({toD(val), 0}); }
                else if (group == 20 && !cur.pts.empty()) cur.pts.back().y = toD(val);
                else if (group == 40) cur.radius     = toD(val);
                else if (group == 50) cur.startAngle = toD(val);
                else if (group == 51) cur.endAngle   = toD(val);
            }
            else if (cur.type == "SPLINE") {
                if      (group == 10) { cur.pts.push_back({toD(val), 0}); }
                else if (group == 20 && !cur.pts.empty()) cur.pts.back().y = toD(val);
            }
        }
    }

    // Дофлашиваем всё что осталось
    flushLW();
    flushVtx();
    flushCurrent();
    flushPolyline();

    return result;
}

// ─── chainSegments ────────────────────────────────────────────────────────────
// Собирает замкнутые контуры из набора сегментов.
// Оптимизация O(n) через хеш-таблицу по округлённым координатам концов.
std::vector<Polygon> DXFLoader::chainSegments(std::vector<Segment> segs, double tol) {
    std::vector<Polygon> result;
    if (segs.empty()) return result;

    // Spatial hash: ключ = rounded(x/tol)*PRIME + rounded(y/tol)
    // Для каждой точки храним список индексов сегментов, чей конец там находится
    auto roundKey = [&](const Point& p) -> std::pair<int64_t, int64_t> {
        return {(int64_t)std::round(p.x / tol),
                (int64_t)std::round(p.y / tol)};
    };

    using Key = std::pair<int64_t, int64_t>;
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return std::hash<int64_t>()(k.first) * 2654435761ULL
                 ^ std::hash<int64_t>()(k.second);
        }
    };

    // endMap[key] = список {segIdx, bool reversed}
    // reversed=false → segs[i].second ≈ point, reversed=true → segs[i].first ≈ point
    std::unordered_map<Key, std::vector<std::pair<int,bool>>, KeyHash> endMap;
    endMap.reserve(segs.size() * 2);

    std::vector<bool> used(segs.size(), false);

    auto buildMap = [&]() {
        endMap.clear();
        for (int i = 0; i < (int)segs.size(); ++i) {
            if (used[i]) continue;
            endMap[roundKey(segs[i].first)].push_back({i, true});
            endMap[roundKey(segs[i].second)].push_back({i, false});
        }
    };
    buildMap();

    auto findNext = [&](const Point& tail) -> std::pair<int,bool> {
        auto it = endMap.find(roundKey(tail));
        if (it == endMap.end()) return {-1, false};
        for (auto& [idx, rev] : it->second) {
            if (!used[idx]) return {idx, rev};
        }
        return {-1, false};
    };

    // Помечаем использованный сегмент и убираем из endMap
    auto markUsed = [&](int i) {
        used[i] = true;
        // Убираем из обоих ключей
        for (int pass = 0; pass < 2; ++pass) {
            const Point& pt = (pass == 0) ? segs[i].first : segs[i].second;
            auto it = endMap.find(roundKey(pt));
            if (it == endMap.end()) continue;
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [i](const std::pair<int,bool>& p){ return p.first == i; }),
                vec.end());
        }
    };

    for (int startIdx = 0; startIdx < (int)segs.size(); ++startIdx) {
        if (used[startIdx]) continue;

        std::vector<Point> chain;
        chain.push_back(segs[startIdx].first);
        chain.push_back(segs[startIdx].second);
        markUsed(startIdx);

        // Наращиваем цепочку вперёд
        while (true) {
            auto [idx, rev] = findNext(chain.back());
            if (idx < 0) break;
            // rev=true → segs[idx].first совпадает с tail → добавляем second
            // rev=false → segs[idx].second совпадает с tail → добавляем first
            chain.push_back(rev ? segs[idx].second : segs[idx].first);
            markUsed(idx);
        }

        // Проверяем замкнутость
        if (chain.size() >= 4 &&
            chain.front().distanceTo(chain.back()) < tol * 2) {
            chain.pop_back(); // убираем дублированную начальную точку
            Polygon poly(chain);
            poly.removeDuplicates(tol * 0.5);
            if (poly.area() > 1.0)
                result.push_back(std::move(poly));
        }
    }

    return result;
}

// ─── buildContours ────────────────────────────────────────────────────────────
std::vector<Polygon> DXFLoader::buildContours(const std::vector<Entity>& ents,
                                               bool cutLayer) {
    std::vector<Polygon> result;
    std::vector<Segment> segments;

    for (const auto& e : ents) {
        bool isCut  = isCutLayer(e.layer, e.color);
        bool isMark = isMarkLayer(e.layer, e.color);

        if (!isCut && !isMark) {
            // Неклассифицированный — берём для CUT (fallback)
            if (!cutLayer) continue;
        } else {
            if (cutLayer  && !isCut)  continue;
            if (!cutLayer && !isMark) continue;
        }

        if (e.type == "LWPOLYLINE" || e.type == "POLYLINE") {
            if (e.pts.size() < 2) continue;
            if (e.closed && e.pts.size() >= 3) {
                Polygon poly(e.pts);
                poly.removeDuplicates();
                if (poly.area() > 1.0)
                    result.push_back(std::move(poly));
            } else {
                // Добавляем рёбра как сегменты — chainSegments сам замкнёт цепочку
                // если концы сходятся. Принудительное добавление back→front создаёт
                // ложный сегмент когда polyline реально незамкнут.
                for (size_t i = 0; i+1 < e.pts.size(); ++i)
                    segments.push_back({e.pts[i], e.pts[i+1]});
            }
        }
        else if (e.type == "LINE") {
            if (e.pts.size() == 2)
                segments.push_back({e.pts[0], e.pts[1]});
        }
        else if (e.type == "CIRCLE" && e.radius > 0) {
            Point center = e.pts.empty() ? Point{} : e.pts[0];
            auto pts = arcToPoints(center, e.radius, 0, 360, 36);
            if (!pts.empty()) pts.pop_back();
            Polygon poly(pts);
            poly.removeDuplicates();
            if (poly.area() > 1.0) result.push_back(std::move(poly));
        }
        else if (e.type == "ARC" && e.radius > 0) {
            Point center = e.pts.empty() ? Point{} : e.pts[0];
            auto pts = arcToPoints(center, e.radius, e.startAngle, e.endAngle, 24);
            for (size_t i = 0; i+1 < pts.size(); ++i)
                segments.push_back({pts[i], pts[i+1]});
        }
        else if (e.type == "SPLINE" && e.pts.size() >= 3) {
            for (size_t i = 0; i+1 < e.pts.size(); ++i)
                segments.push_back({e.pts[i], e.pts[i+1]});
        }
    }

    auto chained = chainSegments(std::move(segments));
    for (auto& p : chained)
        result.push_back(std::move(p));

    return result;
}

// ─── loadFile ────────────────────────────────────────────────────────────────
DXFLoader::LoadResult DXFLoader::loadFile(const std::string& filename) {
    LoadResult res;

    std::ifstream f(filename);
    if (!f) {
        res.warnings.push_back("Не удалось открыть: " + filename);
        return res;
    }

    auto entities = parseEntities(f);

    if (entities.empty()) {
        res.warnings.push_back("DXF не содержит поддерживаемых примитивов");
        return res;
    }

    res.cutContours  = buildContours(entities, true);
    res.markContours = buildContours(entities, false);

    // Fallback: нет CUT — берём все замкнутые контуры
    if (res.cutContours.empty()) {
        res.warnings.push_back("Слой CUT не найден. Используются все замкнутые контуры.");
        for (const auto& e : entities) {
            if ((e.type == "LWPOLYLINE" || e.type == "POLYLINE") &&
                e.closed && e.pts.size() >= 3) {
                Polygon poly(e.pts);
                poly.removeDuplicates();
                if (poly.area() > 1.0)
                    res.cutContours.push_back(std::move(poly));
            }
            if (e.type == "CIRCLE" && e.radius > 0) {
                Point c = e.pts.empty() ? Point{} : e.pts[0];
                auto pts = arcToPoints(c, e.radius, 0, 360, 36);
                if (!pts.empty()) pts.pop_back();
                Polygon poly(pts);
                poly.removeDuplicates();
                if (poly.area() > 1.0) res.cutContours.push_back(std::move(poly));
            }
        }
        // Если и теперь пусто — запускаем chainSegments на всех сегментах
        if (res.cutContours.empty()) {
            std::vector<DXFLoader::Segment> allSegs;
            for (const auto& e : entities) {
                if (e.type == "LINE" && e.pts.size() == 2)
                    allSegs.push_back({e.pts[0], e.pts[1]});
                if (e.type == "ARC" && e.radius > 0) {
                    Point c = e.pts.empty() ? Point{} : e.pts[0];
                    auto pts = arcToPoints(c, e.radius, e.startAngle, e.endAngle, 24);
                    for (size_t i = 0; i+1 < pts.size(); ++i)
                        allSegs.push_back({pts[i], pts[i+1]});
                }
            }
            auto chained = chainSegments(std::move(allSegs));
            for (auto& p : chained) res.cutContours.push_back(std::move(p));
        }
    }

    // Нормализуем
    auto cleanPolys = [&](std::vector<Polygon>& polys) {
        for (auto& poly : polys) {
            poly.makeCCW();
            poly.removeDuplicates();
        }
        polys.erase(std::remove_if(polys.begin(), polys.end(), [&](const Polygon& p) {
            bool deg = p.verts.size() < 3 || p.area() < 1.0;
            if (deg) res.warnings.push_back("Удалён вырожденный контур (площадь < 1мм²)");
            return deg;
        }), polys.end());
    };
    cleanPolys(res.cutContours);
    cleanPolys(res.markContours);

    res.success = !res.cutContours.empty();
    if (!res.success)
        res.warnings.push_back("Не найдено ни одного замкнутого контура");

    return res;
}
