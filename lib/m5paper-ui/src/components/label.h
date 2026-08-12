#pragma once
// Label / Text (issue #43) and Heading (issue #44).
//
// Sizes to content, wraps, ellipsizes and honours a max-line count. This is the
// component every other one draws its text through.

#include <Arduino.h>

#include <vector>

#include "../core/text.h"
#include "../core/widget.h"

namespace m5ui {

enum class TextAlign : uint8_t { Left, Center, Right };

class Label : public Widget {
   public:
    explicit Label(const String& text = "", uint8_t size = tok::kTextMd);

    void SetText(const String& text);
    const String& GetText() const {
        return _text;
    }

    void SetSize(uint8_t size);
    void SetColor(uint8_t grey);
    void SetBold(bool bold);
    void SetAlign(TextAlign align);
    // 0 = unbounded. Beyond this, the last visible line is ellipsized.
    void SetMaxLines(uint16_t lines);
    void SetWrap(bool wrap);

    const TextStyle& Style() const {
        return _style;
    }

    Size Measure(const Constraints& c) override;
    void Layout(const Rect& bounds) override;
    void DrawSelf(PaintContext& ctx) override;

    DrawIntentHint PaintIntent() const override {
        return DrawIntentHint::Text;
    }

   private:
    void InvalidateLayout();
    void EnsureWrapped(int16_t width);

    String _text;
    TextStyle _style;
    TextAlign _align = TextAlign::Left;
    uint16_t _max_lines = 0;
    bool _wrap = true;

    // Wrapping is the expensive part; cache it against the width it was
    // computed for so a repaint does not re-wrap.
    std::vector<TextLine> _lines;
    int16_t _wrapped_for_width = -1;
};

// Heading: a Label pinned to the type scale (issue #44). Level 1-3.
class Heading : public Label {
   public:
    explicit Heading(const String& text, uint8_t level = 1);
    void SetLevel(uint8_t level);

   private:
    uint8_t _level = 1;
};

}  // namespace m5ui
