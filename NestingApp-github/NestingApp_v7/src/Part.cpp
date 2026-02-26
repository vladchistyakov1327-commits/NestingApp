#include "Part.h"

// Нормализуем: сдвигаем так чтобы нижняя-левая точка BBox стала (0,0)
void Part::normalize() {
    if (shape.empty()) return;
    shape.makeCCW();
    Rect bb = shape.boundingBox();
    double dx = -bb.x, dy = -bb.y;
    shape = shape.translated(dx, dy);
    for (auto& m : marks)
        m = m.translated(dx, dy);
}

// Трансформированный контур: поворот вокруг (0,0)
// (т.к. нормализовали — нижний левый угол в (0,0), поворот меняет форму)
Polygon Part::transformedShape(double angleDeg) const {
    if (std::abs(angleDeg) < GEO_EPS) return shape;

    // Поворот вокруг геометрического центра детали
    // Затем нормализуем снова (нижний левый угол → (0,0))
    Point c = shape.centroid();
    Polygon rotated = shape.rotatedAround(angleDeg, c);

    // Нормализуем
    Rect bb = rotated.boundingBox();
    return rotated.translated(-bb.x, -bb.y);
}

// Размещение: pos = нижняя-левая точка BBox размещённого контура
PlacedPart Part::place(const Point& pos, double angleDeg) const {
    PlacedPart pp;
    pp.partId   = id;
    pp.position = pos;
    pp.angle    = angleDeg;

    // Трансформируем контур и сдвигаем в нужную позицию
    Polygon xformed = transformedShape(angleDeg);
    // xformed.boundingBox().x == 0, .y == 0 (нормализован)
    pp.shape = xformed.translated(pos.x, pos.y);

    // Гравировка: rotate с тем же pivot что и shape, применить тот же bbox-offset
    // что был применён при нормализации transformedShape — иначе метки
    // теряют положение относительно контура.
    if (std::abs(angleDeg) > GEO_EPS) {
        // Шаг 1: поворот вокруг центроида (тот же pivot что в transformedShape)
        Point c = shape.centroid();

        // Шаг 2: вычисляем bbox-offset трансформированного shape ДО нормализации
        Rect shapeRotBB = shape.rotatedAround(angleDeg, c).boundingBox();
        double offX = -shapeRotBB.x + pos.x;
        double offY = -shapeRotBB.y + pos.y;

        for (const auto& m : marks) {
            Polygon mr = m.rotatedAround(angleDeg, c);
            pp.marks.push_back(mr.translated(offX, offY));
        }
    } else {
        for (const auto& m : marks)
            pp.marks.push_back(m.translated(pos.x, pos.y));
    }

    return pp;
}
