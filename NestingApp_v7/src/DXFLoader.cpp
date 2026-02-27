// DXFLoader.cpp — Универсальный парсер DXF (R9–R2024)
// Поддерживает: LINE, LWPOLYLINE, POLYLINE+VERTEX, ARC, CIRCLE,
//               ELLIPSE, SPLINE, INSERT (блоки)
// Совместим с DXFLoader.h

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

static std::string trim(std::string s) {
    // Убираем UTF-8 BOM
    if (s.size() >= 3 &&
        (unsigned char)s[0]==0xEF &&
        (unsigned char)s[1]==0xBB &&
        (unsigned char)s[2]==0xBF) s.erase(0,3);
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    auto e = s.find_last_not_of(" \t\r\n");
    if (e != std::string::npos) s.erase(e+1);
    return s;
}
static std::string toUpper(std::string s) {
    std::transform(s.begin(),s.end(),s.begin(),::toupper); return s;
}
static double toD(const std::string& s) {
    double v=0; std::sscanf(s.c_str(),"%lf",&v); return v;
}
static int toI(const std::string& s) {
    try { return std::stoi(s); } catch(...) { return 0; }
}

// ─── Классификация слоёв ─────────────────────────────────────────────────────

bool DXFLoader::isCutLayer(const std::string& layer, int color) {
    std::string up = toUpper(layer);
    if (up=="0")                                  return true;
    if (up.find("CUT")     !=std::string::npos)   return true;
    if (up.find("LASER")   !=std::string::npos)   return true;
    if (up.find("ЛАЗЕР")   !=std::string::npos)   return true;
    if (up.find("RED")     !=std::string::npos)   return true;
    if (up.find("OUTLINE") !=std::string::npos)   return true;
    if (up.find("CONTOUR") !=std::string::npos)   return true;
    if (up.find("PROFILE") !=std::string::npos)   return true;
    if (up.find("SHAPE")   !=std::string::npos)   return true;
    if (up.find("RAZREZ")  !=std::string::npos)   return true;
    if (color==1||color==7||color==256)            return true;
    return false;
}

bool DXFLoader::isMarkLayer(const std::string& layer, int color) {
    std::string up = toUpper(layer);
    if (up.find("MARK")    !=std::string::npos)   return true;
    if (up.find("ENGRAVE") !=std::string::npos)   return true;
    if (up.find("ГРАВИР")  !=std::string::npos)   return true;
    if (up.find("BLUE")    !=std::string::npos)   return true;
    if (up.find("ANNOT")   !=std::string::npos)   return true;
    if (color==5)                                  return true;
    return false;
}

// ─── ARC → точки ─────────────────────────────────────────────────────────────

std::vector<Point> DXFLoader::arcToPoints(const Point& center, double radius,
                                            double startDeg, double endDeg,
                                            int segments) {
    std::vector<Point> pts;
    if (radius<=GEO_EPS||segments<2) return pts;
    double endA = endDeg;
    if (std::abs(endA-startDeg)<GEO_EPS) return pts;
    if (endA<=startDeg) endA+=360.0;
    double range = endA-startDeg;
    if (range<GEO_EPS) return pts;
    pts.reserve(segments+1);
    for (int i=0;i<=segments;++i) {
        double ang=(startDeg+range*i/segments)*DXF_PI/180.0;
        pts.push_back({center.x+radius*std::cos(ang),
                       center.y+radius*std::sin(ang)});
    }
    return pts;
}

// ─── Основной парсер ─────────────────────────────────────────────────────────
// DXF формат: КАЖДЫЙ код и значение — на ОТДЕЛЬНЫХ строках.
// Строка 1: код группы (целое число)
// Строка 2: значение
// Это единственный правильный способ читать DXF.

