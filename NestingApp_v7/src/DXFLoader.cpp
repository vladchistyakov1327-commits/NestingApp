// DXFLoader.cpp — Универсальный парсер DXF R9–R2024
// Протестирован на реальных файлах: LINE/ARC/CIRCLE/LWPOLYLINE/SPLINE/ELLIPSE
// Слои: layer 0, Слой 1, layer 1, OUTER_PROFILES, INTERIOR_PROFILES, цвета 7/250/177

#include "DXFLoader.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <unordered_map>
#include <cstdint>

static constexpr double DXF_PI = M_PI;

// ─── Утилиты ─────────────────────────────────────────────────────────────────

static std::string strTrim(std::string s) {
    if (s.size()>=3 && (unsigned char)s[0]==0xEF &&
        (unsigned char)s[1]==0xBB && (unsigned char)s[2]==0xBF) s.erase(0,3);
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    s.erase(0, a);
    auto b = s.find_last_not_of(" \t\r\n");
    if (b != std::string::npos) s.erase(b+1);
    return s;
}

static std::string strUp(std::string s) {
    for (auto& c : s) if (c>='a'&&c<='z') c-=32;
    return s;
}

static double toD(const std::string& s) {
    double v=0.0; std::sscanf(s.c_str(), "%lf", &v); return v;
}
static int toI(const std::string& s) {
    try { return std::stoi(s); } catch(...) { return 0; }
}

// ─── Классификация ───────────────────────────────────────────────────────────

bool DXFLoader::isCutLayer(const std::string& layer, int color) {
    std::string up = strUp(layer);
    // Явные MARK слои → не CUT
    if (up.find("MARK")             != std::string::npos) return false;
    if (up.find("ENGRAVE")          != std::string::npos) return false;
    if (up.find("INTERIOR_PROFILE") != std::string::npos) return false;
    if (up.find("TEXT")             != std::string::npos) return false;
    if (up.find("DIM")              != std::string::npos) return false;
    if (color == 5) return false; // синий = гравировка
    // ВСЁ остальное = CUT (layer 0, Слой 1, layer 1, OUTER_PROFILES, 15, etc.)
    return true;
}

bool DXFLoader::isMarkLayer(const std::string& layer, int color) {
    std::string up = strUp(layer);
    if (up.find("MARK")             != std::string::npos) return true;
    if (up.find("ENGRAVE")          != std::string::npos) return true;
    if (up.find("INTERIOR_PROFILE") != std::string::npos) return true;
    if (color == 5) return true;
    return false;
}

// ─── ARC → точки ─────────────────────────────────────────────────────────────

std::vector<Point> DXFLoader::arcToPoints(const Point& c, double r,
                                           double a0, double a1, int segs) {
    std::vector<Point> pts;
    if (r <= GEO_EPS || segs < 2) return pts;
    if (std::abs(a1-a0) < GEO_EPS) return pts;
    if (a1 <= a0) a1 += 360.0;
    double rng = a1 - a0;
    pts.reserve(segs+1);
    for (int i=0; i<=segs; ++i) {
        double ang = (a0 + rng*i/segs) * DXF_PI/180.0;
        pts.push_back({c.x + r*std::cos(ang), c.y + r*std::sin(ang)});
    }
    return pts;
}

// ─── NURBS B-Spline → точки (de Boor) ────────────────────────────────────────

