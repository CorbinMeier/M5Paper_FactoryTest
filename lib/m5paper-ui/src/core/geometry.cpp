#include "geometry.h"

namespace m5ui {

namespace {
inline int16_t Min16(int16_t a, int16_t b) {
    return a < b ? a : b;
}
inline int16_t Max16(int16_t a, int16_t b) {
    return a > b ? a : b;
}
}  // namespace

Rect Rect::Union(const Rect& o) const {
    if (IsEmpty()) return o;
    if (o.IsEmpty()) return *this;

    const int16_t nx = Min16(x, o.x);
    const int16_t ny = Min16(y, o.y);
    const int16_t nr = Max16(Right(), o.Right());
    const int16_t nb = Max16(Bottom(), o.Bottom());
    return Rect{nx, ny, (int16_t)(nr - nx), (int16_t)(nb - ny)};
}

Rect Rect::Intersection(const Rect& o) const {
    const int16_t nx = Max16(x, o.x);
    const int16_t ny = Max16(y, o.y);
    const int16_t nr = Min16(Right(), o.Right());
    const int16_t nb = Min16(Bottom(), o.Bottom());
    if (nr <= nx || nb <= ny) return Rect{};
    return Rect{nx, ny, (int16_t)(nr - nx), (int16_t)(nb - ny)};
}

Rect Rect::Inflated(int16_t by) const {
    return Rect{(int16_t)(x - by), (int16_t)(y - by), (int16_t)(w + 2 * by),
                (int16_t)(h + 2 * by)};
}

Rect Rect::AlignedOut(int16_t grid) const {
    if (grid <= 1 || IsEmpty()) return *this;

    const int16_t nx = (int16_t)(x / grid * grid);
    const int16_t ny = (int16_t)(y / grid * grid);
    const int16_t nr = (int16_t)(((Right() + grid - 1) / grid) * grid);
    const int16_t nb = (int16_t)(((Bottom() + grid - 1) / grid) * grid);
    return Rect{nx, ny, (int16_t)(nr - nx), (int16_t)(nb - ny)};
}

}  // namespace m5ui
