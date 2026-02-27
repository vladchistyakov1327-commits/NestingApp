
#include "DXFLoader.h"
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    auto res = DXFLoader::loadFile(argv[1]);
    std::cout << (res.success ? "OK" : "FAIL");
    std::cout << " cut=" << res.cutContours.size();
    std::cout << " mark=" << res.markContours.size();
    for (auto& w : res.warnings) std::cout << " | " << w;
    std::cout << std::endl;
    return res.success ? 0 : 1;
}
