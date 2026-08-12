#include "keyboard.h"

#include "../core/text.h"
#include "../core/tokens.h"

namespace m5ui {

namespace {

// Synthetic keycodes for the layout-switch keys. Above the HID usage range so
// they cannot collide with a real key from a BLE keyboard.
constexpr uint16_t kKeyShift = 0x1000;
constexpr uint16_t kKeyNumeric = 0x1001;
constexpr uint16_t kKeySymbols = 0x1002;
constexpr uint16_t kKeyAlpha = 0x1003;

constexpr uint8_t kUnitsPerRow = 10;
constexpr uint32_t kDoubleTapMs = 400;

// Rows are declared as label strings; codepoints come straight from the
// characters, and HID codes are derived for the letters/digits that need them.
const char* kRowsLower[4] = {"qwertyuiop", "asdfghjkl", "zxcvbnm", ""};
const char* kRowsUpper[4] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM", ""};
const char* kRowsNumeric[4] = {"1234567890", "-/:;()$&@\"", ".,?!'", ""};
const char* kRowsSymbols[4] = {"[]{}#%^*+=", "_\\|~<>", ".,?!'", ""};

const char** RowsFor(KeyboardLayout layout) {
    switch (layout) {
        case KeyboardLayout::Upper:   return kRowsUpper;
        case KeyboardLayout::Numeric: return kRowsNumeric;
        case KeyboardLayout::Symbols: return kRowsSymbols;
        case KeyboardLayout::Lower:
        default:                      return kRowsLower;
    }
}

// HID usage id for an ASCII character, or 0 when there is no simple mapping.
uint16_t HidFor(char c) {
    if (c >= 'a' && c <= 'z') return (uint16_t)(0x04 + (c - 'a'));
    if (c >= 'A' && c <= 'Z') return (uint16_t)(0x04 + (c - 'A'));
    if (c >= '1' && c <= '9') return (uint16_t)(0x1E + (c - '1'));
    if (c == '0') return 0x27;
    if (c == ' ') return key::kSpace;
    return 0;
}

}  // namespace

OnScreenKeyboard::OnScreenKeyboard() : Widget("keyboard") {
    SetVisible(false);
    RebuildKeys();
}

void OnScreenKeyboard::SetLayout(KeyboardLayout layout) {
    if (_layout == layout) return;
    _layout = layout;
    RebuildKeys();
    if (!_frame.IsEmpty()) Layout(_frame);
    Invalidate();
}

void OnScreenKeyboard::Show() {
    if (IsVisible()) return;
    SetVisible(true);
    Invalidate();
}

void OnScreenKeyboard::Hide() {
    if (!IsVisible()) return;
    _pressed_index = -1;
    SetVisible(false);
    Invalidate();
}

Size OnScreenKeyboard::Measure(const Constraints& c) {
    if (!IsVisible()) return Size{c.max_w, 0};
    return Size{c.max_w, (int16_t)(kDefaultHeight < c.max_h ? kDefaultHeight : c.max_h)};
}

void OnScreenKeyboard::RebuildKeys() {
    _keys.clear();
    const char** rows = RowsFor(_layout);

    for (uint8_t r = 0; r < 3; ++r) {
        for (const char* p = rows[r]; *p != '\0'; ++p) {
            KeyDef k;
            k.codepoint = (uint16_t)(uint8_t)*p;
            k.keycode = HidFor(*p);
            // The label needs to outlive this loop; the row strings are static
            // literals, so a pointer into them is stable.
            k.label = p;
            k.width_units = 1;
            _keys.push_back(PlacedKey{k, Rect{}});
        }
    }

    // Row 3: shift, backspace. Row 4: layout switch, space, enter.
    const bool alpha = _layout == KeyboardLayout::Lower ||
                       _layout == KeyboardLayout::Upper;

    KeyDef shift;
    shift.keycode = alpha ? kKeyShift : kKeySymbols;
    shift.label = alpha ? "shift" : "#+=";
    shift.width_units = 2;
    shift.is_modifier = true;
    shift.is_sticky = alpha;
    _keys.push_back(PlacedKey{shift, Rect{}});

    KeyDef backspace;
    backspace.keycode = key::kBackspace;
    backspace.label = "del";
    backspace.width_units = 2;
    _keys.push_back(PlacedKey{backspace, Rect{}});

    KeyDef mode;
    mode.keycode = alpha ? kKeyNumeric : kKeyAlpha;
    mode.label = alpha ? "123" : "abc";
    mode.width_units = 2;
    mode.is_modifier = true;
    _keys.push_back(PlacedKey{mode, Rect{}});

    KeyDef space;
    space.keycode = key::kSpace;
    space.codepoint = ' ';
    space.label = "space";
    space.width_units = 5;
    _keys.push_back(PlacedKey{space, Rect{}});

    KeyDef enter;
    enter.keycode = key::kEnter;
    enter.label = "return";
    enter.width_units = 3;
    _keys.push_back(PlacedKey{enter, Rect{}});
}

void OnScreenKeyboard::Layout(const Rect& bounds) {
    SetFrame(bounds);
    if (_keys.empty()) return;

    const Rect box = ContentRect();
    const int16_t gap = tok::kSpaceXs;
    const int16_t row_h = (int16_t)((box.h - gap * 4) / 5);
    const int16_t unit_w = (int16_t)((box.w - gap * (kUnitsPerRow - 1)) / kUnitsPerRow);

    const char** rows = RowsFor(_layout);
    const uint8_t row_len[3] = {(uint8_t)strlen(rows[0]), (uint8_t)strlen(rows[1]),
                                (uint8_t)strlen(rows[2])};

    size_t index = 0;
    int16_t y = box.y;

    // Character rows, each centred on the widest row.
    for (uint8_t r = 0; r < 3; ++r) {
        const int16_t row_w =
            (int16_t)(row_len[r] * unit_w + (row_len[r] - 1) * gap);
        int16_t x = (int16_t)(box.x + (box.w - row_w) / 2);

        for (uint8_t i = 0; i < row_len[r] && index < _keys.size(); ++i, ++index) {
            _keys[index].frame = Rect{x, y, unit_w, row_h};
            x = (int16_t)(x + unit_w + gap);
        }
        y = (int16_t)(y + row_h + gap);
    }

    // Modifier row: shift + backspace, pushed to the outside edges.
    if (index + 1 < _keys.size()) {
        const int16_t w = (int16_t)(2 * unit_w + gap);
        _keys[index].frame = Rect{box.x, y, w, row_h};
        index++;
        _keys[index].frame = Rect{(int16_t)(box.Right() - w), y, w, row_h};
        index++;
        y = (int16_t)(y + row_h + gap);
    }

    // Bottom row: mode, space, enter -- space takes whatever is left.
    if (index + 2 < _keys.size()) {
        const int16_t mode_w = (int16_t)(2 * unit_w + gap);
        const int16_t enter_w = (int16_t)(3 * unit_w + 2 * gap);
        const int16_t space_w = (int16_t)(box.w - mode_w - enter_w - 2 * gap);

        _keys[index].frame = Rect{box.x, y, mode_w, row_h};
        index++;
        _keys[index].frame = Rect{(int16_t)(box.x + mode_w + gap), y, space_w, row_h};
        index++;
        _keys[index].frame = Rect{(int16_t)(box.Right() - enter_w), y, enter_w, row_h};
    }
}

int OnScreenKeyboard::FindKeyAt(int16_t x, int16_t y) const {
    for (size_t i = 0; i < _keys.size(); ++i) {
        if (_keys[i].frame.Contains(x, y)) return (int)i;
    }
    return -1;
}

void OnScreenKeyboard::Emit(const KeyDef& k) {
    // Layout switches never leave the keyboard.
    switch (k.keycode) {
        case kKeyShift: {
            const uint32_t now = millis();
            if (now - _last_shift_tap_ms < kDoubleTapMs) {
                _locked_modifiers ^= mod::kShift; // caps lock
                _sticky_modifiers = 0;
            } else {
                _sticky_modifiers ^= mod::kShift;
            }
            _last_shift_tap_ms = now;
            SetLayout((_sticky_modifiers | _locked_modifiers) & mod::kShift
                          ? KeyboardLayout::Upper
                          : KeyboardLayout::Lower);
            return;
        }
        case kKeyNumeric: SetLayout(KeyboardLayout::Numeric); return;
        case kKeySymbols: SetLayout(KeyboardLayout::Symbols); return;
        case kKeyAlpha:   SetLayout(KeyboardLayout::Lower); return;
        default: break;
    }

    InputEvent e = InputEvent::MakeKey(
        k.keycode, k.codepoint,
        (uint8_t)(_sticky_modifiers | _locked_modifiers), InputSource::Synthetic);

    if (_queue != nullptr) _queue->Push(e);
    if (_on_key) _on_key(e);

    // Sticky shift applies to exactly one key.
    if (_sticky_modifiers & mod::kShift) {
        _sticky_modifiers = 0;
        if (!(_locked_modifiers & mod::kShift)) SetLayout(KeyboardLayout::Lower);
    }
}

bool OnScreenKeyboard::HandleEvent(const InputEvent& e) {
    if (!IsVisible() || !e.IsPointer()) return false;
    if (!_frame.Contains(e.x, e.y)) return false;

    switch (e.kind) {
        case InputKind::PointerDown: {
            const int idx = FindKeyAt(e.x, e.y);
            if (idx < 0) return true; // swallow taps in the gaps
            _pressed_index = idx;
            InvalidateRect(_keys[idx].frame);
            return true;
        }
        case InputKind::Tap:
        case InputKind::PointerUp: {
            if (_pressed_index < 0) return true;
            const int idx = _pressed_index;
            _pressed_index = -1;
            InvalidateRect(_keys[idx].frame);

            // Only fire when the finger came up over the same key it went down
            // on -- a slide off the key is a cancel.
            if (e.kind == InputKind::PointerUp && FindKeyAt(e.x, e.y) == idx) {
                Emit(_keys[idx].def);
            }
            return true;
        }
        default:
            return true; // the keyboard eats drags so it cannot scroll a parent
    }
}

void OnScreenKeyboard::DrawKey(PaintContext& ctx, const PlacedKey& k,
                               bool pressed) {
    const Rect r = ctx.ToCanvas(k.frame);
    if (r.IsEmpty()) return;

    const uint8_t fill = pressed ? tok::kInverseBg : tok::kSurface;
    const uint8_t fg = pressed ? tok::kInverseFg : tok::kFg;

    ctx.canvas->fillRoundRect(r.x, r.y, r.w, r.h, tok::kRadiusSm, fill);
    ctx.canvas->drawRoundRect(r.x, r.y, r.w, r.h, tok::kRadiusSm, tok::kBorder);

    // Character keys carry a single char out of a longer static row string, so
    // the label is taken one byte wide rather than as a C string.
    String label;
    if (k.def.codepoint != 0 && strlen(k.def.label) > 1) {
        label = String((char)k.def.codepoint);
    } else {
        label = String(k.def.label);
    }

    TextStyle style;
    style.size = k.def.width_units > 1 ? tok::kTextSm : tok::kTextLg;
    style.color = fg;

    const int16_t tw = Text().MeasureWidth(label, style);
    const int16_t th = Text().LineHeight(style);
    Text().Draw(*ctx.canvas, label, (int16_t)(r.x + (r.w - tw) / 2),
                (int16_t)(r.y + (r.h - th) / 2), style);
}

void OnScreenKeyboard::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr || !IsVisible()) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    ctx.canvas->fillRect(r.x, r.y, r.w, r.h, tok::kSurfaceSunken);
    ctx.canvas->drawFastHLine(r.x, r.y, r.w, tok::kBorder);

    for (size_t i = 0; i < _keys.size(); ++i) {
        DrawKey(ctx, _keys[i], (int)i == _pressed_index);
    }
}

}  // namespace m5ui
