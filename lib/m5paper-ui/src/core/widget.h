#pragma once
// Widget base (issues #29, #30, #33).
//
// Replaces EPDGUI_Base. Three changes that matter:
//   1. Callbacks are std::function, so a lambda can capture (issue #33). The
//      old `void(*)(epdgui_args_vector_t&)` forced state through a vector of
//      void*.
//   2. Widgets form a tree with layout params, not a flat list at absolute
//      coordinates.
//   3. A widget declares what it dirtied instead of the whole screen
//      repainting.

#include <Arduino.h>
#include <M5EPD.h>

#include <functional>
#include <vector>

#include "geometry.h"
#include "input.h"
#include "layout.h"
#include "tokens.h"

namespace m5ui {

class Display;

// Mirrors DrawIntent without pulling display.h into every widget header.
enum class DrawIntentHint : uint8_t { Static, Text, Animated };

enum class WidgetState : uint8_t {
    Normal = 0,
    Pressed,
    Focused,
    Disabled,
    Hidden
};

// Painting context handed to Draw(). Carries the canvas plus the clip so a
// widget inside a ScrollView cannot scribble outside it.
struct PaintContext {
    M5EPD_Canvas* canvas = nullptr;
    Rect clip = Rect::FullScreen();
    int16_t scroll_x = 0;
    int16_t scroll_y = 0;

    // Widget-space rect -> canvas-space, clipped.
    Rect ToCanvas(const Rect& r) const {
        return Rect{(int16_t)(r.x - scroll_x), (int16_t)(r.y - scroll_y), r.w,
                    r.h}
            .Intersection(clip);
    }
};

class Widget {
   public:
    using TapHandler = std::function<void(Widget&)>;
    using KeyHandler = std::function<bool(Widget&, const InputEvent&)>;

    explicit Widget(const char* type_name = "widget");
    virtual ~Widget();

    // ------------------------------------------------------------- tree ---
    // Takes ownership; the child is deleted with its parent.
    Widget* Add(Widget* child);
    void Remove(Widget* child);
    void ClearChildren();
    Widget* Parent() const {
        return _parent;
    }
    const std::vector<Widget*>& Children() const {
        return _children;
    }
    Widget* FindById(uint32_t id);

    // ----------------------------------------------------------- geometry --
    void SetFrame(const Rect& r);
    const Rect& Frame() const {
        return _frame;
    }
    // Frame minus padding -- where content goes.
    Rect ContentRect() const {
        return _padding.Deflate(_frame);
    }

    // Natural size given the constraints. Overridden by anything that sizes to
    // content (Label, Button); the default reports the preferred size.
    virtual Size Measure(const Constraints& c);

    // Assigns this widget's frame and lays out its children. The default gives
    // every child the full content rect.
    virtual void Layout(const Rect& bounds);

    // ------------------------------------------------------------ paint ---
    // Draws self, then children. Implementations paint only within ctx.clip.
    virtual void Draw(PaintContext& ctx);
    // Draws only what this widget owns; children are handled by Draw().
    virtual void DrawSelf(PaintContext& ctx) {
        (void)ctx;
    }

    // Marks this widget's frame dirty and propagates to the root so the screen
    // knows to repaint. Cheap and idempotent within a frame.
    void Invalidate();
    // Virtual so a screen root can accumulate the dirty union instead of just
    // flagging itself.
    virtual void InvalidateRect(const Rect& r);
    bool NeedsPaint() const {
        return _needs_paint;
    }
    void ClearNeedsPaint() {
        _needs_paint = false;
    }
    // Preferred update mode for this widget's region -- a caret wants A2, a
    // page of text wants GL16.
    virtual DrawIntentHint PaintIntent() const;

    // ------------------------------------------------------------ input ---
    // Returns true when consumed. The default routes to children back-to-front
    // and then to the tap/key handlers.
    virtual bool HandleEvent(const InputEvent& e);

    // Deepest visible, enabled descendant containing the point.
    Widget* HitTest(int16_t x, int16_t y);

    void OnTap(TapHandler h) {
        _on_tap = std::move(h);
    }
    void OnLongPress(TapHandler h) {
        _on_long_press = std::move(h);
    }
    void OnKey(KeyHandler h) {
        _on_key = std::move(h);
    }

    // ------------------------------------------------------------ state ---
    virtual bool IsFocusable() const {
        return _focusable && _enabled && _visible;
    }
    void SetFocusable(bool v) {
        _focusable = v;
    }
    virtual void OnFocusGained() {
        Invalidate();
    }
    virtual void OnFocusLost() {
        Invalidate();
    }
    bool IsFocused() const {
        return _focused;
    }
    void SetFocused(bool v);

    void SetVisible(bool v);
    bool IsVisible() const {
        return _visible;
    }
    void SetEnabled(bool v);
    bool IsEnabled() const {
        return _enabled;
    }

    void SetId(uint32_t id) {
        _id = id;
    }
    uint32_t Id() const {
        return _id;
    }
    const char* TypeName() const {
        return _type_name;
    }

    // ----------------------------------------------------------- styling --
    void SetPadding(const EdgeInsets& p) {
        _padding = p;
        Invalidate();
    }
    void SetMargin(const EdgeInsets& m) {
        _params.margin = m;
    }
    void SetFlex(uint8_t f) {
        _params.flex = f;
    }
    void SetPreferredSize(Size s) {
        _params.preferred = s;
    }
    LayoutParams& Params() {
        return _params;
    }

   protected:
    void DrawFocusRing(PaintContext& ctx);

    const char* _type_name;
    Widget* _parent = nullptr;
    std::vector<Widget*> _children;

    Rect _frame;
    EdgeInsets _padding;
    LayoutParams _params;

    uint32_t _id = 0;
    bool _visible = true;
    bool _enabled = true;
    bool _focusable = false;
    bool _focused = false;
    bool _pressed = false;
    bool _needs_paint = true;

    TapHandler _on_tap;
    TapHandler _on_long_press;
    KeyHandler _on_key;
};

}  // namespace m5ui