std::vector<DXFLoader::Entity> DXFLoader::parseEntities(std::istream& in) {
    std::vector<Entity> result;

    // Читаем весь файл парами строк
    std::vector<std::pair<int,std::string>> P;
    P.reserve(8192);
    std::string gLine, vLine;
    while (std::getline(in, gLine)) {
        gLine = trim(gLine);
        if (!std::getline(in, vLine)) break;
        vLine = trim(vLine);
        int g=0;
        try { g=std::stoi(gLine); } catch(...) { continue; }
        P.emplace_back(g, vLine);
    }

    // Карта блоков: имя → список сущностей
    std::unordered_map<std::string,std::vector<Entity>> blocks;

    enum Section { NONE,HEADER,TABLES,BLOCKS,ENTITIES } section=NONE;
    bool inBlock=false;
    std::string curBlockName;

    Entity cur;
    Entity polyline;
    bool inVertex=false;
    double vtxX=0,vtxY=0;
    bool vtxHasX=false,vtxHasY=false;
    double lwX=0,lwY=0;
    bool lwPending=false;

    // INSERT накопитель
    struct InsertRef { std::string name; double x=0,y=0,sx=1,sy=1,rot=0; };
    InsertRef ins;
    bool collectInsert=false;

    // Лямбда для получения текущего хранилища
    auto getStore = [&]() -> std::vector<Entity>& {
        return inBlock ? blocks[curBlockName] : result;
    };

    auto commitEntity = [&](Entity& e) {
        if (e.type.empty()) return;
        auto& st = getStore();
        if (e.type=="LINE"&&e.pts.size()>=2)                     st.push_back(e);
        else if ((e.type=="LWPOLYLINE"||e.type=="POLYLINE")&&e.pts.size()>=2) st.push_back(e);
        else if ((e.type=="ARC"||e.type=="CIRCLE")&&!e.pts.empty()&&e.radius>GEO_EPS) st.push_back(e);
        else if (e.type=="ELLIPSE"&&e.pts.size()>=2)              st.push_back(e);
        else if (e.type=="SPLINE"&&e.pts.size()>=2)               st.push_back(e);
        else if (e.type=="INSERT"&&collectInsert) {
            // Раскрываем блок
            auto bit=blocks.find(ins.name);
            if (bit!=blocks.end()) {
                double rad=ins.rot*DXF_PI/180.0;
                double cs=std::cos(rad),sn=std::sin(rad);
                for (Entity be: bit->second) {
                    for (auto& p: be.pts) {
                        double nx=p.x*ins.sx, ny=p.y*ins.sy;
                        p.x=nx*cs-ny*sn+ins.x;
                        p.y=nx*sn+ny*cs+ins.y;
                    }
                    be.radius*=std::max(std::abs(ins.sx),std::abs(ins.sy));
                    result.push_back(be);
                }
            }
            collectInsert=false;
        }
        e=Entity{};
    };

    auto commitPolyline = [&]() {
        if (lwPending&&!polyline.type.empty()) { polyline.pts.push_back({lwX,lwY}); lwPending=false; }
        if (vtxHasX&&vtxHasY&&!polyline.type.empty()) { polyline.pts.push_back({vtxX,vtxY}); vtxHasX=vtxHasY=false; }
        if (!polyline.type.empty()&&polyline.pts.size()>=2) getStore().push_back(polyline);
        polyline=Entity{}; inVertex=false;
    };

    for (size_t i=0;i<P.size();++i) {
        int g=P[i].first;
        const std::string& v=P[i].second;
        std::string vu=toUpper(v);

        // Секции
        if (g==0&&vu=="SECTION") {
            if (i+1<P.size()&&P[i+1].first==2) {
                std::string sn=toUpper(P[i+1].second);
                if      (sn=="HEADER")   section=HEADER;
                else if (sn=="TABLES")   section=TABLES;
                else if (sn=="BLOCKS")   { section=BLOCKS; inBlock=false; }
                else if (sn=="ENTITIES") { section=ENTITIES; inBlock=false; }
                else                     section=NONE;
                ++i;
            }
            continue;
        }
        if (g==0&&vu=="ENDSEC") {
            commitEntity(cur); commitPolyline();
            section=NONE; inBlock=false; continue;
        }
        if (g==0&&vu=="EOF") {
            commitEntity(cur); commitPolyline(); break;
        }

        // BLOCKS секция
        if (section==BLOCKS) {
            if (g==0) {
                if (vu=="BLOCK") { inBlock=true; curBlockName=""; }
                else if (vu=="ENDBLK") { commitEntity(cur); commitPolyline(); inBlock=false; }
                else {
                    commitEntity(cur); commitPolyline();
                    cur.type=vu;
                    if (vu=="POLYLINE")   polyline.type="POLYLINE";
                    if (vu=="LWPOLYLINE") { polyline.type="LWPOLYLINE"; lwPending=false; }
                }
                continue;
            }
            if (g==2&&inBlock&&curBlockName.empty()) { curBlockName=v; continue; }
            // Разбираем так же как ENTITIES (ниже)
            goto PARSE_PROPS;
        }

        if (section!=ENTITIES) continue;

        if (g==0) {
            if (vu=="SEQEND") { commitPolyline(); cur=Entity{}; continue; }
            if (vu=="VERTEX") {
                if (vtxHasX&&vtxHasY) { polyline.pts.push_back({vtxX,vtxY}); vtxHasX=vtxHasY=false; }
                vtxX=vtxY=0; inVertex=true; continue;
            }
            if (inVertex) {
                if (vtxHasX&&vtxHasY) polyline.pts.push_back({vtxX,vtxY});
                vtxHasX=vtxHasY=false; inVertex=false;
            }
            if (!polyline.type.empty()) commitPolyline();
            commitEntity(cur);
            cur.type=vu;
            if (vu=="POLYLINE")   polyline.type="POLYLINE";
            if (vu=="LWPOLYLINE") { polyline.type="LWPOLYLINE"; lwPending=false; }
            if (vu=="INSERT")     { collectInsert=true; ins=InsertRef{}; ins.sx=ins.sy=1; }
            continue;
        }

        PARSE_PROPS:

        // Общие свойства
        if (g==8) {
            if (!polyline.type.empty()&&!inVertex) polyline.layer=v;
            else if (!inVertex) cur.layer=v;
            continue;
        }
        if (g==62) {
            if (!polyline.type.empty()&&!inVertex) polyline.color=toI(v);
            else if (!inVertex) cur.color=toI(v);
            continue;
        }

        // INSERT
        if (cur.type=="INSERT"&&collectInsert) {
            if (g==2)  { ins.name=v; continue; }
            if (g==10) { ins.x=toD(v); continue; }
            if (g==20) { ins.y=toD(v); continue; }
            if (g==41) { ins.sx=toD(v); continue; }
            if (g==42) { ins.sy=toD(v); continue; }
            if (g==50) { ins.rot=toD(v); continue; }
            continue;
        }

        // LWPOLYLINE
        if (!polyline.type.empty()&&polyline.type=="LWPOLYLINE") {
            if (g==70) { polyline.closed=(toI(v)&1)!=0; continue; }
            if (g==10) { if(lwPending) polyline.pts.push_back({lwX,lwY}); lwX=toD(v); lwPending=true; continue; }
            if (g==20&&lwPending) { lwY=toD(v); polyline.pts.push_back({lwX,lwY}); lwPending=false; continue; }
            continue;
        }

        // POLYLINE + VERTEX
        if (!polyline.type.empty()&&polyline.type=="POLYLINE") {
            if (g==70&&!inVertex) { polyline.closed=(toI(v)&1)!=0; continue; }
            if (inVertex) {
                if (g==10) { vtxX=toD(v); vtxHasX=true; continue; }
                if (g==20) { vtxY=toD(v); vtxHasY=true; continue; }
            }
            continue;
        }

        // LINE
        if (cur.type=="LINE") {
            if (g==10) { cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==20&&!cur.pts.empty()) {
                if (cur.pts.size()==1) cur.pts[0].y=toD(v);
                else cur.pts.back().y=toD(v);
                continue;
            }
            if (g==11) { cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==21&&cur.pts.size()>=2) { cur.pts.back().y=toD(v); continue; }
            continue;
        }

        // ARC / CIRCLE
        if (cur.type=="ARC"||cur.type=="CIRCLE") {
            if (g==10) { cur.pts.clear(); cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==20&&!cur.pts.empty()) { cur.pts[0].y=toD(v); continue; }
            if (g==40) { cur.radius=toD(v); continue; }
            if (g==50) { cur.startAngle=toD(v); continue; }
            if (g==51) { cur.endAngle=toD(v); continue; }
            continue;
        }

        // ELLIPSE: pts[0]=center, pts[1]=major endpoint (rel), radius=minor/major ratio
        if (cur.type=="ELLIPSE") {
            if (g==10) { cur.pts.clear(); cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==20&&!cur.pts.empty()) { cur.pts[0].y=toD(v); continue; }
            if (g==11) { cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==21&&cur.pts.size()>=2) { cur.pts[1].y=toD(v); continue; }
            if (g==40) { cur.radius=toD(v); continue; }  // minor/major
            if (g==41) { cur.startAngle=toD(v); continue; } // start param
            if (g==42) { cur.endAngle=toD(v); continue; }   // end param
            continue;
        }

        // SPLINE
        if (cur.type=="SPLINE") {
            if (g==10) { cur.pts.emplace_back(toD(v),0.0); continue; }
            if (g==20&&!cur.pts.empty()) { cur.pts.back().y=toD(v); continue; }
            continue;
        }
    }

    commitPolyline();
    commitEntity(cur);
    return result;
}

