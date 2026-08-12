#include "button.h"

namespace m5ui {

Button::Button(const String& label, ButtonVariant variant)
    : Widget("button"), _label(label), _variant(variant) {
    _style.size = tok::kTextMd;
    _focusable = true;
    SetPadding(EdgeInsets::Symmetric(tok::kSpaceSm, tok::kSpaceMd));
}

void Button::SetLabel(const String& label) {
    if (_label == label) return;
    _label = label;
    Invalidate();
}

void Button::SetVariant(ButtonVariant v) {
    if (_variant == v) return;
    _variant = v;
    Invalidate();
}

void Button::SetTextSize(uint8_t size) {
    if (_style.size == size) return;
    _style.size = size;
    Invalidate();
}

Button* Button::OnPress(std::function<void()> handler) {
    OnTap([h = std::move(handler)](Widget&) {
        if (h) h();
    });
    return this;
}

Size Button::Measure(const Constraints& c) {
    const int16_t text_w = Text().MeasureWidth(_label, _style);
    const int16_t text_h = Text().LineHeight(_style);

    Size s{(int16_t)(text_w + _padding.Horizontal()),
           (int16_t)(text_h + _padding.Vertical())};

    // Never render a tappable box below the minimum touch target -- a 24px
    // label with 8px padding is a 40px box, which is a miss on e-ink.
    if (s.h < tok::kTouchTargetMin) s.h = tok::kTouchTargetMin;
    if (s.w < tok::kTouchTargetMin) s.w = tok::kTouchTargetMin;
    return c.Clamp(s);
}

void Button::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    const bool inverted = _pressed || (_variant == ButtonVariant::Filled);
    uint8_t fill = tok::kSurface;
    uint8_t fg = tok::kFg;

    if (!IsEnabled()) {
        fg = tok::kFgDisabled;
    } else if (inverted) {
        fill = tok::kInverseBg;
        fg = tok::kInverseFg;
    }

    if (inverted || _variant != ButtonVariant::Ghost) {
        ctx.canvas->fillRoundRect(r.x, r.y, r.w, r.h, tok::kRadiusMd, fill);
    }
    if (_variant == ButtonVariant::Outline && !_pressed) {
        for (int16_t i = 0; i < tok::kBorderWidth; ++i) {
            ctx.canvas->drawRoundRect((int16_t)(r.x + i), (int16_t)(r.y + i),
                                      (int16_t)(r.w - 2 * i),
                                      (int16_t)(r.h - 2 * i), tok::kRadiusMd,
                                      IsEnabled() ? tok::kBorderStrong : tok::kBorder);
        }
    }

    TextStyle style = _style;
    style.color = fg;

    const int16_t tw = Text().MeasureWidth(_label, style);
    const int16_t th = Text().LineHeight(style);
    Text().Draw(*ctx.canvas, _label, (int16_t)(r.x + (r.w - tw) / 2),
                (int16_t)(r.y + (r.h - th) / 2), style);
}

}  // namespace m5ui
