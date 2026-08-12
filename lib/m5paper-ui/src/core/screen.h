#pragma once
// Screen: one full-panel view. Replaces Frame_Base.
//
// A Screen owns a widget tree, a focus ring and optional chrome slots. It is
// constructed lazily by the App when it is first navigated to, and may be torn
// down when cold (issue #82) -- so Build() must be repeatable and all state
// that must survive lives in the Screen, not in the widgets.

#include <Arduino.h>

#include <vector>

#include "display.h"
#include "focus.h"
#include "geometry.h"
#include "widget.h"

namespace m5ui {

class App;

class Screen {
   public:
    explicit Screen(const char* name);
    virtual ~Screen();

    const char* Name() const {
        return _name;
    }

    // ---------------------------------------------------------- lifecycle --
    // Constructs the widget tree. Called before the screen is first shown and
    // again after a cold eviction. Must not assume prior state.
    virtual void Build() = 0;

    virtual void OnEnter() {}  // became the top of the stack
    virtual void OnExit() {}   // popped, about to be destroyed or cached
    virtual void OnPause() {}  // covered by a screen pushed above it
    virtual void OnResume() {} // uncovered

    // Called once per main-loop iteration while on top. Default does nothing;
    // override for clocks, polling, animation.
    virtual void Tick() {}

    // Returning false lets the framework pop the screen. Override to intercept
    // a back gesture -- e.g. to prompt about unsaved changes.
    virtual bool OnBack() {
        return false;
    }

    // ------------------------------------------------------------- chrome --
    // Reserved strips at the top and bottom. Content lays out between them.
    void SetChromeInsets(int16_t top, int16_t bottom);
    Rect ContentBounds() const;

    // ------------------------------------------------------------- render --
    Widget* Root() {
        return _root;
    }
    FocusManager& Focus() {
        return _focus;
    }

    // Lays out the tree and rebuilds the focus ring. Idempotent.
    void PerformLayout();

    // Paints anything dirty into the display's shadow canvas and marks the
    // touched regions. Does not flush -- Device decides when to hit the panel.
    void Paint(Display& display);

    // Forces a full repaint on the next Paint().
    void InvalidateAll();

    virtual bool HandleEvent(const InputEvent& e);

    App* Owner() const {
        return _app;
    }
    void SetOwner(App* app) {
        _app = app;
    }

    bool IsBuilt() const {
        return _built;
    }
    void EnsureBuilt();
    // Releases the widget tree. The screen object survives; Build() will run
    // again on next use.
    void Teardown();

    // Preferred update mode for this screen's next flush.
    virtual DrawIntent PaintIntent() const {
        return DrawIntent::Static;
    }

   protected:
    // Convenience: the most common root is a padded vertical stack.
    Widget* _root = nullptr;
    FocusManager _focus;
    const char* _name;
    App* _app = nullptr;
    int16_t _chrome_top = 0;
    int16_t _chrome_bottom = 0;
    bool _built = false;
    bool _needs_layout = true;
};

}  // namespace m5ui
