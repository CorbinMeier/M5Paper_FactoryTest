#pragma once
// The showcase screen for the device wrapper: everything on it comes from
// Device::Get(), nothing is hardcoded.

#include "demo_screen.h"

namespace demo {

class DeviceInfoScreen : public DemoScreen {
   public:
    DeviceInfoScreen();

    // Pulls the battery and memory figures on a 5 s cadence -- this screen is
    // the one place in the demo that asks that often.
    void Tick() override;

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
