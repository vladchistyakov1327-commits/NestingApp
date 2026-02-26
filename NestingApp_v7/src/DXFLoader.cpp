#include "DXFLoader.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <unordered_map>

namespace DXFLoader {

// ───────────────────────────────────────────────────────────
// Утилиты
// ───────────────────────────────────────────────────────────

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static int toI(const std::string& s) {
    try { return std::stoi(s); }
    catch (...) { return 0; }
}

static double toD(const std::string& s) {
    try { return std::stod(s); }
    catch (...) {
        char* end;
        double v = std::strtod(s.c_str(), &end);
        return (v != 0 || end != s.c_str()) ? v : 0.0;
    }
}

static double distance(const Point& a, const Point& b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx*dx + dy*dy);
}

static double cross(const Point& o, const Point& a, const Point& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

static double polygonArea(const std::vector<Point>& poly) {
    double area = 0.0;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += poly[i].x * poly[j].y;
        area -= poly[j].x * poly[i].y;
    }
    return std::abs(area) / 2.0;
}

// ───────────────────────────────────────────────────────────
// DXF Parser (все версии R12-R2024)
// ───────────────────────────────────────────────────────────

std::vector<Entity> parseEntities(std::istream& in) {
    std::vector<Entity> result;
    std::string line;
    
    enum State { SEARCH_SECTIONS, IN_HEADER, IN_TABLES, IN_ENTITIES, IN_POLYLINE };
    State state = SEARCH_SECTIONS;
    Entity current;
    bool inVertex = false;
    
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        
        // Парсим группу/значение (две строки = одна пара)
        std::istringstream iss(line);
        int group = 0;
        std::string val;
        iss >> group;
        std::getline(iss, val);
        val = trim(val);
        
        if (group == 0 && val.empty()) continue;
        
        // ─── СЕКЦИИ ───
        if (group == 0) {
            if (val == "SECTION") state = SEARCH_SECTIONS;
            else if (val == "HEADER") state = IN_HEADER;
            else if (val == "TABLES") state = IN_TABLES;
            else if (val == "ENTITIES" || val == "$ENTITIES") state = IN_ENTITIES;
            else if (val == "ENDSEC" || val == "EOF") state = SEARCH_SECTIONS;
            else if (val == "ENDENT" || val == "SEQEND") {
                if (state == IN_POLYLINE) state = IN_ENTITIES;
            }
        }
        
        // Пропускаем если не в ENTITIES
        if (state != IN_ENTITIES && state != IN_POLYLINE) continue;
        
        // ─── НОВАЯ СУЩНОСТЬ ───
        if (group == 0 && !current.type.empty() && !inVertex) {
            if (isValidEntity(current)) {
                result.push_back(current);
            }
            current = Entity{};
        }
        
        current.type = val;
        
        // ─── СВОЙСТВА ───
        if (group == 8) current.layer = val;
        if (group == 62) current.color = toI(val);
        if (group == 39) current.thickness = toD(val);
        if (group == 70) current.flags = toI(val);
        
        // ─── ТОЧКИ ───
        if (group == 10) {
            if (current.pts.empty()) current.pts.emplace_back(toD(val), 0);
            else current.pts.back().x = toD(val);
        }
        else if (group == 20) {
            if (!current.pts.empty()) current.pts.back().y = toD(val);
        }
        else if (group == 11 || group == 12 || group == 13) {
            current.pts.emplace_back(toD(val), 0);
        }
        else if (group == 21 || group == 22 || group == 23) {
            if (!current.pts.empty() && current.pts.back().x == 0) {
                current.pts.back().y = toD(val);
            }
        }
        
        // ─── LWPOLYLINE (R12-R2007) ───
        if (current.type == "LWPOLYLINE") {
            if (group == 90) current.vertexCount = toI(val);
            if (group == 70) current.closed = (toI(val) & 1);
        }
        // ─── POLYLINE (R2000+) ───
        else if (current.type == "POLYLINE") {
            state = IN_POLYLINE;
            current.closed = (toI(val) & 1);
        }
        else if (current.type == "VERTEX" && state == IN_POLYLINE) {
            inVertex = true;
            current.pts.emplace_back(0, 0);
        }
        
        // ─── АРКИ/КРУГИ ───
        if (group == 40) {
            if (current.type == "ARC" || current.type == "CIRCLE") 
                current.radius = toD(val);
        }
        if (current.type == "ARC") {
            if (group == 50) current.startAngle = toD(val);
            if (group == 51) current.endAngle = toD(val);
        }
    }
    
    // Последняя сущность
    if (isValidEntity(current)) result.push_back(current);
    
    return result;
}

bool isValidEntity(const Entity& e) {
    if (e.type.empty()) return false;
    if (e.pts.size() < 2) return false;
    
    if (e.type == "LINE" && e.pts.size() == 2) return true;
    if ((e.type == "LWPOLYLINE" || e.type == "POLYLINE") && e.pts.size() >= 3) return true;
    if ((e.type == "ARC" || e.type == "CIRCLE") && e.radius > 0) return true;
    
    return false;
}

// ───────────────────────────────────────────────────────────
// Chaining & Cleanup
// ───────────────────────────────────────────────────────────

