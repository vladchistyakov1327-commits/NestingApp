#include "DXFLoader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <string>

static constexpr double PI = 3.141592653589793;
// ✅ Убрал дублирующий GEO_EPS - используем из Geometry.h

// ─── Строковые утилиты ────────────────────────────────────────────────────────
static std::string trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    size_t e = s.find_last_not_of(" \t\r\n");
    if (e != std::string::npos) s.erase(e + 1);
    return s;
}

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

static double toD(const std::string& s) {
    try {
        size_t pos;
        return std::stod(s, &pos);
    } catch (...) {
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
    if (up.find("CUT") != std::string::npos) return true;
    if (up.find("RED") != std::string::npos) return true;
    if (up.find("ЛАЗЕР") != std::string::npos) return true;
    if (up == "0") return true;
    if (color == 1 || color == 7) return true;
    return false;
}

bool DXFLoader::isMarkLayer(const std::string& layer, int color) {
    std::string up = toUpper(layer);
    if (up.find("MARK") != std::string::npos) return true;
    if (up.find("ENGRAVE") != std::string::npos) return true;
    if (up.find("ГРАВИР") != std::string::npos) return true;
    if (up.find("BLUE") != std::string::npos) return true;
    if (color == 5) return true;
    return false;
}

// ─── ARC/CIRCLE → точки ──────────────────────────────────────────────────────
std::vector<Point> DXFLoader::arcToPoints(const Point& center, double radius,
                                          double startDeg, double endDeg, int segments) {
    std::vector<Point> pts;
    if (radius <= 0) return pts;
    
    double end = endDeg;
    if (std::abs(end - startDeg) < GEO_EPS) return pts;
    if (end < startDeg) end += 360.0;
    double range = end - startDeg;
    if (range < GEO_EPS) return pts;
    
    pts.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        double ang = (startDeg + range * i / segments) * PI / 180.0;
        pts.emplace_back(center.x + radius * std::cos(ang),
                        center.y + radius * std::sin(ang));
    }
    return pts;
}

// ─── ✅ ИСПРАВЛЁННЫЙ ПАРСЕР ───────────────────────────────────────────────────
std::vector<DXFLoader::Entity> DXFLoader::parseEntities(std::istream& in) {
    std::vector<Entity> result;
    std::string gLine, vLine;
    
    bool inEntities = false;
    Entity current;
    
    while (std::getline(in, gLine) && std::getline(in, vLine)) {
        int group = toI(trim(gLine));
        std::string val = trim(vLine);
        
        // ✅ НОВАЯ СУЩНОСТЬ (group 0) - сохраняем предыдущую
        if (group == 0) {
            // Финализируем предыдущую сущность
            if (!current.type.empty()) {
                if ((current.type == "LINE" && current.pts.size() == 2) ||
                    ((current.type == "LWPOLYLINE" || current.type == "POLYLINE") && 
                     current.pts.size() >= 3) ||
                    (current.type == "ARC" && current.radius > 0) ||
                    (current.type == "CIRCLE" && current.radius > 0)) {
                    result.push_back(current);
                }
            }
            
            // ✅ Начинаем новую сущность
            current = Entity{};
            current.type = val;
            
            if (val == "SECTION") {
                inEntities = false;
            } else if (val == "ENTITIES") {
                inEntities = true;
            } else if (val == "ENDSEC" || val == "EOF" || val == "SEQEND") {
                inEntities = false;
            }
            continue;
        }
        
        // Пропускаем если не в секции ENTITIES
        if (!inEntities || current.type.empty()) continue;
        
        // ✅ Общие группы для всех сущностей
        if (group == 8)  current.layer = val;
        if (group == 62) current.color = toI(val);
        if (group == 70) {
            int flags = toI(val);
            current.closed = (flags & 1) != 0;
        }
        
        // ✅ ЛИНЕЙКИ (LINE)
        if (current.type == "LINE") {
            if (group == 10) {
                current.pts = {{toD(val), 0}};
            } else if (group == 20 && !current.pts.empty()) {
                current.pts[0].y = toD(val);
            } else if (group == 11 && current.pts.size() == 1) {
                current.pts.emplace_back(toD(val), 0);
            } else if (group == 21 && current.pts.size() == 2) {
                current.pts[1].y = toD(val);
            }
        }
        // ✅ LWPOLYLINE (самые частые!)
        else if (current.type == "LWPOLYLINE") {
            if (group == 10) {
                current.pts.emplace_back(toD(val), 0);
            } else if (group == 20 && !current.pts.empty()) {
                current.pts.back().y = toD(val);
            }
        }
        // ✅ POLYLINE + VERTEX
        else if (current.type == "POLYLINE") {
            if (val == "VERTEX") {
                // Новая вершина в текущем POLYLINE
                if (!current.pts.empty()) {
                    current.pts.emplace_back(0, 0);
                }
            } else if (!current.pts.empty()) {
                if (group == 10) {
                    current.pts.back().x = toD(val);
                } else if (group == 20) {
                    current.pts.back().y = toD(val);
                }
            }
        }
        // ✅ ДУГИ И ОКРУЖНОСТИ
        else if (current.type == "ARC") {
            if (group == 10) {
                current.pts = {{toD(val), 0}};
            } else if (group == 20 && !current.pts.empty()) {
                current.pts[0].y = toD(val);
            } else if (group == 40) current.radius = toD(val);
            else if (group == 50) current.startAngle = toD(val);
            else if (group == 51) current.endAngle = toD(val);
        }
        else if (current.type == "CIRCLE") {
            if (group == 10) {
                current.pts = {{toD(val), 0}};
            } else if (group == 20 && !current.pts.empty()) {
                current.pts[0].y = toD(val);
            } else if (group == 40) current.radius = toD(val);
        }
        // ✅ СПЛАЙНЫ (примитивная аппроксимация)
        else if (current.type == "SPLINE") {
            if (group == 10) {
                current.pts.emplace_back(toD(val), 0);
            } else if (group == 20 && !current.pts.empty()) {
                current.pts.back().y = toD(val);
            }
        }
    }
    
    // ✅ Последняя сущность
    if (!current.type.empty() && 
        ((current.type == "LINE" && current.pts.size() == 2) ||
         ((current.type == "LWPOLYLINE" || current.type == "POLYLINE") && current.pts.size() >= 3))) {
        result.push_back(current);
    }
    
    return result;
}

