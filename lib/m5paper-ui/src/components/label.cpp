#include "label.h"

namespace m5ui {

Label::Label(const String& text, uint8_t size) : Widget("label"), _text(text) {
    _style.size = size;
    _style.color = tok::kFg;
}

void Label::InvalidateLayout() {
    _wrapped_for_width = -1;
    _lines.clear();
    Invalidate();
}

void Label::SetText(const String& text) {
    if (_text == text) return;
    _text = text;
    InvalidateLayout();
}

void Label::SetSize(uint8_t size) {
    if (_style.size == size) return;
    _style.size = size;
    InvalidateLayout();
}

void Label::SetColor(uint8_t grey) {
    if (_style.color == grey) return;
    _style.color = grey;
    Invalidate();
}

void Label::SetBold(bool bold) {
    if (_style.bold == bold) return;
    _style.bold = bold;
    InvalidateLayout();
}

void Label::SetAlign(TextAlign align) {
    if (_align == align) return;
    _align = align;
    Invalidate();
}

void Label::SetMaxLines(uint16_t lines) {
    if (_max_lines == lines) return;
    _max_lines = lines;
    InvalidateLayout();
}

void Label::SetWrap(bool wrap) {
    if (_wrap == wrap) return;
    _wrap = wrap;
    InvalidateLayout();
}

void Label::EnsureWrapped(int16_t width) {
    if (_wrapped_for_width == width) return;

    if (_wrap) {
        _lines = Text().Wrap(_text, _style, width, _max_lines);
    } else {
        _lines.clear();
        if (_text.length() > 0) {
            _lines.push_back(TextLine{0, (uint16_t)_text.length(),
                                      Text().MeasureWidth(_text, _style),
                                      Text().MeasureWidth(_text, _style) > width});
        }
    }
    _wrapped_for_width = width;
}

Size Label::Measure(const Constraints& c) {
    if (_text.length() == 0) return c.Clamp(Size{0, 0});

    const int16_t avail = (int16_t)(c.max_w - _padding.Horizontal());
    EnsureWrapped(avail);

    int16_t widest = 0;
    for (const TextLine& l : _lines) {
        if (l.width > widest) widest = l.width;
    }
    const int16_t h =
        (int16_t)(Text().LineHeight(_style) * (int16_t)(_lines.empty() ? 1 : _lines.size()));

    return c.Clamp(Size{(int16_t)(widest + _padding.Horizontal()),
                        (int16_t)(h + _padding.Vertical())});
}

void Label::Layout(const Rect& bounds) {
    SetFrame(bounds);
    EnsureWrapped(ContentRect().w);
}

void Label::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr || _text.length() == 0) return;

    const Rect content = ContentRect();
    EnsureWrapped(content.w);

    const int16_t lh = Text().LineHeight(_style);
    TextStyle style = _style;
    if (!IsEnabled()) style.color = tok::kFgDisabled;

    int16_t y = content.y;
    for (const TextLine& l : _lines) {
        if (y + lh > content.Bottom()) break;

        String s = _text.substring(l.start, l.start + l.length);
        if (l.ellipsized) s = Text().Ellipsize(s, style, content.w);

        int16_t x = content.x;
        if (_align != TextAlign::Left) {
            const int16_t w = Text().MeasureWidth(s, style);
            x = (int16_t)(content.x + AlignOffset(_align == TextAlign::Center
                                                      ? Align::Center
                                                      : Align::End,
                                                  content.w, w));
        }

        const Rect line_rect = ctx.ToCanvas(Rect{x, y, content.w, lh});
        if (!line_rect.IsEmpty()) {
            Text().Draw(*ctx.canvas, s, line_rect.x, line_rect.y, style);
        }
        y = (int16_t)(y + lh);
    }
}

// -------------------------------------------------------------- Heading ----

Heading::Heading(const String& text, uint8_t level) : Label(text) {
    SetLevel(level);
}

void Heading::SetLevel(uint8_t level) {
    _level = level == 0 ? 1 : (level > 3 ? 3 : level);
    switch (_level) {
        case 1: SetSize(tok::kH1); break;
        case 2: SetSize(tok::kH2); break;
        default: SetSize(tok::kH3); break;
    }
    SetBold(_level <= 2);
}

}  // namespace m5ui
