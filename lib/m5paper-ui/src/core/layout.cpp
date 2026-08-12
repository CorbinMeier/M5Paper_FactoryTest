#include "layout.h"

#include "widget.h"

namespace m5ui {

namespace {
inline int16_t Clamp16(int16_t v, int16_t lo, int16_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
}  // namespace

Size Constraints::Clamp(Size s) const {
    return Size{Clamp16(s.w, min_w, max_w), Clamp16(s.h, min_h, max_h)};
}

Constraints Constraints::Deflate(const EdgeInsets& pad) const {
    Constraints c = *this;
    c.max_w = (int16_t)(c.max_w - pad.Horizontal());
    c.max_h = (int16_t)(c.max_h - pad.Vertical());
    if (c.max_w < 0) c.max_w = 0;
    if (c.max_h < 0) c.max_h = 0;
    c.min_w = Clamp16(c.min_w, 0, c.max_w);
    c.min_h = Clamp16(c.min_h, 0, c.max_h);
    return c;
}

int16_t AlignOffset(Align a, int16_t outer, int16_t inner) {
    switch (a) {
        case Align::Center: return (int16_t)((outer - inner) / 2);
        case Align::End:    return (int16_t)(outer - inner);
        case Align::Start:
        case Align::Stretch:
        default:            return 0;
    }
}

void LayoutLinear(const std::vector<Widget*>& children, const Rect& bounds,
                  Axis axis, Justify justify, Align cross_align, int16_t gap,
                  const EdgeInsets& padding) {
    const Rect box = padding.Deflate(bounds);
    if (box.IsEmpty()) return;

    const bool vertical = axis == Axis::Vertical;
    const int16_t main_extent = vertical ? box.h : box.w;
    const int16_t cross_extent = vertical ? box.w : box.h;

    // ---- pass 1: measure the inflexible children, tally flex weights ------
    std::vector<Size> sizes(children.size());
    int16_t used = 0;
    uint16_t total_flex = 0;
    uint16_t laid_out = 0;

    for (size_t i = 0; i < children.size(); ++i) {
        Widget* c = children[i];
        if (c == nullptr || !c->IsVisible() || c->Params().absolute) continue;
        laid_out++;

        const EdgeInsets& m = c->Params().margin;
        if (c->Params().flex > 0) {
            total_flex += c->Params().flex;
            used = (int16_t)(used + (vertical ? m.Vertical() : m.Horizontal()));
            continue;
        }

        const Constraints cc = Constraints::Loose(
            (int16_t)(cross_extent - m.Horizontal()),
            (int16_t)(main_extent - m.Vertical()));
        sizes[i] = c->Measure(vertical ? cc : Constraints::Loose(
                                                  (int16_t)(main_extent - m.Horizontal()),
                                                  (int16_t)(cross_extent - m.Vertical())));
        used = (int16_t)(used + (vertical ? sizes[i].h + m.Vertical()
                                          : sizes[i].w + m.Horizontal()));
    }

    if (laid_out > 1) used = (int16_t)(used + gap * (int16_t)(laid_out - 1));

    // ---- pass 2: hand the leftover to the flexible children ---------------
    int16_t leftover = (int16_t)(main_extent - used);
    if (leftover < 0) leftover = 0;

    if (total_flex > 0) {
        int16_t remaining = leftover;
        uint16_t remaining_flex = total_flex;
        for (size_t i = 0; i < children.size(); ++i) {
            Widget* c = children[i];
            if (c == nullptr || !c->IsVisible() || c->Params().absolute) continue;
            if (c->Params().flex == 0) continue;

            // Divide down rather than multiplying up, so rounding error does
            // not accumulate into a visible gap at the end of the run.
            const int16_t share =
                (int16_t)((int32_t)remaining * c->Params().flex / remaining_flex);
            remaining = (int16_t)(remaining - share);
            remaining_flex = (uint16_t)(remaining_flex - c->Params().flex);

            const EdgeInsets& m = c->Params().margin;
            if (vertical) {
                sizes[i] = Size{(int16_t)(cross_extent - m.Horizontal()), share};
            } else {
                sizes[i] = Size{share, (int16_t)(cross_extent - m.Vertical())};
            }
        }
        leftover = 0;
    }

    // ---- pass 3: place ----------------------------------------------------
    int16_t cursor = vertical ? box.y : box.x;
    int16_t extra_gap = 0;

    switch (justify) {
        case Justify::Center:
            cursor = (int16_t)(cursor + leftover / 2);
            break;
        case Justify::End:
            cursor = (int16_t)(cursor + leftover);
            break;
        case Justify::SpaceBetween:
            if (laid_out > 1) extra_gap = (int16_t)(leftover / (laid_out - 1));
            break;
        case Justify::SpaceAround:
            if (laid_out > 0) {
                extra_gap = (int16_t)(leftover / laid_out);
                cursor = (int16_t)(cursor + extra_gap / 2);
            }
            break;
        case Justify::Start:
        default:
            break;
    }

    bool first = true;
    for (size_t i = 0; i < children.size(); ++i) {
        Widget* c = children[i];
        if (c == nullptr || !c->IsVisible() || c->Params().absolute) continue;

        if (!first) cursor = (int16_t)(cursor + gap + extra_gap);
        first = false;

        const EdgeInsets& m = c->Params().margin;
        const Align ca = c->Params().cross_align;

        Rect frame;
        if (vertical) {
            const int16_t cw = ca == Align::Stretch
                                   ? (int16_t)(cross_extent - m.Horizontal())
                                   : sizes[i].w;
            frame.x = (int16_t)(box.x + m.left +
                                AlignOffset(ca, (int16_t)(cross_extent - m.Horizontal()), cw));
            frame.y = (int16_t)(cursor + m.top);
            frame.w = cw;
            frame.h = sizes[i].h;
            cursor = (int16_t)(cursor + sizes[i].h + m.Vertical());
        } else {
            const int16_t ch = ca == Align::Stretch
                                   ? (int16_t)(cross_extent - m.Vertical())
                                   : sizes[i].h;
            frame.x = (int16_t)(cursor + m.left);
            frame.y = (int16_t)(box.y + m.top +
                                AlignOffset(ca, (int16_t)(cross_extent - m.Vertical()), ch));
            frame.w = sizes[i].w;
            frame.h = ch;
            cursor = (int16_t)(cursor + sizes[i].w + m.Horizontal());
        }
        c->Layout(frame);
    }
}

void LayoutStack(const std::vector<Widget*>& children, const Rect& bounds,
                 Align horizontal, Align vertical) {
    for (Widget* c : children) {
        if (c == nullptr || !c->IsVisible() || c->Params().absolute) continue;

        const EdgeInsets& m = c->Params().margin;
        const Rect box = m.Deflate(bounds);
        const Size s = c->Measure(Constraints::Loose(box.w, box.h));

        const int16_t w = horizontal == Align::Stretch ? box.w : s.w;
        const int16_t h = vertical == Align::Stretch ? box.h : s.h;
        c->Layout(Rect{(int16_t)(box.x + AlignOffset(horizontal, box.w, w)),
                       (int16_t)(box.y + AlignOffset(vertical, box.h, h)), w, h});
    }
}

}  // namespace m5ui
