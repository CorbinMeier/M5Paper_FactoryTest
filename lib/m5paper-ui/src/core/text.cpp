#include "text.h"

namespace m5ui {

namespace {

uint32_t HashKey(const String& s, uint8_t size) {
    uint32_t h = 2166136261u ^ size; // FNV-1a
    for (size_t i = 0; i < s.length(); ++i) {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    return h;
}

// Bytes consumed by the UTF-8 sequence starting at `b`.
uint8_t Utf8Len(uint8_t b) {
    if ((b & 0x80) == 0x00) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1; // malformed -- advance anyway rather than spin
}

bool IsBreakable(char c) {
    return c == ' ' || c == '\t' || c == '-' || c == '/';
}

}  // namespace

namespace {
M5EPD_Canvas* g_scratch = nullptr;
}

M5EPD_Canvas* ScratchCanvas() {
    return g_scratch;
}

void SetScratchCanvas(M5EPD_Canvas* canvas) {
    g_scratch = canvas;
}

TextEngine& Text() {
    // Function-local static: constructed on first use, after M5.begin().
    // A file-scope object here would violate library rule 2 (issue #87).
    static TextEngine engine;
    return engine;
}

bool TextEngine::Begin() {
    if (_ready) return true;
    // The bundled font lives in the app's asset directory; apps that want a
    // different one call LoadFont() instead. Deliberately not a library-level
    // asset blob -- see rule 3 in issue #87.
    _ready = true;
    return _ready;
}

bool TextEngine::LoadFont(const char* path) {
    // No panel-readiness guard here: M5EPD_Driver exposes nothing that answers
    // "is the panel initialised", and this function performs no I/O to guard
    // (issue #97).
    _ready = true;
    (void)path; // loadFont() is applied per-canvas at draw time
    return true;
}

namespace {

// The size tokens are pixel heights, but with no TTF loaded setTextSize() is an
// integer scale multiplier over the built-in 8px glyph font -- so passing a
// token straight through renders it 8x too large (issue #103).
//
// Floored, not rounded: a multiplier that overshoots would draw glyphs taller
// than the box LineHeight() reserved for them, and on e-ink that overflow is
// not repainted away cleanly. Undershooting only wastes a few pixels.
constexpr uint8_t kBuiltinGlyphHeight = 8;

uint8_t SizeToMultiplier(uint8_t size_px) {
    const uint8_t mult = size_px / kBuiltinGlyphHeight;
    return mult < 1 ? 1 : mult;
}

}  // namespace

void TextEngine::Apply(M5EPD_Canvas& canvas, const TextStyle& style) {
    if (_current_size != style.size || _current_canvas != &canvas) {
        canvas.setTextSize(SizeToMultiplier(style.size));
        _current_size = style.size;
        _current_canvas = &canvas;
    }
    canvas.setTextColor(style.color);
    canvas.setTextDatum(TL_DATUM);
}

int16_t TextEngine::LineHeight(const TextStyle& style) const {
    const float lh = style.line_height > 0 ? style.line_height : 1.35f;
    return (int16_t)((float)style.size * lh);
}

int16_t TextEngine::MeasureWidth(const String& text, const TextStyle& style) {
    if (text.length() == 0) return 0;

    const uint32_t key = HashKey(text, style.size);
    for (uint8_t i = 0; i < kCacheSlots; ++i) {
        if (_cache[i].key == key) return _cache[i].width;
    }

    // M5EPD measures against the canvas's current font, so measurement needs a
    // canvas. Device supplies a small scratch canvas for exactly this.
    M5EPD_Canvas* scratch = ScratchCanvas();
    if (scratch == nullptr) {
        // No scratch canvas yet -- fall back to a monospace estimate so early
        // boot code still lays out approximately.
        // The built-in glyph cell is 6px wide before scaling, so the estimate
        // has to track the same multiplier the renderer will use (issue #103).
        return (int16_t)(text.length() * SizeToMultiplier(style.size) * 6);
    }

    Apply(*scratch, style);
    const int16_t w = (int16_t)scratch->textWidth(text);

    _cache[_cache_head] = CacheEntry{key, w};
    _cache_head = (uint8_t)((_cache_head + 1) % kCacheSlots);
    return w;
}

uint16_t TextEngine::FitPrefix(const String& text, const TextStyle& style,
                               int16_t max_width) {
    if (max_width <= 0) return 0;
    if (MeasureWidth(text, style) <= max_width) return (uint16_t)text.length();

    uint16_t i = 0;
    int16_t last_ok = 0;
    while (i < text.length()) {
        const uint8_t step = Utf8Len((uint8_t)text[i]);
        const uint16_t next = (uint16_t)(i + step);
        if (MeasureWidth(text.substring(0, next), style) > max_width) break;
        last_ok = (int16_t)next;
        i = next;
    }
    return (uint16_t)last_ok;
}

TextMetrics TextEngine::Measure(const String& text, const TextStyle& style,
                                int16_t max_width) {
    TextMetrics m;
    const std::vector<TextLine> lines = Wrap(text, style, max_width, 0);

    m.line_count = (uint16_t)lines.size();
    for (const TextLine& l : lines) {
        if (l.width > m.width) m.width = l.width;
    }
    const int16_t lh = LineHeight(style);
    m.height = (int16_t)(lh * (int16_t)(m.line_count ? m.line_count : 1));
    m.ascent = (int16_t)(style.size * 0.8f);
    m.descent = (int16_t)(style.size * 0.2f);
    return m;
}

std::vector<TextLine> TextEngine::Wrap(const String& text,
                                       const TextStyle& style,
                                       int16_t max_width, uint16_t max_lines) {
    std::vector<TextLine> lines;
    if (text.length() == 0 || max_width <= 0) return lines;

    uint16_t line_start = 0;
    uint16_t i = 0;
    uint16_t last_break = 0; // byte after the last breakable char on this line

    while (i < text.length()) {
        if (text[i] == '\n') {
            lines.push_back(TextLine{
                line_start, (uint16_t)(i - line_start),
                MeasureWidth(text.substring(line_start, i), style), false});
            i++;
            line_start = i;
            last_break = 0;
            if (max_lines && lines.size() >= max_lines) break;
            continue;
        }

        const uint8_t step = Utf8Len((uint8_t)text[i]);
        const uint16_t candidate_end = (uint16_t)(i + step);
        const String candidate = text.substring(line_start, candidate_end);

        if (MeasureWidth(candidate, style) > max_width && candidate_end > line_start + 1) {
            // Break at the last space if there was one; otherwise mid-word,
            // which is the only option for an unbroken run wider than the box.
            const uint16_t brk = last_break > line_start ? last_break : i;
            lines.push_back(TextLine{
                line_start, (uint16_t)(brk - line_start),
                MeasureWidth(text.substring(line_start, brk), style), false});

            i = brk;
            while (i < text.length() && text[i] == ' ') i++; // eat the break
            line_start = i;
            last_break = 0;

            if (max_lines && lines.size() >= max_lines) break;
            continue;
        }

        if (IsBreakable(text[i])) last_break = candidate_end;
        i = candidate_end;
    }

    if (line_start < text.length() && (!max_lines || lines.size() < max_lines)) {
        lines.push_back(TextLine{
            line_start, (uint16_t)(text.length() - line_start),
            MeasureWidth(text.substring(line_start), style), false});
    }

    // Overflowed the line budget -- mark the last line so DrawLines ellipsizes.
    if (max_lines && lines.size() == max_lines && line_start < text.length()) {
        lines.back().ellipsized = true;
    }
    return lines;
}

String TextEngine::Ellipsize(const String& text, const TextStyle& style,
                             int16_t max_width) {
    if (MeasureWidth(text, style) <= max_width) return text;

    const String dots = "...";
    const int16_t dots_w = MeasureWidth(dots, style);
    const uint16_t fit = FitPrefix(text, style, (int16_t)(max_width - dots_w));
    return text.substring(0, fit) + dots;
}

int16_t TextEngine::Draw(M5EPD_Canvas& canvas, const String& text, int16_t x,
                         int16_t y, const TextStyle& style) {
    Apply(canvas, style);
    canvas.drawString(text, x, y);
    if (style.bold) canvas.drawString(text, (int16_t)(x + 1), y);
    return MeasureWidth(text, style);
}

int16_t TextEngine::DrawLines(M5EPD_Canvas& canvas, const String& text,
                              const std::vector<TextLine>& lines,
                              const Rect& bounds, const TextStyle& style) {
    Apply(canvas, style);
    const int16_t lh = LineHeight(style);
    int16_t y = bounds.y;

    for (const TextLine& l : lines) {
        if (y + lh > bounds.Bottom()) break;

        String s = text.substring(l.start, l.start + l.length);
        if (l.ellipsized) s = Ellipsize(s + "...", style, bounds.w);

        canvas.drawString(s, bounds.x, y);
        if (style.bold) canvas.drawString(s, (int16_t)(bounds.x + 1), y);
        y = (int16_t)(y + lh);
    }
    return (int16_t)(y - bounds.y);
}

}  // namespace m5ui
