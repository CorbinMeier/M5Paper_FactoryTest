#pragma once
// Application: a navigation stack of screens plus the per-frame update pass.
//
// Replaces the EPDGUI frame stack. Two behavioural differences worth naming:
//   - Screens are registered by name and built lazily on first navigation
//     (issue #82), so a 14-screen demo does not allocate 14 widget trees at
//     boot.
//   - Screens below a configurable depth are torn down to reclaim PSRAM, and
//     rebuilt on the way back. Screen::Build() must therefore be repeatable.

#include <Arduino.h>

#include <functional>
#include <map>
#include <vector>

#include "display.h"
#include "input.h"
#include "screen.h"

namespace m5ui {

class Device;

// Screens are created on demand rather than up front.
using ScreenFactory = std::function<Screen*()>;

class App {
   public:
    // Screens deeper than this in the stack are torn down; 0 disables it.
    static constexpr uint8_t kColdDepth = 3;

    // Reserved route for the framework system menu (issue #115). Every App
    // gets a screen registered here from the constructor, so a 5-second hold
    // of the Push side button works with no per-app wiring.
    static constexpr const char* kSystemMenuRoute = "__system_menu__";

    explicit App(const char* name = "app");
    virtual ~App();

    const char* Name() const {
        return _name;
    }

    // ----------------------------------------------------------- registry --
    void Register(const String& route, ScreenFactory factory);
    void SetHome(const String& route) {
        _home = route;
    }

    // --------------------------------------------------------- navigation --
    // Pushes a registered route. Returns false when the route is unknown.
    bool Push(const String& route);
    // Pops the top screen. Returns false at the root -- the app decides what
    // that means (usually nothing, or shutdown).
    bool Pop();
    // Replaces the top screen instead of stacking on it.
    bool Replace(const String& route);
    // Pops back to the home route.
    void PopToRoot();

    // Opens the system menu overlay (issue #115), unless it is already the
    // top screen. Called by Device on the Push-button hold gesture; apps may
    // also call it directly, e.g. from a settings button.
    bool OpenSystemMenu();

    Screen* Top() const;
    uint8_t Depth() const {
        return (uint8_t)_stack.size();
    }

    // ---------------------------------------------------------- lifecycle --
    // Called once by Device after hardware init, before the first frame.
    virtual void OnStart() {}
    virtual void OnStop() {}

    // Called by Device every iteration, in this order.
    void DispatchEvent(const InputEvent& e);
    void Tick();
    void Paint(Display& display);

    // True when the top screen wants the panel touched this frame.
    DrawIntent PaintIntent() const;

    // Entry point Device calls once. Builds and enters the home screen.
    void Start();

   protected:
    void EvictColdScreens();

    const char* _name;
    String _home;
    std::map<String, ScreenFactory> _routes;
    std::vector<Screen*> _stack;
    bool _started = false;
};

}  // namespace m5ui
