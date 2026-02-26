#include "LXDExporter.h"
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

void LXDExporter::writeContour(std::ofstream& f, const Polygon& poly, uint32_t type) {
    uint32_t cnt = (uint32_t)poly.verts.size();
    f.write(reinterpret_cast<const char*>(&type), 4);
    f.write(reinterpret_cast<const char*>(&cnt),  4);
    for (const auto& pt : poly.verts) {
        float fx = (float)pt.x, fy = (float)pt.y;
        f.write(reinterpret_cast<const char*>(&fx), 4);
        f.write(reinterpret_cast<const char*>(&fy), 4);
    }
}

bool LXDExporter::exportSheet(const Sheet& sheet, const std::string& filename) {
    std::ofstream f(filename, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    LXDHeader hdr;
    hdr.minX = 1e18; hdr.minY = 1e18;
    hdr.maxX = -1e18; hdr.maxY = -1e18;
    uint32_t contours = 0;

    for (const auto& pp : sheet.placed) {
        ++contours;
        contours += (uint32_t)pp.marks.size();
        for (const auto& v : pp.shape.verts) {
            hdr.minX = std::min(hdr.minX, v.x); hdr.minY = std::min(hdr.minY, v.y);
            hdr.maxX = std::max(hdr.maxX, v.x); hdr.maxY = std::max(hdr.maxY, v.y);
        }
    }

    if (sheet.placed.empty()) {
        hdr.minX = hdr.minY = 0;
        hdr.maxX = sheet.width; hdr.maxY = sheet.height;
    }
    hdr.contourCount = contours;

    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    for (const auto& pp : sheet.placed) {
        writeContour(f, pp.shape, 1);
        for (const auto& m : pp.marks)
            writeContour(f, m, 2);
    }
    return f.good();
}

bool LXDExporter::exportAllSheets(const std::vector<Sheet>& sheets,
                                   const std::string& folder) {
    fs::create_directories(folder);
    bool ok = true;
    for (size_t i = 0; i < sheets.size(); ++i) {
        std::ostringstream oss;
        oss << folder << "/sheet_"
            << std::setw(3) << std::setfill('0') << (i+1) << ".lxd";
        if (!exportSheet(sheets[i], oss.str())) ok = false;
    }
    return ok;
}
