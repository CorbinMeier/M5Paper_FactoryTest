#include "textinput.h"

#include "../core/device.h"

namespace m5ui {

namespace {
constexpr int16_t kCaretWidth = 2;
}

TextInput::TextInput() : Widget("textinput") {
    _focusable = true;
    _style.size = tok::kTextMd;
    SetPadding(EdgeInsets::Symmetric(tok::kSpaceSm, tok::kSpaceSm));
}

void TextInput::SetValue(const String& value) {
    if (_value == value) return;
    _value = value;
    if (_caret > _value.length()) _caret = (uint16_t)_value.length();
    ClearSelection();
    Invalidate();
    if (_on_change) _on_change(_value);
}

void TextInput::SetPlaceholder(const String& placeholder) {
    _placeholder = placeholder;
    Invalidate();
}

void TextInput::SetMaxLength(uint16_t max_length) {
    _max_length = max_length;
}

void TextInput::SetPassword(bool password) {
    if (_password == password) return;
    _password = password;
    Invalidate();
}

void TextInput::SetTextSize(uint8_t size) {
    if (_style.size == size) return;
    _style.size = size;
    Invalidate();
}

String TextInput::DisplayString() const {
    if (!_password) return _value;

    String dots;
    dots.reserve(_value.length());
    for (size_t i = 0; i < _value.length(); ++i) dots += '*';
    return dots;
}

// ---------------------------------------------------------------- caret ----

void TextInput::SetCaretIndex(uint16_t index) {
    const uint16_t clamped =
        index > _value.length() ? (uint16_t)_value.length() : index;
    if (_caret == clamped) return;
    _caret = clamped;
    Invalidate();
}

void TextInput::MoveCaret(int16_t by) {
    const int32_t target = (int32_t)_caret + by;
    SetCaretIndex((uint16_t)(target < 0 ? 0 : target));
}

void TextInput::SelectAll() {
    _sel_anchor = 0;
    _caret = (uint16_t)_value.length();
    Invalidate();
}

void TextInput::ClearSelection() {
    if (_sel_anchor < 0) return;
    _sel_anchor = -1;
    Invalidate();
}

void TextInput::DeleteSelection() {
    if (!HasSelection()) return;

    const uint16_t a = (uint16_t)_sel_anchor;
    const uint16_t lo = a < _caret ? a : _caret;
    const uint16_t hi = a < _caret ? _caret : a;

    _value = _value.substring(0, lo) + _value.substring(hi);
    _caret = lo;
    _sel_anchor = -1;
}

void TextInput::InsertCodepoint(uint16_t cp) {
    if (_max_length > 0 && _value.length() >= _max_length) return;
    DeleteSelection();
    _value = _value.substring(0, _caret) + String((char)cp) + _value.substring(_caret);
    _caret++;
}

uint16_t TextInput::IndexFromX(int16_t x) const {
    const Rect content = ContentRect();
    const int16_t target = (int16_t)(x - content.x + _scroll_x);
    if (target <= 0) return 0;

    // Linear scan: an input field holds tens of characters, so a binary search
    // would not pay for its own complexity.
    TextEngine& text = Text();
    for (uint16_t i = 1; i <= _value.length(); ++i) {
        if (text.MeasureWidth(_value.substring(0, i), _style) > target) {
            return (uint16_t)(i - 1);
        }
    }
    return (uint16_t)_value.length();
}

// ---------------------------------------------------------------- input ----

bool TextInput::ApplyKey(const InputEvent& e) {
    if (e.kind == InputKind::KeyUp) return false;

    switch (e.keycode) {
        case key::kBackspace:
            if (HasSelection()) {
                DeleteSelection();
            } else if (_caret > 0) {
                _value = _value.substring(0, _caret - 1) + _value.substring(_caret);
                _caret--;
            } else {
                return false;
            }
            return true;

        case key::kLeft:
            if (e.Has(mod::kShift) && _sel_anchor < 0) _sel_anchor = _caret;
            if (!e.Has(mod::kShift)) ClearSelection();
            MoveCaret(-1);
            return false;

        case key::kRight:
            if (e.Has(mod::kShift) && _sel_anchor < 0) _sel_anchor = _caret;
            if (!e.Has(mod::kShift)) ClearSelection();
            MoveCaret(1);
            return false;

        case key::kHome:
            SetCaretIndex(0);
            return false;

        case key::kEnd:
            SetCaretIndex((uint16_t)_value.length());
            return false;

        case key::kEnter:
            if (_on_submit) _on_submit(_value);
            return false;

        case key::kEscape:
            ClearSelection();
            return false;

        default:
            break;
    }

    // Ctrl-A selects all; nothing else is bound yet.
    if (e.Has(mod::kCtrl)) {
        if (e.codepoint == 'a' || e.codepoint == 'A') SelectAll();
        return false;
    }

    if (e.codepoint >= 0x20 && e.codepoint < 0x7F) {
        InsertCodepoint(e.codepoint);
        return true;
    }
    return false;
}

bool TextInput::HandleEvent(const InputEvent& e) {
    if (!_visible || !_enabled) return false;

    if (e.IsKey()) {
        if (!_focused) return false;
        const bool changed = ApplyKey(e);
        Invalidate();
        if (changed && _on_change) _on_change(_value);
        return true;
    }

    if (!e.IsPointer() || !_frame.Contains(e.x, e.y)) return false;

    switch (e.kind) {
        case InputKind::Tap:
            ClearSelection();
            SetCaretIndex(IndexFromX(e.x));
            return true;
        case InputKind::LongPress:
            SelectAll();
            return true;
        case InputKind::PointerDown:
        case InputKind::PointerUp:
            return true;
        default:
            return false;
    }
}

void TextInput::OnFocusGained() {
    // Focus is what raises the on-screen keyboard -- the field asks for it
    // rather than the screen wiring it up.
    Device::Get().Keyboard().Show();
    Invalidate();
}

void TextInput::OnFocusLost() {
    Device::Get().Keyboard().Hide();
    ClearSelection();
    Invalidate();
}

// ---------------------------------------------------------------- paint ----

Size TextInput::Measure(const Constraints& c) {
    const int16_t h =
        (int16_t)(Text().LineHeight(_style) + _padding.Vertical());
    return c.Clamp(Size{c.max_w, h < tok::kControlH ? tok::kControlH : h});
}

void TextInput::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    ctx.canvas->fillRoundRect(r.x, r.y, r.w, r.h, tok::kRadiusSm,
                             tok::kSurfaceSunken);
    for (int16_t i = 0; i < (IsFocused() ? tok::kFocusRingWidth : tok::kBorderWidth); ++i) {
        ctx.canvas->drawRoundRect((int16_t)(r.x + i), (int16_t)(r.y + i),
                                  (int16_t)(r.w - 2 * i), (int16_t)(r.h - 2 * i),
                                  tok::kRadiusSm,
                                  IsFocused() ? tok::kFg : tok::kBorder);
    }

