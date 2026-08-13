#pragma once
// One-screen app: registers "calendar" as the (only, home) route.

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

   private:
    m5ui::WeatherSnapshot _weather;
};

}  // namespace calendar