std::vector<Polygon> chainSegments(const std::vector<Entity>& entities, double tolerance) {
    std::vector<Segment> segments;
    
    // 1. Извлекаем все сегменты
    for (const auto& e : entities) {
        if (e.type == "LINE") {
            segments.emplace_back(e.pts[0], e.pts[1], e.layer, e.color);
        } else if (e.type == "LWPOLYLINE" || e.type == "POLYLINE") {
            for (size_t i = 0; i < e.pts.size(); ++i) {
                size_t j = (i + 1) % e.pts.size();
                segments.emplace_back(e.pts[i], e.pts[j], e.layer, e.color);
            }
        }
    }
    
    // 2. Spatial hash для O(n) chaining
    tolerance *= tolerance; // squared
    std::unordered_map<int64_t, std::vector<size_t>> hash;
    const double cell = tolerance * 4;
    
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& s = segments[i];
        int64_t hx1 = (s.start.x / cell), hy1 = (s.start.y / cell);
        int64_t hx2 = (s.end.x / cell), hy2 = (s.end.y / cell);
        
        hash[hx1 * 1000000LL + hy1].push_back(i);
        if (hx1 != hx2 || hy1 != hy2) {
            hash[hx2 * 1000000LL + hy2].push_back(i);
        }
    }
    
    // 3. Соединяем сегменты
    std::vector<Polygon> polygons;
    std::vector<bool> used(segments.size(), false);
    
    for (size_t i = 0; i < segments.size(); ++i) {
        if (used[i]) continue;
        
        Polygon poly;
        poly.layer = segments[i].layer;
        poly.color = segments[i].color;
        poly.isClosed = false;
        
        Point current = segments[i].start;
        Point next = segments[i].end;
        poly.points.push_back(current);
        used[i] = true;
        
        while (distance(current, next) > 1e-6) {
            poly.points.push_back(next);
            current = next;
            
            // Ищем продолжение
            bool found = false;
            for (size_t j = 0; j < segments.size(); ++j) {
                if (used[j]) continue;
                const auto& cand = segments[j];
                
                if (distance(current, cand.start) < std::sqrt(tolerance) ||
                    distance(current, cand.end) < std::sqrt(tolerance)) {
                    
                    next = (distance(current, cand.start) < distance(current, cand.end)) 
                         ? cand.end : cand.start;
                    used[j] = true;
                    found = true;
                    break;
                }
            }
            
            if (!found) break;
        }
        
        // Замыкаем если возможно
        if (!poly.points.empty() && distance(poly.points.front(), poly.points.back()) < std::sqrt(tolerance)) {
            poly.isClosed = true;
            poly.points.pop_back();
        }
        
        if (poly.points.size() >= 3) {
            cleanupPolygon(poly);
            if (polygonArea(poly.points) > 1.0) { // > 1mm²
                polygons.push_back(poly);
            }
        }
    }
    
    return polygons;
}

void cleanupPolygon(Polygon& poly) {
    // Убираем дубликаты
    std::vector<Point> cleaned;
    for (const auto& p : poly.points) {
        bool dup = false;
        for (const auto& c : cleaned) {
            if (distance(p, c) < 0.01) { dup = true; break; }
        }
        if (!dup) cleaned.push_back(p);
    }
    poly.points = std::move(cleaned);
    
    // CCW порядок
    if (cross(poly.points[0], poly.points[1], poly.points[2]) < 0) {
        std::reverse(poly.points.begin(), poly.points.end());
    }
}

// ───────────────────────────────────────────────────────────
// Основной Loader
// ───────────────────────────────────────────────────────────

LoadResult DXFLoader::load(const std::string& filename) {
    LoadResult result;
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        result.warnings.push_back("Cannot open: " + filename);
        return result;
    }
    
    auto entities = parseEntities(file);
    result.entityCount = entities.size();
    
    if (entities.empty()) {
        result.warnings.push_back("DXF does not contain primitives");
        return result;
    }
    
    // Классификация по слоям/цвету
    std::vector<Polygon> cutPolys, markPolys;
    
    auto polys = chainSegments(entities, 0.1); // 0.1mm tolerance
    result.polygonCount = polys.size();
    
    for (const auto& p : polys) {
        // CUT: Layer "CUT", Red (1,7), Layer 0
        if (p.layer == "CUT" || p.layer == "0" || p.color == 1 || p.color == 7) {
            cutPolys.push_back(p);
        }
        // MARK: Blue (5)
        else if (p.color == 5 || p.layer.find("MARK") != std::string::npos) {
            markPolys.push_back(p);
        }
    }
    
    // Fallback: все замкнутые полигоны → CUT
    if (cutPolys.empty()) {
        for (const auto& p : polys) {
            if (p.isClosed) cutPolys.push_back(p);
        }
    }
    
    result.cutPolygons = std::move(cutPolys);
    result.markPolygons = std::move(markPolys);
    result.warnings.push_back("Loaded: " + std::to_string(result.cutPolygons.size()) + 
                             " CUT, " + std::to_string(result.markPolygons.size()) + " MARK");
    
    return result;
}

} // namespace DXFLoader