std::vector<Point> DXFLoader::splineToPoints(const SplineData& spl, int segs) {
    std::vector<Point> result;
    const auto& K = spl.knots;
    const auto& P = spl.ctrl;
    const auto& W = spl.weights;
    int deg = spl.degree;
    int n   = (int)P.size();

    if (n < 2 || (int)K.size() < n + deg + 1) {
        // Недостаточно данных — линейная аппроксимация по контрольным точкам
        return P;
    }

    double tMin = K.front(), tMax = K.back();
    if (tMax - tMin < 1e-12) return P;

    result.reserve(segs+1);
    for (int s=0; s<=segs; ++s) {
        double t = tMin + (tMax-tMin)*s/segs;
        if (t > tMax - 1e-12) t = tMax - 1e-12;

        // Найти knot span
        int span = deg;
        for (int j=deg; j < (int)K.size()-deg-1; ++j)
            if (t >= K[j] && t < K[j+1]) { span=j; break; }

        // de Boor
        std::vector<double> wx(deg+1,0), wy(deg+1,0), ww(deg+1,0);
        for (int j=0; j<=deg; ++j) {
            int idx = span-deg+j;
            if (idx<0||idx>=n) continue;
            double w = W.empty() ? 1.0 : W[idx];
            wx[j] = P[idx].x*w;
            wy[j] = P[idx].y*w;
            ww[j] = w;
        }
        for (int r=1; r<=deg; ++r) {
            for (int j=deg; j>=r; --j) {
                int i = span-deg+j;
                double denom = K[i+deg-r+1] - K[i];
                double alpha = (denom < 1e-14) ? 0.0 : (t-K[i])/denom;
                wx[j] = (1-alpha)*wx[j-1] + alpha*wx[j];
                wy[j] = (1-alpha)*wy[j-1] + alpha*wy[j];
                ww[j] = (1-alpha)*ww[j-1] + alpha*ww[j];
            }
        }
        double w = (ww[deg] < 1e-14) ? 1.0 : ww[deg];
        result.push_back({wx[deg]/w, wy[deg]/w});
    }
    return result;
}

// ─── Парсер ───────────────────────────────────────────────────────────────────
// DXF: строка 1 = код группы (int), строка 2 = значение. Всегда пары.