    const Rect content = ContentRect();
    const Rect cr = ctx.ToCanvas(content);
    const int16_t text_y = (int16_t)(cr.y + (cr.h - Text().LineHeight(_style)) / 2);

    const String shown = DisplayString();

    if (shown.length() == 0 && _placeholder.length() > 0) {
        TextStyle ph = _style;
        ph.color = tok::kFgDisabled;
        Text().Draw(*ctx.canvas, _placeholder, cr.x, text_y, ph);
        return;
    }

    // Keep the caret on screen by scrolling the text under a fixed viewport.
    const int16_t caret_x = Text().MeasureWidth(shown.substring(0, _caret), _style);
    if (caret_x - _scroll_x > cr.w - tok::kSpaceSm) {
        _scroll_x = (int16_t)(caret_x - cr.w + tok::kSpaceSm);
    } else if (caret_x < _scroll_x) {
        _scroll_x = caret_x;
    }

    if (HasSelection()) {
        const uint16_t a = (uint16_t)_sel_anchor;
        const uint16_t lo = a < _caret ? a : _caret;
        const uint16_t hi = a < _caret ? _caret : a;
        const int16_t x0 = Text().MeasureWidth(shown.substring(0, lo), _style);
        const int16_t x1 = Text().MeasureWidth(shown.substring(0, hi), _style);
        ctx.canvas->fillRect((int16_t)(cr.x + x0 - _scroll_x), cr.y,
                             (int16_t)(x1 - x0), cr.h, tok::kSurfaceSunken - 3);
    }

