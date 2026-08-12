#include "container.h"

namespace m5ui {

Container::Container(Axis axis) : Widget("container"), _axis(axis) {}

Container* Container::SetAxis(Axis a) {
    _axis = a;
    Invalidate();
    return this;
}

Container* Container::SetGap(int16_t gap) {
    _gap = gap;
    Invalidate();
    return this;
}

Container* Container::SetJustify(Justify j) {
    _justify = j;
    Invalidate();
    return this;
}

Container* Container::SetCrossAlign(Align a) {
    _cross_align = a;
    Invalidate();
    return this;
}

Container* Container::SetBackground(uint8_t grey) {
    _background = grey;
    Invalidate();
    return this;
}

Container* Container::SetBorder(uint8_t grey, int16_t width) {
    _border_color = grey;
    _border_width = width;
    Invalidate();
    return this;
}

Container* Container::SetRadius(int16_t radius) {
    _radius = radius;
    Invalidate();
    return this;
}

Size Container::Measure(const Constraints& c) {
    const Constraints inner = c.Deflate(_padding);

    int16_t main = 0;
    int16_t cross = 0;
    uint16_t counted = 0;

    for (Widget* child : _children) {
        if (child == nullptr || !child->IsVisible() || child->Params().absolute) {
            continue;
        }
        counted++;

        const EdgeInsets& m = child->Params().margin;
        const Size s = child->Measure(inner);

        if (_axis == Axis::Vertical) {
            main = (int16_t)(main + s.h + m.Vertical());
            cross = max(cross, (int16_t)(s.w + m.Horizontal()));
        } else {
            main = (int16_t)(main + s.w + m.Horizontal());
            cross = max(cross, (int16_t)(s.h + m.Vertical()));
        }
    }

    if (counted > 1) main = (int16_t)(main + _gap * (int16_t)(counted - 1));

    const Size total =
        _axis == Axis::Vertical
            ? Size{(int16_t)(cross + _padding.Horizontal()),
                   (int16_t)(main + _padding.Vertical())}
            : Size{(int16_t)(main + _padding.Horizontal()),
                   (int16_t)(cross + _padding.Vertical())};
    return c.Clamp(total);
}

void Container::Layout(const Rect& bounds) {
    SetFrame(bounds);
    LayoutLinear(_children, bounds, _axis, _justify, _cross_align, _gap, _padding);
}

void Container::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;
    if (_background == 255 && _border_color == 255) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    if (_background != 255) {
        if (_radius > 0) {
            ctx.canvas->fillRoundRect(r.x, r.y, r.w, r.h, _radius, _background);
        } else {
            ctx.canvas->fillRect(r.x, r.y, r.w, r.h, _background);
        }
    }
    if (_border_color != 255 && _border_width > 0) {
        for (int16_t i = 0; i < _border_width; ++i) {
            if (_radius > 0) {
                ctx.canvas->drawRoundRect((int16_t)(r.x + i), (int16_t)(r.y + i),
                                          (int16_t)(r.w - 2 * i),
                                          (int16_t)(r.h - 2 * i), _radius,
                                          _border_color);
            } else {
                ctx.canvas->drawRect((int16_t)(r.x + i), (int16_t)(r.y + i),
                                     (int16_t)(r.w - 2 * i),
                                     (int16_t)(r.h - 2 * i), _border_color);
            }
        }
    }
}

// ---------------------------------------------------------------- sugar ----

Column::Column(int16_t gap) : Container(Axis::Vertical) {
    _gap = gap;
}

Row::Row(int16_t gap) : Container(Axis::Horizontal) {
    _gap = gap;
    _cross_align = Align::Center;
}

Card::Card(Axis axis) : Container(axis) {
    _background = tok::kSurface;
    _border_color = tok::kBorder;
    _border_width = tok::kBorderWidth;
    _radius = tok::kRadiusMd;
    SetPadding(EdgeInsets::All(tok::kSpaceMd));
}

// -------------------------------------------------------------- Divider ----

Divider::Divider(Axis axis) : Widget("divider"), _axis(axis) {}

Size Divider::Measure(const Constraints& c) {
    return _axis == Axis::Horizontal ? Size{c.max_w, 1} : Size{1, c.max_h};
}

void Divider::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;
    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    if (_axis == Axis::Horizontal) {
        ctx.canvas->drawFastHLine(r.x, r.y, r.w, tok::kBorder);
    } else {
        ctx.canvas->drawFastVLine(r.x, r.y, r.h, tok::kBorder);
    }
}

// --------------------------------------------------------------- Spacer ----

Spacer::Spacer(int16_t size) : Widget("spacer"), _size(size) {
    if (size == 0) _params.flex = 1;
}

Size Spacer::Measure(const Constraints& c) {
    if (_params.flex > 0) return Size{0, 0};
    return c.Clamp(Size{_size, _size});
}

}  // namespace m5ui