std::vector<DXFLoader::Entity> DXFLoader::parseEntities(const std::string& content) {
    std::vector<Entity> result;

    // Читаем все пары group/value
    std::vector<std::pair<int,std::string>> P;
    P.reserve(16384);
    {
        std::istringstream in(content);
        std::string gl, vl;
        while (std::getline(in, gl)) {
            gl = strTrim(gl);
            if (!std::getline(in, vl)) break;
            vl = strTrim(vl);
            int g=0;
            try { g=std::stoi(gl); } catch(...) { continue; }
            P.emplace_back(g, vl);
        }
    }

    // Карта блоков для INSERT
    std::unordered_map<std::string, std::vector<Entity>> blocks;

    enum Sec { NONE, HEADER, TABLES, BLOCKS, ENTITIES } sec = NONE;
    bool inBlock = false;
    std::string blockName;

    Entity cur;
    // POLYLINE state
    Entity polyline;
    bool inVertex = false;
    double vtxX=0, vtxY=0, lwX=0, lwY=0;
    bool vtxHasX=false, vtxHasY=false, lwPending=false;
    // SPLINE state
    SplineData spl;

    // INSERT state
    struct Ins { std::string name; double x=0,y=0,sx=1,sy=1,rot=0; } ins;
    bool collectIns = false;

    auto getStore = [&]() -> std::vector<Entity>& {
        return inBlock ? blocks[blockName] : result;
    };

    // Финализация SPLINE: вычисляем точки
    auto finishSpline = [&](Entity& e) {
        if (e.type != "SPLINE") return;
        if (spl.ctrl.size() >= 2) {
            int segs = std::max(48, (int)spl.ctrl.size() * 8);
            e.pts = splineToPoints(spl, segs);
        }
        if (e.pts.size() < 2 && spl.ctrl.size() >= 2)
            e.pts = spl.ctrl;
        spl = SplineData{};
    };

    auto flushPolyline = [&]() {
        if (lwPending) { polyline.pts.push_back({lwX,lwY}); lwPending=false; }
        if (vtxHasX && vtxHasY) {
            polyline.pts.push_back({vtxX,vtxY});
            vtxHasX=vtxHasY=false;
        }
        if (!polyline.type.empty() && polyline.pts.size()>=2)
            getStore().push_back(polyline);
        polyline=Entity{}; inVertex=false;
    };

    auto commitCur = [&]() {
        if (cur.type.empty()) return;
        auto& st = getStore();
        if (cur.type=="LINE"       && cur.pts.size()>=2)                     st.push_back(cur);
        else if ((cur.type=="LWPOLYLINE"||cur.type=="POLYLINE")&&cur.pts.size()>=2) st.push_back(cur);
        else if ((cur.type=="ARC"||cur.type=="CIRCLE")&&!cur.pts.empty()&&cur.radius>GEO_EPS) st.push_back(cur);
        else if (cur.type=="ELLIPSE"   && cur.pts.size()>=2)                 st.push_back(cur);
        else if (cur.type=="SPLINE"    && cur.pts.size()>=2)                 st.push_back(cur);
        else if (cur.type=="INSERT" && collectIns) {
            // Раскрываем блок
            auto it = blocks.find(ins.name);
            if (it != blocks.end()) {
                double rad = ins.rot*DXF_PI/180.0, cs=std::cos(rad), sn=std::sin(rad);
                for (Entity be : it->second) {
                    for (auto& p : be.pts) {
                        double nx=p.x*ins.sx, ny=p.y*ins.sy;
                        p.x=nx*cs-ny*sn+ins.x; p.y=nx*sn+ny*cs+ins.y;
                    }
                    be.radius *= std::max(std::abs(ins.sx),std::abs(ins.sy));
                    result.push_back(be);
                }
            }
            collectIns=false;
        }
        cur=Entity{};
    };

    size_t N = P.size();
    for (size_t i=0; i<N; ++i) {
        int  g = P[i].first;
        const std::string& v  = P[i].second;
        std::string vu = strUp(v);

        // ── Секции ──
        if (g==0 && vu=="SECTION" && i+2<N && P[i+1].first==2) {
            std::string sn = strUp(P[i+1].second);
            if      (sn=="HEADER")   sec=HEADER;
            else if (sn=="TABLES")   sec=TABLES;
            else if (sn=="BLOCKS")   { sec=BLOCKS; inBlock=false; }
            else if (sn=="ENTITIES") { sec=ENTITIES; inBlock=false; }
            else                     sec=NONE;
            ++i; continue;
        }
        if (g==0 && vu=="ENDSEC") {
            flushPolyline(); finishSpline(cur); commitCur();
            sec=NONE; inBlock=false; continue;
        }
        if (g==0 && vu=="EOF") {
            flushPolyline(); finishSpline(cur); commitCur(); break;
        }

        // ── BLOCKS ──
        if (sec==BLOCKS) {
            if (g==0) {
                if      (vu=="BLOCK")  { inBlock=true; blockName=""; continue; }
                else if (vu=="ENDBLK") { flushPolyline(); finishSpline(cur); commitCur(); inBlock=false; continue; }
                else {
                    if (vu!="SEQEND") { flushPolyline(); finishSpline(cur); commitCur(); }
                    if (vu=="SEQEND") { flushPolyline(); cur=Entity{}; continue; }
                    cur.type=vu;
                    if (vu=="LWPOLYLINE") { polyline.type="LWPOLYLINE"; lwPending=false; }
                    if (vu=="POLYLINE")   polyline.type="POLYLINE";
                    if (vu=="SPLINE")     { spl=SplineData{};  }
                    if (vu=="VERTEX") {
                        if(vtxHasX&&vtxHasY){polyline.pts.push_back({vtxX,vtxY});vtxHasX=vtxHasY=false;}
                        vtxX=vtxY=0; inVertex=true; continue;
                    }
                    continue;
                }
            }
            if (g==2 && inBlock && blockName.empty()) { blockName=v; continue; }
            goto PROPS;
        }

        if (sec != ENTITIES) continue;

        // ── Группа 0: новая сущность ──
        if (g==0) {
            if (vu=="SEQEND") { flushPolyline(); cur=Entity{}; continue; }
            if (vu=="VERTEX") {
                if (vtxHasX&&vtxHasY) { polyline.pts.push_back({vtxX,vtxY}); vtxHasX=vtxHasY=false; }
                vtxX=vtxY=0; inVertex=true; continue;
            }
            if (inVertex) {
                if (vtxHasX&&vtxHasY) polyline.pts.push_back({vtxX,vtxY});
                vtxHasX=vtxHasY=false; inVertex=false;
            }
            flushPolyline();
            finishSpline(cur);
            commitCur();
            cur.type=vu; 
            if (vu=="LWPOLYLINE") { polyline.type="LWPOLYLINE"; lwPending=false; }
            if (vu=="POLYLINE")   polyline.type="POLYLINE";
            if (vu=="SPLINE")     { spl=SplineData{};  }
            if (vu=="INSERT")     { collectIns=true; ins=Ins{}; ins.sx=ins.sy=1; }
            continue;
        }

        PROPS:

        if (g==100) continue; // AcDb* subclass markers — пропускаем

        // Слой и цвет
        if (g==8) {
            if (!polyline.type.empty()&&!inVertex) polyline.layer=v;
            else if (!inVertex) cur.layer=v;
            continue;
        }
        if (g==62) {
            int col=toI(v);
            if (!polyline.type.empty()&&!inVertex) polyline.color=col;
            else if (!inVertex) cur.color=col;
            continue;
        }

        // ── LWPOLYLINE ──
        if (!polyline.type.empty() && polyline.type=="LWPOLYLINE") {
            if (g==70) { polyline.closed=(toI(v)&1)!=0; continue; }
            if (g==10) { if(lwPending)polyline.pts.push_back({lwX,lwY}); lwX=toD(v); lwPending=true; continue; }
            if (g==20 && lwPending) { lwY=toD(v); polyline.pts.push_back({lwX,lwY}); lwPending=false; continue; }
            continue;
        }

        // ── POLYLINE + VERTEX ──
        if (!polyline.type.empty() && polyline.type=="POLYLINE") {
            if (g==70 && !inVertex) { polyline.closed=(toI(v)&1)!=0; continue; }
            if (inVertex) {
                if (g==10) { vtxX=toD(v); vtxHasX=true; continue; }
                if (g==20) { vtxY=toD(v); vtxHasY=true; continue; }
            }
            continue;
        }

        // ── INSERT ──
        if (cur.type=="INSERT" && collectIns) {
            if (g==2)  { ins.name=v;    continue; }
            if (g==10) { ins.x=toD(v);  continue; }
            if (g==20) { ins.y=toD(v);  continue; }
            if (g==41) { ins.sx=toD(v); continue; }
            if (g==42) { ins.sy=toD(v); continue; }
            if (g==50) { ins.rot=toD(v);continue; }
            continue;
        }

        // ── LINE ──
        if (cur.type=="LINE") {
            if (g==10) { cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==20 && !cur.pts.empty()) { cur.pts[cur.pts.size()==1?0:cur.pts.size()-1].y=toD(v); continue; }
            if (g==11) { cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==21 && cur.pts.size()>=2) { cur.pts.back().y=toD(v); continue; }
            continue;
        }

        // ── ARC / CIRCLE ──
        if (cur.type=="ARC" || cur.type=="CIRCLE") {
            if (g==10) { cur.pts.clear(); cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==20 && !cur.pts.empty()) { cur.pts[0].y=toD(v); continue; }
            if (g==40) { cur.radius=toD(v); continue; }
            if (g==50) { cur.startAngle=toD(v); continue; }
            if (g==51) { cur.endAngle=toD(v);   continue; }
            continue;
        }

        // ── ELLIPSE ──
        if (cur.type=="ELLIPSE") {
            if (g==10) { cur.pts.clear(); cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==20 && !cur.pts.empty()) { cur.pts[0].y=toD(v); continue; }
            if (g==11) { cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==21 && cur.pts.size()>=2) { cur.pts[1].y=toD(v); continue; }
            if (g==40) { cur.radius=toD(v);      continue; } // minor/major ratio
            if (g==41) { cur.startAngle=toD(v);  continue; } // param start (rad)
            if (g==42) { cur.endAngle=toD(v);    continue; } // param end (rad)
            continue;
        }

        // ── SPLINE ──
        // group 71=degree, 72=nKnots, 73=nCtrl
        // group 40=knot values (повторяются nKnots раз)
        // group 41=weights (опционально)
        // group 10/20=control points (повторяются nCtrl раз, ПОСЛЕ knots)
        if (cur.type=="SPLINE") {
            if (g==71) { spl.degree=toI(v);  continue; }
            if (g==72) { spl.nKnots=toI(v);  continue; }
            if (g==73) { spl.nCtrl=toI(v);   continue; }
            if (g==40) { spl.knots.push_back(toD(v)); continue; }
            if (g==41) { spl.weights.push_back(toD(v)); continue; }
            if (g==10) { spl.ctrl.emplace_back(toD(v),0.0); continue; }
            if (g==20 && !spl.ctrl.empty()) { spl.ctrl.back().y=toD(v); continue; }
            // g==30 = Z, игнорируем
            continue;
        }
    }

    // Финализация
    if (inVertex && vtxHasX && vtxHasY) polyline.pts.push_back({vtxX,vtxY});
    flushPolyline();
    finishSpline(cur);
    commitCur();
    return result;
}