    Text().Draw(*ctx.canvas, shown, (int16_t)(cr.x - _scroll_x), text_y, _style);

    if (IsFocused()) {
        // Solid, not blinking: an e-ink caret that blinks costs a panel update
        // twice a second and ghosts within a minute.
        ctx.canvas->fillRect((int16_t)(cr.x + caret_x - _scroll_x),
                             (int16_t)(text_y - 2), kCaretWidth,
                             (int16_t)(Text().LineHeight(_style) + 4), tok::kFg);
    }
}

// ------------------------------------------------------------- TextArea ----

TextArea::TextArea(uint16_t visible_lines) : _visible_lines(visible_lines) {
    _type_name = "textarea";
    SetPadding(EdgeInsets::All(tok::kSpaceSm));
}

void TextArea::SetVisibleLines(uint16_t lines) {
    if (_visible_lines == lines) return;
    _visible_lines = lines;
    Invalidate();
}

uint16_t TextArea::LineCount() const {
    return (uint16_t)_lines.size();
}

bool TextArea::ApplyKey(const InputEvent& e) {
    // Enter is a newline here, not a submit.
    if (e.keycode == key::kEnter && !e.Has(mod::kCtrl)) {
        DeleteSelection();
        _value = _value.substring(0, _caret) + "\n" + _value.substring(_caret);
        _caret++;
        _wrapped_for_width = -1;
        EnsureCaretVisible();
        return true;
    }

    const bool changed = TextInput::ApplyKey(e);
    if (changed) _wrapped_for_width = -1;
    EnsureCaretVisible();
    return changed;
}

void TextArea::EnsureCaretVisible() {
    // Find the wrapped line holding the caret and page to it.
    for (uint16_t i = 0; i < _lines.size(); ++i) {
        const TextLine& l = _lines[i];
        if (_caret < l.start || _caret > (uint16_t)(l.start + l.length)) continue;

        if (i < _first_visible_line) {
            _first_visible_line = i;
        } else if (i >= _first_visible_line + _visible_lines) {
            _first_visible_line = (uint16_t)(i - _visible_lines + 1);
        }
        return;
    }
}

Size TextArea::Measure(const Constraints& c) {
    const int16_t h = (int16_t)(Text().LineHeight(_style) * _visible_lines +
                                _padding.Vertical());
    return c.Clamp(Size{c.max_w, h});
}

void TextArea::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    ctx.canvas->fillRoundRect(r.x, r.y, r.w, r.h, tok::kRadiusSm, tok::kSurface);
    for (int16_t i = 0; i < (IsFocused() ? tok::kFocusRingWidth : tok::kBorderWidth); ++i) {
        ctx.canvas->drawRoundRect((int16_t)(r.x + i), (int16_t)(r.y + i),
                                  (int16_t)(r.w - 2 * i), (int16_t)(r.h - 2 * i),
                                  tok::kRadiusSm,
                                  IsFocused() ? tok::kFg : tok::kBorder);
    }

    const Rect content = ContentRect();
    if (_wrapped_for_width != content.w) {
        _lines = Text().Wrap(_value, _style, content.w, 0);
        _wrapped_for_width = content.w;
    }

    const Rect cr = ctx.ToCanvas(content);
    const int16_t lh = Text().LineHeight(_style);

    if (_value.length() == 0 && _placeholder.length() > 0) {
        TextStyle ph = _style;
        ph.color = tok::kFgDisabled;
        Text().Draw(*ctx.canvas, _placeholder, cr.x, cr.y, ph);
        return;
    }

    int16_t y = cr.y;
    for (uint16_t i = _first_visible_line;
         i < _lines.size() && i < _first_visible_line + _visible_lines; ++i) {
        const TextLine& l = _lines[i];
        const String s = _value.substring(l.start, l.start + l.length);
        Text().Draw(*ctx.canvas, s, cr.x, y, _style);

        if (IsFocused() && _caret >= l.start &&
            _caret <= (uint16_t)(l.start + l.length)) {
            const int16_t cx = Text().MeasureWidth(
                s.substring(0, _caret - l.start), _style);
            ctx.canvas->fillRect((int16_t)(cr.x + cx), y, kCaretWidth, lh,
                                 tok::kFg);
        }
        y = (int16_t)(y + lh);
    }
}

}  // namespace m5ui
