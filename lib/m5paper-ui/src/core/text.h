#pragma once
// Text measurement and wrapping (issue #31).
//
// Nothing in the widget kit can lay out text today because there is no
// measureText equivalent -- every existing frame positions strings by eye.
// This is the measurement half; components/label.h is the drawing half.

#include <Arduino.h>
#include <M5EPD.h>

#include <functional>
#include <vector>

#include "geometry.h"

namespace m5ui {

struct TextMetrics {
    int16_t width = 0;
    int16_t height = 0;
    int16_t ascent = 0;
    int16_t descent = 0;
    uint16_t line_count = 0;
};

struct TextStyle {
    uint8_t size = 24;
    uint8_t color = 0;
    uint8_t bg = 15;
    float line_height = 1.35f;
    bool bold = false; // emulated by a 1px offset double-draw
};

// One wrapped line: a substring of the source plus where it goes.
struct TextLine {
    uint16_t start = 0; // byte offset into the source string
    uint16_t length = 0;
    int16_t width = 0;
    bool ellipsized = false;
};

class TextEngine {
   public:
    // Loads the bundled TTF into PSRAM. Called once from Device::Begin().
    bool Begin();
    bool IsReady() const {
        return _ready;
    }

    // Loads a font from storage instead of the bundled one.
    bool LoadFont(const char* path);

    // ------------------------------------------------------- measurement --
    int16_t MeasureWidth(const String& text, const TextStyle& style);
    int16_t LineHeight(const TextStyle& style) const;
    TextMetrics Measure(const String& text, const TextStyle& style,
                        int16_t max_width = kDisplayW);

    // Longest prefix of `text` that fits in `max_width`. Returns a byte count,
    // never splitting a UTF-8 sequence.
    uint16_t FitPrefix(const String& text, const TextStyle& style,
                       int16_t max_width);

    // ---------------------------------------------------------- wrapping --
    // Greedy word wrap, breaking mid-word only when a single word cannot fit.
    // `max_lines == 0` means unbounded; otherwise the last line is ellipsized.
    std::vector<TextLine> Wrap(const String& text, const TextStyle& style,
                               int16_t max_width, uint16_t max_lines = 0);

    // "A very long ti..." for a single line.
    String Ellipsize(const String& text, const TextStyle& style,
                     int16_t max_width);

    // ----------------------------------------------------------- drawing --
    // Applies the style to a canvas, then draws. Returns the advance width.
    int16_t Draw(M5EPD_Canvas& canvas, const String& text, int16_t x, int16_t y,
                 const TextStyle& style);

    // Draws pre-wrapped lines. Returns total height consumed.
    int16_t DrawLines(M5EPD_Canvas& canvas, const String& text,
                      const std::vector<TextLine>& lines, const Rect& bounds,
                      const TextStyle& style);

   private:
    void Apply(M5EPD_Canvas& canvas, const TextStyle& style);

    bool _ready = false;
    uint8_t _current_size = 0;
    // Measurement is the hot path in list and reader layout; a tiny cache of
    // (size, text hash) -> width keeps re-measure off the critical path.
    static constexpr uint8_t kCacheSlots = 16;
    struct CacheEntry {
        uint32_t key = 0;
        int16_t width = 0;
    };
    CacheEntry _cache[kCacheSlots];
    uint8_t _cache_head = 0;
};

// The framework-wide instance, owned by Device. Free function so components do
// not each need a Device reference just to measure a string.
TextEngine& Text();

// A small off-screen canvas kept solely so text can be measured without a real
// draw target. Owned by Device; null before Device::Begin(), in which case
// measurement falls back to a monospace estimate.
M5EPD_Canvas* ScratchCanvas();
void SetScratchCanvas(M5EPD_Canvas* canvas);

}  // namespace m5ui
