#pragma once
// System menu: the framework-level overlay opened by holding the Push side
// button (G38) for tok::kSystemMenuHoldMs (issue #115).
//
// App's constructor registers this under App::kSystemMenuRoute, so every app
// gets it for free -- no per-app wiring needed.
//
// Connectivity rows are stubs: there is no WiFi module (epic #26) and no BLE
// stack yet (epic #24, #59 spike unstarted), so Wi-Fi/Bluetooth render
// disabled, ready to wire up once those land.

#include "screen.h"

namespace m5ui {

class SystemMenuScreen : public Screen {
   public:
    SystemMenuScreen();

    void Build() override;

   private:
    // What the next Build() should produce. Menu is the normal state; Blank
    // and Calendar are one-shot splashes drawn right before power-off.
    enum class Mode : uint8_t { Menu = 0, Blank, Calendar };

    void BuildMenu();
    // Intentionally empty -- ScreenRoot already clears its dirty rect to
    // tok::kSurface before drawing children, so no children is a blank page.
    void BuildBlankSplash() {}
    void BuildCalendarSplash();

    // Rebuilds the screen in `mode`, paints and flushes it synchronously,
    // then cuts power. Does not return.
    [[noreturn]] void PowerOffAfter(Mode mode);

    void Close();

    Mode _mode = Mode::Menu;
};

}  // namespace m5ui