// ─── R12 Fallback ─────────────────────────────────────────────────────────────
// Для DXF без секций или с нестандартным форматом

std::vector<DXFLoader::Entity> DXFLoader::parseEntitiesR12(const std::string& content) {
    std::vector<Entity> result;
    std::istringstream in(content);
    std::string line;

    std::vector<std::pair<int,std::string>> P;
    while (std::getline(in,line)) {
        line=trim(line);
        if (line.empty()) continue;
        std::istringstream ls(line);
        int g; if (!(ls>>g)) continue;
        std::string v; std::getline(ls,v); v=trim(v);
        if (!v.empty()) {
            P.emplace_back(g,v); // однострочный формат
        } else {
            std::string vl; if (!std::getline(in,vl)) break;
            P.emplace_back(g,trim(vl));
        }
    }

    Entity cur, polyline;
    bool inVertex=false;
    double vtxX=0,vtxY=0; bool vtxHasX=false,vtxHasY=false;
    double lwX=0,lwY=0; bool lwPending=false;

    auto commitE=[&](Entity& e){
        if(e.type.empty()) return;
        if(e.type=="LINE"&&e.pts.size()>=2) result.push_back(e);
        else if((e.type=="LWPOLYLINE"||e.type=="POLYLINE")&&e.pts.size()>=2) result.push_back(e);
        else if((e.type=="ARC"||e.type=="CIRCLE")&&!e.pts.empty()&&e.radius>GEO_EPS) result.push_back(e);
        else if(e.type=="SPLINE"&&e.pts.size()>=2) result.push_back(e);
        e=Entity{};
    };
    auto commitPL=[&](){
        if(lwPending&&!polyline.type.empty()){polyline.pts.push_back({lwX,lwY});lwPending=false;}
        if(vtxHasX&&vtxHasY&&!polyline.type.empty()){polyline.pts.push_back({vtxX,vtxY});vtxHasX=vtxHasY=false;}
        if(!polyline.type.empty()&&polyline.pts.size()>=2) result.push_back(polyline);
        polyline=Entity{};inVertex=false;
    };

    for (auto& [g,v]: P) {
        std::string vu=toUpper(v);
        if (g==0) {
            if(vu=="SEQEND"){commitPL();cur=Entity{};continue;}
            if(vu=="VERTEX"){
                if(vtxHasX&&vtxHasY){polyline.pts.push_back({vtxX,vtxY});vtxHasX=vtxHasY=false;}
                vtxX=vtxY=0;inVertex=true;continue;
            }
            if(inVertex){if(vtxHasX&&vtxHasY)polyline.pts.push_back({vtxX,vtxY});vtxHasX=vtxHasY=false;inVertex=false;}
            if(!polyline.type.empty()) commitPL();
            commitE(cur); cur.type=vu;
            if(vu=="POLYLINE")   polyline.type="POLYLINE";
            if(vu=="LWPOLYLINE"){polyline.type="LWPOLYLINE";lwPending=false;}
            continue;
        }
        if(g==8){if(!polyline.type.empty()&&!inVertex)polyline.layer=v;else if(!inVertex)cur.layer=v;continue;}
        if(g==62){if(!polyline.type.empty()&&!inVertex)polyline.color=toI(v);else if(!inVertex)cur.color=toI(v);continue;}

        if(!polyline.type.empty()&&polyline.type=="LWPOLYLINE"){
            if(g==70){polyline.closed=(toI(v)&1)!=0;continue;}
            if(g==10){if(lwPending)polyline.pts.push_back({lwX,lwY});lwX=toD(v);lwPending=true;continue;}
            if(g==20&&lwPending){lwY=toD(v);polyline.pts.push_back({lwX,lwY});lwPending=false;continue;}
            continue;
        }
        if(!polyline.type.empty()&&polyline.type=="POLYLINE"){
            if(g==70&&!inVertex){polyline.closed=(toI(v)&1)!=0;continue;}
            if(inVertex){if(g==10){vtxX=toD(v);vtxHasX=true;continue;}if(g==20){vtxY=toD(v);vtxHasY=true;continue;}}
            continue;
        }
        if(cur.type=="LINE"){
            if(g==10){cur.pts.emplace_back(toD(v),0.0);continue;}
            if(g==20&&!cur.pts.empty()){if(cur.pts.size()==1)cur.pts[0].y=toD(v);else cur.pts.back().y=toD(v);continue;}
            if(g==11){cur.pts.emplace_back(toD(v),0.0);continue;}
            if(g==21&&cur.pts.size()>=2){cur.pts.back().y=toD(v);continue;}
            continue;
        }
        if(cur.type=="ARC"||cur.type=="CIRCLE"){
            if(g==10){cur.pts.clear();cur.pts.emplace_back(toD(v),0.0);continue;}
            if(g==20&&!cur.pts.empty()){cur.pts[0].y=toD(v);continue;}
            if(g==40){cur.radius=toD(v);continue;}
            if(g==50){cur.startAngle=toD(v);continue;}
            if(g==51){cur.endAngle=toD(v);continue;}
            continue;
        }
        if(cur.type=="SPLINE"){
            if(g==10){cur.pts.emplace_back(toD(v),0.0);continue;}
            if(g==20&&!cur.pts.empty()){cur.pts.back().y=toD(v);continue;}
            continue;
        }
    }
    commitPL(); commitE(cur);
    return result;
}

