#pragma once
// Button. Replaces EPDGUI_Button.
//
// The differences from the old one: no per-state canvas allocation (the old
// switch widget cost 228x228x2 per instance -- ~312 KB for six of them on the
// home screen), a std::function callback, and a focus ring so it is reachable
// from a keyboard.

#include <Arduino.h>

#include "../core/text.h"
#include "../core/widget.h"

namespace m5ui {

enum class ButtonVariant : uint8_t {
    Filled,  // dark fill, light text -- primary action
    Outline, // hairline border -- secondary
    Ghost    // text only -- tertiary, list rows, toolbars
};

class Button : public Widget {
   public:
    explicit Button(const String& label = "",
                    ButtonVariant variant = ButtonVariant::Outline);

    void SetLabel(const String& label);
    const String& GetLabel() const {
        return _label;
    }
    void SetVariant(ButtonVariant v);
    void SetTextSize(uint8_t size);

    // Fluent form, so a screen reads as a declaration:
    //   Add((new Button("Save"))->OnPress([this] { Save(); }));
    Button* OnPress(std::function<void()> handler);

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;
    bool IsFocusable() const override {
        return IsEnabled() && IsVisible();
    }

   private:
    String _label;
    ButtonVariant _variant = ButtonVariant::Outline;
    TextStyle _style;
};

}  // namespace m5ui
