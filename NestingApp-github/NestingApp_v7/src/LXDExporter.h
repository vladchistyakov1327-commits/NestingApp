#pragma once
#include "Sheet.h"
#include <string>
#include <vector>
#include <fstream>

class LXDExporter {
public:
    static bool exportSheet    (const Sheet& sheet, const std::string& filename);
    static bool exportAllSheets(const std::vector<Sheet>& sheets, const std::string& folder);

private:
#pragma pack(push, 1)
    struct LXDHeader {
        char     magic[4]  = {'L','X','D','1'};
        uint32_t version   = 0x0100;
        uint32_t units     = 1;    // мм
        double   minX = 0, minY = 0, maxX = 0, maxY = 0;
        uint32_t contourCount = 0;
        uint32_t reserved     = 0;
    };
#pragma pack(pop)

    static void writeContour(std::ofstream& f, const Polygon& poly, uint32_t type);
};
