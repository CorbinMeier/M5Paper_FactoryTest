#pragma once
// Focus manager and tab order (issue #29).
//
// Focus is what makes a keyboard useful: without it a BLE key event has no
// destination. Order is derived from the widget tree in depth-first paint
// order, which matches reading order for every layout the framework produces.

#include <vector>

#include "input.h"
#include "widget.h"

namespace m5ui {

enum class FocusDirection : uint8_t { Next, Previous, Up, Down, Left, Right };

class FocusManager {
   public:
    // Rebuilds the tab ring from `root`. Called after any structural change to
    // a screen; cheap enough to call on every layout pass.
    void Rebuild(Widget* root);

    Widget* Focused() const {
        return _focused;
    }
    bool SetFocus(Widget* w);
    void ClearFocus();

    // Advances the ring. Wraps. Returns the newly focused widget, or null when
    // nothing is focusable.
    Widget* Move(FocusDirection dir);

    // Spatial move for the arrow keys -- picks the nearest focusable widget in
    // the given direction rather than the next one in tab order.
    Widget* MoveSpatial(FocusDirection dir);

    // Routes a key event to the focused widget, bubbling to its ancestors when
    // unhandled, and handles Tab/Shift-Tab and the arrows itself as a fallback.
    // Returns true when consumed.
    bool DispatchKey(const InputEvent& e);

    // Touch sets focus too, so tapping a field then typing on a BLE keyboard
    // does what the user expects.
    void FocusFromPointer(Widget* root, int16_t x, int16_t y);

    size_t Count() const {
        return _ring.size();
    }

   private:
    void Collect(Widget* w);
    int IndexOf(Widget* w) const;

    std::vector<Widget*> _ring;
    Widget* _focused = nullptr;
};

}  // namespace m5ui