// ─── buildContours ────────────────────────────────────────────────────────────

std::vector<Polygon> DXFLoader::buildContours(const std::vector<Entity>& ents, bool cutLayer) {
    std::vector<Polygon> res;
    std::vector<Segment> segs;

    for (const auto& e : ents) {
        bool isCut  = isCutLayer(e.layer, e.color);
        bool isMark = isMarkLayer(e.layer, e.color);
        if ( cutLayer && !isCut && isMark)  continue;
        if (!cutLayer && !isMark)            continue;

        auto addSeg = [&](const std::vector<Point>& pts, bool closed) {
            if (closed && pts.size()>=3) {
                Polygon poly(pts); poly.removeDuplicates();
                if (poly.area()>1.0) { poly.makeCCW(); res.push_back(std::move(poly)); }
            } else {
                for (size_t j=0; j+1<pts.size(); ++j)
                    segs.push_back({pts[j], pts[j+1]});
            }
        };

        if (e.type=="LWPOLYLINE"||e.type=="POLYLINE") {
            addSeg(e.pts, e.closed && e.pts.size()>=3);
        }
        else if (e.type=="LINE" && e.pts.size()==2) {
            segs.push_back({e.pts[0], e.pts[1]});
        }
        else if (e.type=="CIRCLE" && !e.pts.empty() && e.radius>GEO_EPS) {
            auto pts=arcToPoints(e.pts[0],e.radius,0,360,64);
            if (!pts.empty()) pts.pop_back();
            addSeg(pts, true);
        }
        else if (e.type=="ARC" && !e.pts.empty() && e.radius>GEO_EPS) {
            auto pts=arcToPoints(e.pts[0],e.radius,e.startAngle,e.endAngle,48);
            addSeg(pts, false);
        }
        else if (e.type=="ELLIPSE" && e.pts.size()>=2) {
            double mj=std::sqrt(e.pts[1].x*e.pts[1].x+e.pts[1].y*e.pts[1].y);
            if (mj<GEO_EPS) continue;
            double mn=mj*e.radius, ax=std::atan2(e.pts[1].y,e.pts[1].x);
            double a0=e.startAngle, a1=e.endAngle;
            if (a1<=a0) a1+=2*DXF_PI;
            bool full=std::abs(a1-a0-2*DXF_PI)<0.01;
            int s=full?64:std::max(8,(int)(64*(a1-a0)/(2*DXF_PI)));
            std::vector<Point> pts; pts.reserve(s+1);
            for (int k=0;k<=s;++k) {
                double t=a0+(a1-a0)*k/s;
                double lx=mj*std::cos(t), ly=mn*std::sin(t);
                pts.push_back({e.pts[0].x+lx*std::cos(ax)-ly*std::sin(ax),
                               e.pts[0].y+lx*std::sin(ax)+ly*std::cos(ax)});
            }
            if (full&&!pts.empty()) pts.pop_back();
            addSeg(pts, full);
        }
        else if (e.type=="SPLINE" && e.pts.size()>=2) {
            bool closed = e.pts.size()>=3 &&
                std::abs(e.pts.front().x-e.pts.back().x)<1.0 &&
                std::abs(e.pts.front().y-e.pts.back().y)<1.0;
            auto pts = e.pts;
            if (closed && !pts.empty()) pts.pop_back();
            addSeg(pts, closed);
        }
    }

    auto chained=chainSegments(std::move(segs));
    for (auto& p:chained) res.push_back(std::move(p));
    return res;
}

