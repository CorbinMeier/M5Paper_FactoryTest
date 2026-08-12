#pragma once
// The showcase screen for the device wrapper: everything on it comes from
// Device::Get(), nothing is hardcoded.

#include "demo_screen.h"

namespace demo {

class DeviceInfoScreen : public DemoScreen {
   public:
    DeviceInfoScreen();

    void Tick() override;
    // Tighten the battery sampling period while this screen is open, and put
    // the app's configured period back on the way out.
    void OnEnter() override;
    void OnExit() override;

   protected:
    void BuildContent(m5ui::Column& column) override;
    void BuildActions(m5ui::AppBar& bar) override;

   private:
    void BuildBatteryCard(m5ui::Column& column);
    void BuildMemoryCard(m5ui::Column& column);
    void BuildSpecCards(m5ui::Column& column);
    void RefreshLiveValues();

    // Live widgets, re-read on Tick().
    m5ui::Meter* _battery_meter = nullptr;
    m5ui::Label* _battery_detail = nullptr;
    m5ui::Meter* _dram_meter = nullptr;
    m5ui::Meter* _psram_meter = nullptr;
    m5ui::Label* _dram_detail = nullptr;
    m5ui::Label* _uptime = nullptr;

    uint32_t _last_live_refresh_ms = 0;
};

}  // namespace demo
