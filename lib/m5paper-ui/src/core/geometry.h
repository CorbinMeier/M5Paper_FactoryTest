#pragma once
// Global display geometry (issue #27).
//
// The M5EPD panel is natively landscape; the framework applies ROTATE_90 at
// init, so every coordinate above the driver is portrait 540x960. Nothing
// outside this header may hardcode 540 or 960.

#include <stdint.h>

namespace m5ui {

// ---------------------------------------------------------------- panel ----

constexpr int16_t kDisplayW = 540;
constexpr int16_t kDisplayH = 960;
constexpr int16_t kDisplayCX = kDisplayW / 2;
constexpr int16_t kDisplayCY = kDisplayH / 2;

// 4 bits per pixel, 16 grey levels. 0 = black, 15 = white.
constexpr uint8_t kGreyBits = 4;
constexpr uint8_t kGreyLevels = 1 << kGreyBits;
constexpr uint8_t kBlack = 0;
constexpr uint8_t kWhite = 15;

// One full-screen 4bpp framebuffer, in bytes. 259,200 -- PSRAM only.
constexpr uint32_t kFramebufferBytes =
    (uint32_t)kDisplayW * (uint32_t)kDisplayH / 2;

// -------------------------------------------------------------- vectors ----

struct Point {
    int16_t x = 0;
    int16_t y = 0;
};

struct Size {
    int16_t w = 0;
    int16_t h = 0;

    bool IsEmpty() const {
        return w <= 0 || h <= 0;
    }
};

struct Rect {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;

    static Rect FullScreen() {
        return Rect{0, 0, kDisplayW, kDisplayH};
    }

    int16_t Right() const {
        return x + w;
    }
    int16_t Bottom() const {
        return y + h;
    }
    int16_t CenterX() const {
        return x + w / 2;
    }
    int16_t CenterY() const {
        return y + h / 2;
    }

    bool IsEmpty() const {
        return w <= 0 || h <= 0;
    }

    bool Contains(int16_t px, int16_t py) const {
        return px >= x && px < Right() && py >= y && py < Bottom();
    }

    bool Intersects(const Rect& o) const {
        return !(o.x >= Right() || o.Right() <= x || o.y >= Bottom() ||
                 o.Bottom() <= y);
    }

    // Smallest rect covering both. An empty operand is ignored.
    Rect Union(const Rect& o) const;

    // Overlapping area, or an empty rect when they do not touch.
    Rect Intersection(const Rect& o) const;

    // Grow (positive) or shrink (negative) on every edge.
    Rect Inflated(int16_t by) const;

    // Clamp to the panel bounds.
    Rect Clipped() const {
        return Intersection(FullScreen());
    }

    // Snap outward to a grid -- used by the tile-based ghost accounting so a
    // dirty region always covers whole tiles.
    Rect AlignedOut(int16_t grid) const;
};

}  // namespace m5ui
