#include "progressbar.h"

#include "../core/text.h"

namespace m5ui {

namespace {

// Diagonal hatching, used where a colour fill would be the obvious choice on a
// colour display.
void FillHatched(M5EPD_Canvas& canvas, const Rect& r, uint8_t grey,
                 int16_t spacing = 6) {
    for (int16_t x = r.x - r.h; x < r.Right(); x += spacing) {
        canvas.drawLine(x, r.Bottom(), (int16_t)(x + r.h), r.y, grey);
    }
}

}  // namespace

ProgressBar::ProgressBar() : Widget("progressbar") {}

void ProgressBar::SetValue(float value) {
    const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    // Repaint only on a visible change -- a progress bar updated per byte
    // would otherwise thrash the panel.
    if ((int)(clamped * 100) == (int)(_value * 100)) {
        _value = clamped;
        return;
    }
    _value = clamped;
    Invalidate();
}

void ProgressBar::SetIndeterminate(bool v) {
    if (_indeterminate == v) return;
    _indeterminate = v;
    Invalidate();
}

void ProgressBar::SetThickness(int16_t px) {
    if (_thickness == px) return;
    _thickness = px;
    Invalidate();
}

Size ProgressBar::Measure(const Constraints& c) {
    return c.Clamp(Size{c.max_w, (int16_t)(_thickness + _padding.Vertical())});
}

void ProgressBar::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;

    const Rect content = ContentRect();
    const Rect r = ctx.ToCanvas(Rect{content.x, content.y, content.w, _thickness});
    if (r.IsEmpty()) return;

    ctx.canvas->fillRoundRect(r.x, r.y, r.w, r.h, tok::kRadiusSm,
                             tok::kSurfaceSunken);
    ctx.canvas->drawRoundRect(r.x, r.y, r.w, r.h, tok::kRadiusSm, tok::kBorder);

    if (_indeterminate) {
        FillHatched(*ctx.canvas, r, tok::kFgMuted);
        return;
    }

    const int16_t fill_w = (int16_t)((float)(r.w - 2) * _value);
    if (fill_w > 0) {
        ctx.canvas->fillRoundRect((int16_t)(r.x + 1), (int16_t)(r.y + 1), fill_w,
                                 (int16_t)(r.h - 2), tok::kRadiusSm, tok::kFg);
    }
}

// ---------------------------------------------------------------- Meter ----

Meter::Meter(const String& caption) : _caption(caption) {
    _type_name = "meter";
    _thickness = 10;
}

void Meter::SetCaption(const String& caption) {
    if (_caption == caption) return;
    _caption = caption;
    Invalidate();
}

void Meter::SetValueText(const String& text) {
    if (_value_text == text) return;
    _value_text = text;
    Invalidate();
}

void Meter::SetWarnBelow(float fraction) {
    _warn_below = fraction;
    Invalidate();
}

Size Meter::Measure(const Constraints& c) {
    TextStyle style;
    style.size = tok::kTextSm;
    const int16_t label_h = (int16_t)(Text().LineHeight(style) + tok::kSpaceXs);
    return c.Clamp(Size{c.max_w, (int16_t)(label_h + _thickness + _padding.Vertical())});
}

void Meter::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;

    const Rect content = ContentRect();
    const Rect r = ctx.ToCanvas(content);
    if (r.IsEmpty()) return;

    TextStyle style;
    style.size = tok::kTextSm;
    style.color = tok::kFg;
    const int16_t label_h = Text().LineHeight(style);

    if (_caption.length() > 0) {
        Text().Draw(*ctx.canvas, _caption, r.x, r.y, style);
    }
    if (_value_text.length() > 0) {
        TextStyle right = style;
        right.color = tok::kFgMuted;
        const int16_t w = Text().MeasureWidth(_value_text, right);
        Text().Draw(*ctx.canvas, _value_text, (int16_t)(r.Right() - w), r.y, right);
    }

    const Rect track{r.x, (int16_t)(r.y + label_h + tok::kSpaceXs), r.w,
                     _thickness};
    ctx.canvas->fillRoundRect(track.x, track.y, track.w, track.h, tok::kRadiusSm,
                             tok::kSurfaceSunken);
    ctx.canvas->drawRoundRect(track.x, track.y, track.w, track.h, tok::kRadiusSm,
                             tok::kBorder);

    const int16_t fill_w = (int16_t)((float)(track.w - 2) * _value);
    if (fill_w <= 0) return;

    const Rect fill{(int16_t)(track.x + 1), (int16_t)(track.y + 1), fill_w,
                    (int16_t)(track.h - 2)};

    if (_warn_below > 0.0f && _value < _warn_below) {
        FillHatched(*ctx.canvas, fill, tok::kFg, 4);
        ctx.canvas->drawRect(fill.x, fill.y, fill.w, fill.h, tok::kFg);
    } else {
        ctx.canvas->fillRoundRect(fill.x, fill.y, fill.w, fill.h, tok::kRadiusSm,
                                 tok::kFg);
    }
}

}  // namespace m5ui