// ─── chainSegments O(n) ──────────────────────────────────────────────────────

std::vector<Polygon> DXFLoader::chainSegments(std::vector<Segment> segs, double tol) {
    std::vector<Polygon> result;
    if (segs.empty()) return result;

    using Key=std::pair<int64_t,int64_t>;
    struct KH { size_t operator()(const Key& k) const {
        return std::hash<int64_t>()(k.first)*2654435761ULL^std::hash<int64_t>()(k.second);
    }};
    auto rk=[&](const Point& p)->Key{
        return {(int64_t)std::round(p.x/tol),(int64_t)std::round(p.y/tol)};
    };

    std::vector<bool> used(segs.size(),false);
    std::unordered_map<Key,std::vector<std::pair<int,bool>>,KH> em;
    em.reserve(segs.size()*2);
    for (int i=0;i<(int)segs.size();++i) {
        em[rk(segs[i].first)].push_back({i,true});
        em[rk(segs[i].second)].push_back({i,false});
    }

    auto findNext=[&](const Point& tail)->std::pair<int,bool>{
        auto it=em.find(rk(tail));
        if(it==em.end()) return {-1,false};
        for(auto&[idx,rev]:it->second) if(!used[idx]) return {idx,rev};
        return {-1,false};
    };
    auto markUsed=[&](int i){
        used[i]=true;
        for(int p=0;p<2;++p){
            const Point& pt=(p==0)?segs[i].first:segs[i].second;
            auto it=em.find(rk(pt));
            if(it==em.end()) continue;
            auto& vec=it->second;
            vec.erase(std::remove_if(vec.begin(),vec.end(),
                [i](const std::pair<int,bool>& x){return x.first==i;}),vec.end());
        }
    };

    for (int si=0;si<(int)segs.size();++si) {
        if(used[si]) continue;
        std::vector<Point> chain;
        chain.push_back(segs[si].first);
        chain.push_back(segs[si].second);
        markUsed(si);
        while(true){
            auto[idx,rev]=findNext(chain.back());
            if(idx<0) break;
            chain.push_back(rev?segs[idx].second:segs[idx].first);
            markUsed(idx);
        }
        bool closed=chain.size()>=4&&
            std::abs(chain.front().x-chain.back().x)<tol*2&&
            std::abs(chain.front().y-chain.back().y)<tol*2;
        if(closed){
            chain.pop_back();
            if(chain.size()>=3){
                Polygon poly(chain); poly.removeDuplicates(tol*0.5);
                if(poly.area()>1.0){poly.makeCCW();result.push_back(std::move(poly));}
            }
        }
    }
    return result;
}

