#pragma once
#include "Geometry.h"
#include <string>
#include <vector>
#include <memory>

enum class LayerType { Cut, Mark };

// Трансформированная деталь на листе
struct PlacedPart {
    int    partId  = -1;
    Polygon shape;               // Контур в глобальных координатах листа
    std::vector<Polygon> marks;  // Гравировка в глобальных координатах
    Point  position;             // Позиция опорной точки (ref vertex) на листе
    double angle    = 0;
};

// Шаблон детали (из DXF, в локальных координатах)
// Локальная система: опорная точка (refVertex) = (0, 0) — нижняя левая вершина BBox
class Part {
public:
    int    id           = 0;
    std::string name;
    std::string sourceFile;

    Polygon            shape;       // В нормализованных локальных координатах
    std::vector<Polygon> marks;

    int requiredCount = 1;
    int placedCount   = 0;

    Part() = default;

    // После загрузки из DXF — нормализовать: сдвинуть чтобы refVertex = (0,0)
    void normalize();

    // Применить поворот и получить трансформированный контур
    // angle в градусах, вращение вокруг (0,0)
    Polygon transformedShape(double angleDeg) const;

    // Создать PlacedPart: поворот на angle, затем сдвиг так чтобы
    // нижняя-левая вершина BBox трансформированного контура = pos
    PlacedPart place(const Point& pos, double angleDeg) const;

    double area()       const { return shape.area(); }
    Rect   boundingBox() const { return shape.boundingBox(); }
    Rect   boundingBoxRotated(double angleDeg) const {
        return transformedShape(angleDeg).boundingBox();
    }

    bool isFullyPlaced() const { return placedCount >= requiredCount; }
    void resetPlacement()      { placedCount = 0; }

    std::unique_ptr<Part> clone() const { return std::make_unique<Part>(*this); }
};