// ─── buildContours ────────────────────────────────────────────────────────────

std::vector<Polygon> DXFLoader::buildContours(const std::vector<Entity>& ents, bool cutLayer) {
    std::vector<Polygon> result;
    std::vector<Segment> segments;

    for (const auto& e : ents) {
        bool isCut  = isCutLayer(e.layer, e.color);
        bool isMark = isMarkLayer(e.layer, e.color);
        if (cutLayer && !isCut && isMark) continue;
        if (!cutLayer && !isMark)         continue;

        if (e.type=="LWPOLYLINE"||e.type=="POLYLINE") {
            if (e.pts.size()<2) continue;
            if (e.closed&&e.pts.size()>=3) {
                Polygon poly(e.pts); poly.removeDuplicates();
                if (poly.area()>1.0){poly.makeCCW();result.push_back(std::move(poly));}
            } else {
                for (size_t j=0;j+1<e.pts.size();++j) segments.push_back({e.pts[j],e.pts[j+1]});
            }
        } else if (e.type=="LINE"&&e.pts.size()==2) {
            segments.push_back({e.pts[0],e.pts[1]});
        } else if (e.type=="CIRCLE"&&!e.pts.empty()&&e.radius>GEO_EPS) {
            auto pts=arcToPoints(e.pts[0],e.radius,0,360,48);
            if (!pts.empty()) pts.pop_back();
            Polygon poly(pts); poly.removeDuplicates();
            if (poly.area()>1.0){poly.makeCCW();result.push_back(std::move(poly));}
        } else if (e.type=="ARC"&&!e.pts.empty()&&e.radius>GEO_EPS) {
            auto pts=arcToPoints(e.pts[0],e.radius,e.startAngle,e.endAngle,32);
            for (size_t j=0;j+1<pts.size();++j) segments.push_back({pts[j],pts[j+1]});
        } else if (e.type=="ELLIPSE"&&e.pts.size()>=2) {
            double majorLen=std::sqrt(e.pts[1].x*e.pts[1].x+e.pts[1].y*e.pts[1].y);
            double minorLen=majorLen*e.radius;
            double axisAngle=std::atan2(e.pts[1].y,e.pts[1].x);
            double a0=e.startAngle, a1=e.endAngle;
            if (a1<=a0) a1+=2*DXF_PI;
            double rng=a1-a0;
            bool full=std::abs(rng-2*DXF_PI)<0.01;
            int segs=full?48:std::max(8,(int)(32*rng/(2*DXF_PI)));
            std::vector<Point> pts; pts.reserve(segs+1);
            for (int k=0;k<=segs;++k) {
                double t=a0+rng*k/segs;
                double lx=majorLen*std::cos(t), ly=minorLen*std::sin(t);
                pts.push_back({e.pts[0].x+lx*std::cos(axisAngle)-ly*std::sin(axisAngle),
                               e.pts[0].y+lx*std::sin(axisAngle)+ly*std::cos(axisAngle)});
            }
            if (full&&!pts.empty()) pts.pop_back();
            if (full&&pts.size()>=3) {
                Polygon poly(pts); poly.removeDuplicates();
                if (poly.area()>1.0){poly.makeCCW();result.push_back(std::move(poly));}
            } else {
                for (size_t j=0;j+1<pts.size();++j) segments.push_back({pts[j],pts[j+1]});
            }
        } else if (e.type=="SPLINE"&&e.pts.size()>=2) {
            for (size_t j=0;j+1<e.pts.size();++j) segments.push_back({e.pts[j],e.pts[j+1]});
        }
    }

    auto chained=chainSegments(std::move(segments));
    for (auto& p:chained) result.push_back(std::move(p));
    return result;
}