// ─── chainSegments (улучшенная O(n) версия) ───────────────────────────────────
std::vector<Polygon> DXFLoader::chainSegments(std::vector<Segment> segs, double tol) {
    std::vector<Polygon> result;
    if (segs.empty()) return result;

    auto roundKey = [tol](const Point& p) -> std::pair<int64_t, int64_t> {
        return {(int64_t)std::round(p.x / tol), (int64_t)std::round(p.y / tol)};
    };

    using Key = std::pair<int64_t, int64_t>;
    struct KeyHash {
        size_t operator()(const Key& k) const {
            return std::hash<int64_t>()(k.first) ^ std::hash<int64_t>()(k.second);
        }
    };

    std::unordered_map<Key, std::vector<std::pair<int, bool>>, KeyHash> endMap;
    std::vector<bool> used(segs.size(), false);

    // Строим карту концов
    for (int i = 0; i < (int)segs.size(); ++i) {
        endMap[roundKey(segs[i].first)].emplace_back(i, true);  // true = начало
        endMap[roundKey(segs[i].second)].emplace_back(i, false); // false = конец
    }

    auto findNext = [&](const Point& tail) -> std::pair<int, bool> {
        auto it = endMap.find(roundKey(tail));
        if (it == endMap.end()) return {-1, false};
        for (auto& p : it->second) {
            if (!used[p.first]) return p;
        }
        return {-1, false};
    };

    auto markUsed = [&](int i) {
        used[i] = true;
        // Удаляем из обоих концов
        for (int pass = 0; pass < 2; ++pass) {
            const Point& pt = pass == 0 ? segs[i].first : segs[i].second;
            auto it = endMap.find(roundKey(pt));
            if (it != endMap.end()) {
                it->second.erase(
                    std::remove_if(it->second.begin(), it->second.end(),
                        [i](const auto& p) { return p.first == i; }),
                    it->second.end());
            }
        }
    };

    // Собираем все цепочки
    for (int start = 0; start < (int)segs.size(); ++start) {
        if (used[start]) continue;

        std::vector<Point> chain;
        chain.push_back(segs[start].first);
        chain.push_back(segs[start].second);
        markUsed(start);

        // Вперед
        while (true) {
            auto [idx, rev] = findNext(chain.back());
            if (idx < 0) break;
            chain.push_back(rev ? segs[idx].first : segs[idx].second);
            markUsed(idx);
        }

        // Назад
        while (true) {
            auto [idx, rev] = findNext(chain.front());
            if (idx < 0) break;
            chain.insert(chain.begin(), rev ? segs[idx].second : segs[idx].first);
            markUsed(idx);
        }

        // Замкнутый контур?
        if (chain.size() >= 4 && chain.front().distanceTo(chain.back()) < tol * 2) {
            chain.pop_back();
            Polygon poly(chain);
            poly.removeDuplicates(tol * 0.5);
            if (poly.area() > 1.0) result.push_back(std::move(poly));
        }
    }

    return result;
}

