#pragma once
// Layout containers and box model (issue #30).
//
// The existing widget kit positions everything in absolute pixels. This adds a
// box model (margin/border/padding/content) and two solvers -- a linear
// row/column flow and a simple stack -- so screens stop hardcoding y offsets.

#include <stdint.h>

#include <vector>

#include "geometry.h"

namespace m5ui {

class Widget;

struct EdgeInsets {
    int16_t top = 0, right = 0, bottom = 0, left = 0;

    static EdgeInsets All(int16_t v) {
        return EdgeInsets{v, v, v, v};
    }
    static EdgeInsets Symmetric(int16_t vertical, int16_t horizontal) {
        return EdgeInsets{vertical, horizontal, vertical, horizontal};
    }
    static EdgeInsets Only(int16_t t, int16_t r, int16_t b, int16_t l) {
        return EdgeInsets{t, r, b, l};
    }

    int16_t Horizontal() const {
        return (int16_t)(left + right);
    }
    int16_t Vertical() const {
        return (int16_t)(top + bottom);
    }

    Rect Deflate(const Rect& r) const {
        return Rect{(int16_t)(r.x + left), (int16_t)(r.y + top),
                    (int16_t)(r.w - Horizontal()), (int16_t)(r.h - Vertical())};
    }
    Rect Inflate(const Rect& r) const {
        return Rect{(int16_t)(r.x - left), (int16_t)(r.y - top),
                    (int16_t)(r.w + Horizontal()), (int16_t)(r.h + Vertical())};
    }
};

enum class Axis : uint8_t { Vertical, Horizontal };

// Distribution along the main axis.
enum class Justify : uint8_t { Start, Center, End, SpaceBetween, SpaceAround };

// Alignment across the cross axis.
enum class Align : uint8_t { Start, Center, End, Stretch };

// Sizing constraints handed down a layout pass.
struct Constraints {
    int16_t min_w = 0, max_w = kDisplayW;
    int16_t min_h = 0, max_h = kDisplayH;

    static Constraints Loose(int16_t w, int16_t h) {
        return Constraints{0, w, 0, h};
    }
    static Constraints Tight(int16_t w, int16_t h) {
        return Constraints{w, w, h, h};
    }
    Size Clamp(Size s) const;
    Constraints Deflate(const EdgeInsets& pad) const;
};

// Per-child layout hints, stored on the Widget.
struct LayoutParams {
    EdgeInsets margin;
    Align cross_align = Align::Stretch;
    uint8_t flex = 0;      // 0 = size to content; >0 = share leftover space
    Size preferred;        // honoured when flex == 0 and content has no size
    bool absolute = false; // opt out of flow; use the widget's own rect
};

// Arranges `children` inside `bounds` and writes each child's frame. Shared by
// Column, Row and any screen that wants flow layout without a container.
void LayoutLinear(const std::vector<Widget*>& children, const Rect& bounds,
                  Axis axis, Justify justify, Align cross_align, int16_t gap,
                  const EdgeInsets& padding);

// Places every child at `bounds`, honouring per-child margins and alignment.
// Used by modals and by anything that overlays.
void LayoutStack(const std::vector<Widget*>& children, const Rect& bounds,
                 Align horizontal, Align vertical);

// Positions `inner` inside `outer` on one axis.
int16_t AlignOffset(Align a, int16_t outer, int16_t inner);

}  // namespace m5ui
