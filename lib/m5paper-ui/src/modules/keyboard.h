#pragma once
// On-screen keyboard (issue #61 -- the on-screen half of mode switching).
//
// Rewritten from EPDGUI_Keyboard around two changes: it emits InputEvents into
// the shared queue rather than mutating a textbox it holds a pointer to, and
// it is a Widget, so it participates in layout instead of being drawn at fixed
// coordinates.
//
// Because it emits the same HID keycodes a BLE keyboard does, a TextInput
// cannot tell the two apart -- which is what makes #61 a policy decision
// rather than a second input path.

#include <Arduino.h>

#include <functional>
#include <vector>

#include "../core/input.h"
#include "../core/widget.h"

namespace m5ui {

enum class KeyboardLayout : uint8_t {
    Lower = 0,
    Upper,
    Numeric,
    Symbols
};

// One key's footprint within a layout row.
struct KeyDef {
    uint16_t keycode = 0;   // HID usage id
    uint16_t codepoint = 0; // character emitted, 0 for a function key
    const char* label = ""; // what is drawn
    uint8_t width_units = 1; // 1 = standard key; space is ~5
    bool is_modifier = false;
    bool is_sticky = false; // shift: applies to the next key only
};

class OnScreenKeyboard : public Widget {
   public:
    static constexpr int16_t kRows = 5;
    static constexpr int16_t kDefaultHeight = 380;

    OnScreenKeyboard();

    // Events go here. Device wires this to its own queue at construction so
    // on-screen and BLE keys land in the same place.
    void SetTarget(InputQueue* queue) {
        _queue = queue;
    }

    // Optional direct hook, for a field that wants keys without the queue.
    void OnKeyEmitted(std::function<void(const InputEvent&)> h) {
        _on_key = std::move(h);
    }

    void SetLayout(KeyboardLayout layout);
    KeyboardLayout Layout() const {
        return _layout;
    }

    // Slides in/out. Shown state reserves height in the parent's layout, so a
    // ScrollView above it shrinks rather than being covered.
    void Show();
    void Hide();
    bool IsShown() const {
        return IsVisible();
    }

    Size Measure(const Constraints& c) override;
    void Layout(const Rect& bounds) override;
    void DrawSelf(PaintContext& ctx) override;
    bool HandleEvent(const InputEvent& e) override;

    // The keyboard repaints a single key on press; the whole board is far too
    // much to push for one keystroke.
    DrawIntentHint PaintIntent() const override {
        return DrawIntentHint::Animated;
    }

   private:
    struct PlacedKey {
        KeyDef def;
        Rect frame;
    };

    void RebuildKeys();
    void Emit(const KeyDef& key);
    void DrawKey(PaintContext& ctx, const PlacedKey& key, bool pressed);
    int FindKeyAt(int16_t x, int16_t y) const;

    KeyboardLayout _layout = KeyboardLayout::Lower;
    std::vector<PlacedKey> _keys;
    InputQueue* _queue = nullptr;
    std::function<void(const InputEvent&)> _on_key;
    int _pressed_index = -1;
    uint8_t _sticky_modifiers = 0;
    uint8_t _locked_modifiers = 0; // caps lock, from a double-tapped shift
    uint32_t _last_shift_tap_ms = 0;
};

}  // namespace m5ui