// ─── buildContours ───────────────────────────────────────────────────────────
std::vector<Polygon> DXFLoader::buildContours(const std::vector<Entity>& ents, bool cutLayer) {
    std::vector<Polygon> result;
    std::vector<Segment> segments;

    int lineCount = 0, polyCount = 0, circleCount = 0, arcCount = 0;

    for (const auto& e : ents) {
        bool isCut = isCutLayer(e.layer, e.color);
        bool isMark = isMarkLayer(e.layer, e.color);

        // Фильтр по типу (cutLayer=true → только CUT, false → только MARK)
        if (cutLayer) {
            if (!isCut && !e.layer.empty()) continue;
        } else {
            if (!isMark && !e.layer.empty()) continue;
        }

        // ✅ ЗАМКНУТЫЕ POLYLINE → сразу Polygon
        if ((e.type == "LWPOLYLINE" || e.type == "POLYLINE") && e.closed && e.pts.size() >= 3) {
            Polygon poly(e.pts);
            poly.removeDuplicates();
            if (poly.area() > 1.0) result.push_back(std::move(poly));
            ++polyCount;
        }
        // ✅ ОТКРЫТЫЕ POLYLINE → сегменты для chainSegments
        else if ((e.type == "LWPOLYLINE" || e.type == "POLYLINE") && e.pts.size() >= 2) {
            for (size_t i = 0; i + 1 < e.pts.size(); ++i) {
                segments.emplace_back(e.pts[i], e.pts[i + 1]);
            }
            ++polyCount;
        }
        // ✅ ЛИНИИ
        else if (e.type == "LINE" && e.pts.size() == 2) {
            segments.emplace_back(e.pts[0], e.pts[1]);
            ++lineCount;
        }
        // ✅ ОКРУЖНОСТИ
        else if (e.type == "CIRCLE" && e.radius > 0) {
            Point center = e.pts.empty() ? Point{} : e.pts[0];
            auto pts = arcToPoints(center, e.radius, 0, 360, 36);
            if (!pts.empty()) pts.pop_back();
            Polygon poly(pts);
            poly.removeDuplicates();
            if (poly.area() > 1.0) result.push_back(std::move(poly));
            ++circleCount;
        }
        // ✅ ДУГИ
        else if (e.type == "ARC" && e.radius > 0) {
            Point center = e.pts.empty() ? Point{} : e.pts[0];
            auto pts = arcToPoints(center, e.radius, e.startAngle, e.endAngle, 24);
            for (size_t i = 0; i + 1 < pts.size(); ++i) {
                segments.emplace_back(pts[i], pts[i + 1]);
            }
            ++arcCount;
        }
        // ✅ СПЛАЙНЫ (линейная аппроксимация)
        else if (e.type == "SPLINE" && e.pts.size() >= 2) {
            for (size_t i = 0; i + 1 < e.pts.size(); ++i) {
                segments.emplace_back(e.pts[i], e.pts[i + 1]);
            }
        }
    }

    // Замыкаем открытые контуры
    auto chained = chainSegments(std::move(segments), 0.1);
    result.insert(result.end(), std::make_move_iterator(chained.begin()),
                  std::make_move_iterator(chained.end()));

    // ✅ Статистика (локальная)
    if (lineCount || polyCount || circleCount || arcCount) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), 
            "Примитивов: LINE=%d, POLY=%d, CIRCLE=%d, ARC=%d", 
            lineCount, polyCount, circleCount, arcCount);
        result[0].userData = buf; // Первая полигон получает статистику
    }

    return result;
}

// ─── ✅ ОСНОВНАЯ ФУНКЦИЯ ЗАГРУЗКИ ──────────────────────────────────────────────
DXFLoader::LoadResult DXFLoader::loadFile(const std::string& filename) {
    LoadResult res;
    
    std::ifstream f(filename);
    if (!f) {
        res.warnings.push_back("Не удалось открыть: " + filename);
        return res;
    }

    // ✅ Парсим ВСЕ сущности
    auto entities = parseEntities(f);
    
    if (entities.empty()) {
        res.warnings.push_back("DXF не содержит примитивов (LINE/LWPOLYLINE/ARC/CIRCLE)");
        return res;
    }

    // ✅ Строим контуры CUT и MARK
    res.cutContours = buildContours(entities, true);
    res.markContours = buildContours(entities, false);

    // ✅ Fallback: если нет CUT - берём всё
    if (res.cutContours.empty()) {
        res.warnings.push_back("Слой CUT не найден. Используются все контуры.");
        for (const auto& e : entities) {
            if ((e.type == "LWPOLYLINE" || e.type == "POLYLINE") && e.closed && e.pts.size() >= 3) {
                Polygon poly(e.pts);
                poly.removeDuplicates();
                if (poly.area() > 1.0) res.cutContours.push_back(std::move(poly));
            }
        }
    }

    // ✅ Нормализация и очистка
    auto cleanPolys = [&](std::vector<Polygon>& polys) {
        for (auto& poly : polys) {
            poly.makeCCW();
            poly.removeDuplicates();
        }
        polys.erase(std::remove_if(polys.begin(), polys.end(), [&](const Polygon& p) {
            if (p.verts.size() < 3 || p.area() < 1.0) {
                res.warnings.push_back("Удалён вырожденный контур (площадь < 1мм²)");
                return true;
            }
            return false;
        }), polys.end());
    };
    
    cleanPolys(res.cutContours);
    cleanPolys(res.markContours);

    res.success = !res.cutContours.empty();

    if (!entities.empty()) {
        res.warnings.push_back("Загружено сущностей: " + std::to_string(entities.size()));
    }

    return res;
}