// ─── loadFile ─────────────────────────────────────────────────────────────────

DXFLoader::LoadResult DXFLoader::loadFile(const std::string& filename) {
    LoadResult res;

    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) {
        res.warnings.push_back("Не удалось открыть: "+filename);
        return res;
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    // Нормализуем CRLF: \r\n → \n, одиночный \r → \n
    {
        std::string fixed; fixed.reserve(content.size());
        for (size_t ci=0; ci<content.size(); ++ci) {
            if (content[ci]=='\r') {
                fixed += '\n';
                if (ci+1<content.size() && content[ci+1]=='\n') ++ci;
            } else {
                fixed += content[ci];
            }
        }
        content = std::move(fixed);
    }

    auto entities = parseEntities(content);

    if (entities.empty()) {
        res.warnings.push_back("DXF не содержит примитивов (LINE/LWPOLYLINE/ARC/CIRCLE/SPLINE/ELLIPSE)");
        return res;
    }

    res.cutContours  = buildContours(entities, true);
    res.markContours = buildContours(entities, false);

    // Fallback: нет CUT → берём все контуры без фильтрации по слою
    if (res.cutContours.empty()) {
        res.warnings.push_back("Автоопределение слоя не сработало — используем все контуры");
        std::vector<Segment> segs;
        for (const auto& e:entities) {
            if ((e.type=="LWPOLYLINE"||e.type=="POLYLINE")&&e.pts.size()>=2) {
                if(e.closed&&e.pts.size()>=3){Polygon p(e.pts);p.removeDuplicates();if(p.area()>1.0){p.makeCCW();res.cutContours.push_back(std::move(p));}}
                else for(size_t j=0;j+1<e.pts.size();++j) segs.push_back({e.pts[j],e.pts[j+1]});
            } else if(e.type=="LINE"&&e.pts.size()==2) segs.push_back({e.pts[0],e.pts[1]});
            else if(e.type=="CIRCLE"&&!e.pts.empty()&&e.radius>GEO_EPS){auto p=arcToPoints(e.pts[0],e.radius,0,360,64);if(!p.empty())p.pop_back();Polygon poly(p);poly.removeDuplicates();if(poly.area()>1.0){poly.makeCCW();res.cutContours.push_back(std::move(poly));}}
            else if(e.type=="ARC"&&!e.pts.empty()&&e.radius>GEO_EPS){auto p=arcToPoints(e.pts[0],e.radius,e.startAngle,e.endAngle,48);for(size_t j=0;j+1<p.size();++j)segs.push_back({p[j],p[j+1]});}
            else if((e.type=="SPLINE"||e.type=="ELLIPSE")&&e.pts.size()>=2){
                bool cl=e.pts.size()>=3&&std::abs(e.pts.front().x-e.pts.back().x)<1.0&&std::abs(e.pts.front().y-e.pts.back().y)<1.0;
                auto pts=e.pts; if(cl&&!pts.empty())pts.pop_back();
                if(cl&&pts.size()>=3){Polygon poly(pts);poly.removeDuplicates();if(poly.area()>1.0){poly.makeCCW();res.cutContours.push_back(std::move(poly));}}
                else for(size_t j=0;j+1<pts.size();++j)segs.push_back({pts[j],pts[j+1]});
            }
        }
        auto chained=chainSegments(std::move(segs));
        for(auto& p:chained) res.cutContours.push_back(std::move(p));
    }

    // Очистка вырожденных
    auto clean=[&](std::vector<Polygon>& polys){
        for(auto& p:polys){p.makeCCW();p.removeDuplicates();}
        polys.erase(std::remove_if(polys.begin(),polys.end(),[&](const Polygon& p){
            bool bad=p.verts.size()<3||p.area()<1.0;
            return bad;
        }),polys.end());
    };
    clean(res.cutContours);
    clean(res.markContours);

    res.success = !res.cutContours.empty();
    if (!res.success)
        res.warnings.push_back("Не найдено замкнутых контуров. Проверьте что контуры замкнуты.");
    else
        res.warnings.push_back("Загружено: "+std::to_string(res.cutContours.size())+
                                " CUT + "+std::to_string(res.markContours.size())+" MARK");
    return res;
}
