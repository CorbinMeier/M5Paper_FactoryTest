#pragma once
// Registers "calendar" (the default home) and "pairing" (issue #114).

#include <m5paper_ui.h>

// Not in the umbrella header on purpose (see ble_companion.h) -- it pulls in
// NimBLE-Arduino, which demo-all and every other app should not pay for.
#include "modules/ble_companion.h"

namespace calendar {

class CalendarApp : public m5ui::App {
   public:
    CalendarApp();
    void OnStart() override;

    // Set before Device::SetApp() triggers OnStart()/Push(), so the home
    // screen's factory (which runs synchronously inside Push()) sees it.
    void SetWeather(const m5ui::WeatherSnapshot& weather) {
        _weather = weather;
    }

    // Owned by the app (not Device -- BLE is opt-in per-app, issue #112),
    // long-lived so any screen or main.cpp code can drive pairing, not just
    // whatever triggered it first (issue #114).
    m5ui::BleCompanion& Ble() {
        return _ble;
    }

    // Must be called before Device::SetApp()/Start() -- OnStart() reads this
    // once to decide the home route. main.cpp sets it when the Push button
    // is held through boot.
    void RequestPairingOnStart() {
        _pairing_requested = true;
    }

   private:
    m5ui::WeatherSnapshot _weather;
    m5ui::BleCompanion _ble;
    bool _pairing_requested = false;
};

}  // namespace calendar
