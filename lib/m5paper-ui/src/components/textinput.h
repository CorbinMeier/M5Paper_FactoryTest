#pragma once
// TextInput (issue #52) and TextArea (issue #53).
//
// Accepts keys from the shared input queue, so it cannot tell an on-screen tap
// from a BLE keystroke -- which is exactly what makes keyboard mode switching
// (#61) a policy decision rather than a second code path.
//
// The caret is the one thing on this device that genuinely wants to blink, and
// e-ink cannot afford it: the caret is drawn solid while focused and repainted
// only when it moves, via the fast-text path.

#include <Arduino.h>

#include <functional>

#include "../core/text.h"
#include "../core/widget.h"

namespace m5ui {

class TextInput : public Widget {
   public:
    using ChangeHandler = std::function<void(const String&)>;
    using SubmitHandler = std::function<void(const String&)>;

    TextInput();

    void SetValue(const String& value);
    const String& Value() const {
        return _value;
    }
    void SetPlaceholder(const String& placeholder);
    void SetMaxLength(uint16_t max_length);
    // Renders as dots. Paired with #53's PasswordInput.
    void SetPassword(bool password);
    void SetTextSize(uint8_t size);

    void OnChange(ChangeHandler h) {
        _on_change = std::move(h);
    }
    void OnSubmit(SubmitHandler h) {
        _on_submit = std::move(h);
    }

    // ------------------------------------------------------------- caret --
    uint16_t CaretIndex() const {
        return _caret;
    }
    void SetCaretIndex(uint16_t index);
    void MoveCaret(int16_t by);
    void SelectAll();
    void ClearSelection();
    bool HasSelection() const {
        return _sel_anchor >= 0 && (uint16_t)_sel_anchor != _caret;
    }

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;
    bool HandleEvent(const InputEvent& e) override;
    bool IsFocusable() const override {
        return IsEnabled() && IsVisible();
    }
    void OnFocusGained() override;
    void OnFocusLost() override;

    DrawIntentHint PaintIntent() const override {
        return DrawIntentHint::Text;
    }

   protected:
    // Applies one key. Returns true when the value changed.
    virtual bool ApplyKey(const InputEvent& e);
    void DeleteSelection();
    void InsertCodepoint(uint16_t cp);
    // Byte offset in `_value` nearest to a tapped x position.
    uint16_t IndexFromX(int16_t x) const;
    String DisplayString() const;

    String _value;
    String _placeholder;
    TextStyle _style;
    uint16_t _caret = 0;
    int32_t _sel_anchor = -1; // -1 = no selection
    uint16_t _max_length = 0; // 0 = unbounded
    bool _password = false;
    // Horizontal scroll, so a caret past the right edge stays visible.
    int16_t _scroll_x = 0;

    ChangeHandler _on_change;
    SubmitHandler _on_submit;
};

// Multi-line variant. Enter inserts a newline rather than submitting, and the
// value wraps to the widget width.
class TextArea : public TextInput {
   public:
    explicit TextArea(uint16_t visible_lines = 6);

    void SetVisibleLines(uint16_t lines);
    uint16_t LineCount() const;

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;

   protected:
    bool ApplyKey(const InputEvent& e) override;

   private:
    void EnsureCaretVisible();

    uint16_t _visible_lines = 6;
    uint16_t _first_visible_line = 0;
    std::vector<TextLine> _lines;
    int16_t _wrapped_for_width = -1;
};

}  // namespace m5ui
