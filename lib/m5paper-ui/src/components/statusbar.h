#pragma once
// StatusBar and AppBar (issue #50).
//
// The stock firmware draws the clock and battery inside one private method of
// Frame_Main, so no other screen shows them. These are components, available
// to every screen, and the battery gauge reads from Device::GetBattery()
// rather than re-deriving a voltage curve.

#include <Arduino.h>

#include <functional>

#include "../core/widget.h"
#include "button.h"
#include "label.h"

namespace m5ui {

// Top strip: title on the left, clock centred, battery and radio icons right.
class StatusBar : public Widget {
   public:
    StatusBar();

    void SetTitle(const String& title);
    void SetShowClock(bool show);
    void SetShowBattery(bool show);
    void SetShowMemory(bool show); // free internal DRAM, per issue #77
    void SetWifiConnected(bool connected);
    void SetBleConnected(bool connected);

    // Re-reads battery, clock and memory. Called from the owning screen's
    // Tick(); rate-limited internally to tok::kStatusPollMs.
    void Refresh();

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;

    // The clock and battery change often and are small -- exactly the case the
    // fast-text refresh path exists for (issue #38).
    DrawIntentHint PaintIntent() const override {
        return DrawIntentHint::Text;
    }

   private:
    void DrawBatteryGauge(PaintContext& ctx, int16_t right_edge);

    String _title;
    String _clock_text;
    uint8_t _battery_percent = 0;
    bool _battery_charging = false;
    uint32_t _free_dram_kb = 0;

    bool _show_clock = true;
    bool _show_battery = true;
    bool _show_memory = false;
    bool _wifi = false;
    bool _ble = false;
    uint32_t _last_refresh_ms = 0;
};

// Navigation strip: optional back button, title, trailing actions.
class AppBar : public Widget {
   public:
    explicit AppBar(const String& title = "");

    void SetTitle(const String& title);
    // Shows a back affordance and calls `handler` (or pops the app) when hit.
    void SetBackAction(std::function<void()> handler = nullptr);
    // Right-aligned action button. Returns it so the caller can style it.
    Button* AddAction(const String& label, std::function<void()> handler);

    Size Measure(const Constraints& c) override;
    void Layout(const Rect& bounds) override;
    void DrawSelf(PaintContext& ctx) override;

   private:
    String _title;
    Button* _back = nullptr;
    std::vector<Button*> _actions;
};

}  // namespace m5ui
