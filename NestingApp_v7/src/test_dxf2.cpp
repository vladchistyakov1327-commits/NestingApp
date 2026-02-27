
#include "DXFLoader.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Debug: проверим readPairs внутри
// Повторим логику parseEntities упрощённо

int main(int argc, char* argv[]) {
    std::ifstream f(argv[1], std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), {});
    for (auto& c:content) if(c=='\r') c='\n';
    
    // Читаем пары
    std::vector<std::pair<int,std::string>> P;
    std::istringstream in(content);
    std::string gl, vl;
    while (std::getline(in, gl)) {
        // trim
        while(!gl.empty()&&(gl.front()==' '||gl.front()=='\t')) gl.erase(0,1);
        while(!gl.empty()&&(gl.back()==' '||gl.back()=='\t'||gl.back()=='\r'||gl.back()=='\n')) gl.pop_back();
        if (!std::getline(in, vl)) break;
        while(!vl.empty()&&(vl.front()==' '||vl.front()=='\t')) vl.erase(0,1);
        while(!vl.empty()&&(vl.back()==' '||vl.back()=='\t'||vl.back()=='\r'||vl.back()=='\n')) vl.pop_back();
        int g=0;
        try { g=std::stoi(gl); } catch(...) { continue; }
        P.emplace_back(g,vl);
    }
    std::cout << "Total pairs: " << P.size() << "\n";
    
    // Найти SECTION ENTITIES
    bool found=false;
    for (size_t i=0; i<P.size(); ++i) {
        if (P[i].first==0 && P[i].second=="SECTION") {
            if (i+1<P.size() && P[i+1].first==2 && P[i+1].second=="ENTITIES") {
                std::cout << "Found SECTION ENTITIES at pair " << i << "\n";
                found=true;
                // Следующие 5 пар
                for(int k=0;k<5;++k) std::cout<<"  ["<<(i+2+k)<<"] "<<P[i+2+k].first<<" "<<P[i+2+k].second<<"\n";
            }
        }
    }
    if(!found) std::cout << "NOT FOUND!\n";
    
    // Реальный loadFile
    auto res = DXFLoader::loadFile(argv[1]);
    std::cout << (res.success?"OK":"FAIL") << " cut=" << res.cutContours.size() << "\n";
    for(auto& w:res.warnings) std::cout << "  W: " << w << "\n";
    return 0;
}