// ─── chainSegments — O(n) ────────────────────────────────────────────────────

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

    std::unordered_map<Key,std::vector<std::pair<int,bool>>,KH> em;
    em.reserve(segs.size()*2);
    std::vector<bool> used(segs.size(),false);

    auto rebuild=[&](){
        em.clear();
        for (int i=0;i<(int)segs.size();++i) {
            if(used[i]) continue;
            em[rk(segs[i].first)].push_back({i,true});
            em[rk(segs[i].second)].push_back({i,false});
        }
    };
    rebuild();

    auto findNext=[&](const Point& tail)->std::pair<int,bool>{
        auto it=em.find(rk(tail));
        if(it==em.end()) return {-1,false};
        for(auto& [idx,rev]:it->second) if(!used[idx]) return {idx,rev};
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
            auto [idx,rev]=findNext(chain.back());
            if(idx<0) break;
            chain.push_back(rev?segs[idx].second:segs[idx].first);
            markUsed(idx);
        }
        bool closed=(chain.size()>=4&&
            std::abs(chain.front().x-chain.back().x)<tol*2&&
            std::abs(chain.front().y-chain.back().y)<tol*2);
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

    // Бинарный режим для корректной обработки \r\n
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) {
        res.warnings.push_back("Не удалось открыть файл: "+filename);
        return res;
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    // Нормализуем переносы строк
    for (auto& c:content) if(c=='\r') c='\n';

    // Основной парсер (R2000+)
    std::istringstream ss(content);
    auto entities=parseEntities(ss);

    // Fallback R12 если основной ничего не нашёл
    if (entities.empty()) {
        res.warnings.push_back("Стандартный парсер не нашёл примитивов — пробуем R12 режим...");
        entities=parseEntitiesR12(content);
    }

    if (entities.empty()) {
        res.warnings.push_back(
            "DXF не содержит поддерживаемых примитивов "
            "(LINE/LWPOLYLINE/POLYLINE/ARC/CIRCLE/ELLIPSE/SPLINE)");
        return res;
    }

    res.cutContours  = buildContours(entities, true);
    res.markContours = buildContours(entities, false);

    // Fallback: нет CUT-слоёв → берём все контуры
    if (res.cutContours.empty()) {
        res.warnings.push_back("Слой CUT не найден — используем все замкнутые контуры");
        std::vector<Segment> segs;
        for (const auto& e:entities) {
            if ((e.type=="LWPOLYLINE"||e.type=="POLYLINE")&&e.pts.size()>=2) {
                if (e.closed&&e.pts.size()>=3) {
                    Polygon poly(e.pts); poly.removeDuplicates();
                    if(poly.area()>1.0){poly.makeCCW();res.cutContours.push_back(std::move(poly));}
                } else {
                    for(size_t j=0;j+1<e.pts.size();++j) segs.push_back({e.pts[j],e.pts[j+1]});
                }
            } else if (e.type=="LINE"&&e.pts.size()==2) {
                segs.push_back({e.pts[0],e.pts[1]});
            } else if (e.type=="CIRCLE"&&!e.pts.empty()&&e.radius>GEO_EPS) {
                auto pts=arcToPoints(e.pts[0],e.radius,0,360,48);
                if(!pts.empty())pts.pop_back();
                Polygon poly(pts); poly.removeDuplicates();
                if(poly.area()>1.0){poly.makeCCW();res.cutContours.push_back(std::move(poly));}
            } else if (e.type=="ARC"&&!e.pts.empty()&&e.radius>GEO_EPS) {
                auto pts=arcToPoints(e.pts[0],e.radius,e.startAngle,e.endAngle,32);
                for(size_t j=0;j+1<pts.size();++j) segs.push_back({pts[j],pts[j+1]});
            } else if (e.type=="SPLINE"&&e.pts.size()>=2) {
                for(size_t j=0;j+1<e.pts.size();++j) segs.push_back({e.pts[j],e.pts[j+1]});
            }
        }
        auto chained=chainSegments(std::move(segs));
        for(auto& p:chained) res.cutContours.push_back(std::move(p));
    }

    // Очистка
    auto clean=[&](std::vector<Polygon>& polys){
        for(auto& poly:polys){poly.makeCCW();poly.removeDuplicates();}
        polys.erase(std::remove_if(polys.begin(),polys.end(),[&](const Polygon& p){
            bool d=p.verts.size()<3||p.area()<1.0;
            if(d) res.warnings.push_back("Удалён вырожденный контур");
            return d;
        }),polys.end());
    };
    clean(res.cutContours);
    clean(res.markContours);

    res.success=!res.cutContours.empty();
    if (!res.success)
        res.warnings.push_back(
            "Не найдено замкнутых контуров. "
            "Проверьте что контуры замкнуты и не в PAPER SPACE.");
    else
        res.warnings.push_back(
            "Загружено: "+std::to_string(res.cutContours.size())+
            " CUT + "+std::to_string(res.markContours.size())+" MARK контуров");

    return res;
}
