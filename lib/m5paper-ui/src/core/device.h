#pragma once
// Device: the single runtime facade over the hardware, and the owner of the
// application loop (issue #92).
//
// Everything the firmware needs to know about itself, and every piece of
// hardware it talks to, is reachable from one object:
//
//     auto& dev = m5ui::Device::Get();
//     dev.Begin();
//     dev.Run(my_app);                       // never returns
//
//     dev.Power().GetBattery().percent;      // 74
//     dev.Specs().soc.model;                 // "ESP32-D0WDQ6"
//     dev.Memory().FreeInternal();           // 213_408
//     dev.Keyboard().Show();
//
// It wraps the UI as well as the hardware: input sources feed one queue, the
// queue drains through the focus manager into the app's top screen, the screen
// paints into the shadow framebuffer, and Device decides when that reaches the
// panel.

#include <Arduino.h>
#include <M5EPD.h>

#include <functional>

#include "../modules/keyboard.h"
#include "../modules/storage.h"
#include "app.h"
#include "display.h"
#include "focus.h"
#include "geometry.h"
#include "input.h"
#include "memory.h"
#include "power.h"
#include "specs.h"
#include "text.h"

namespace m5ui {

// What Begin() should bring up. Subsystems left out are never initialised, so
// an app that does not need SD does not pay for the mount.
struct DeviceConfig {
    bool touch = true;
    bool side_buttons = true;
    bool storage = true;
    bool rtc = true;
    bool serial_log = true;
    bool print_spec_banner = true; // dump the spec sheet to serial at boot
    const char* storage_root = "/app";
    uint32_t serial_baud = 115200;

    // Idle handling. The stock firmware shuts down after 5 minutes; the
    // framework makes that a policy rather than a constant buried in a frame.
    uint32_t idle_prompt_ms = 5 * 60 * 1000;
    uint32_t idle_shutdown_ms = 6 * 60 * 1000;
    bool auto_power_save = true;

    // Clean the panel after this many partial flushes even when the debt
    // ledger has not tripped. 0 disables.
    uint32_t periodic_refresh_flushes = 200;
};

// RTC + NTP, wrapped so screens stop calling M5.RTC directly.
class Clock {
   public:
    void Begin();
    String TimeString(bool with_seconds = false); // "14:07"
    String DateString();                          // "2026-08-12"
    uint32_t EpochSeconds();
    void SetTimeZoneOffsetMinutes(int16_t minutes);
    int16_t TimeZoneOffsetMinutes() const {
        return _tz_minutes;
    }
    bool IsSynced() const {
        return _synced;
    }
    // Requires WiFi to already be up; the framework does not start it for you.
    bool SyncFromNtp(const char* server = "pool.ntp.org", uint32_t timeout_ms = 10000);

   private:
    int16_t _tz_minutes = 0;
    bool _synced = false;
};

class Device {
   public:
    // The one instance. Function-local static, so construction happens on
    // first use rather than at static-init time (library rule 2, issue #87).
    static Device& Get();

    // Brings up power rails, panel, touch, buttons, storage, RTC and the text
    // engine, then probes the spec sheet. Safe to call twice.
    bool Begin(const DeviceConfig& config = DeviceConfig{});
    bool IsBegun() const {
        return _begun;
    }

    // ---------------------------------------------------------- subsystems --
    // Named Panel(), not Display(), so the accessor does not collide with the
    // type it returns.
    Display& Panel() {
        return _display;
    }
    PowerManager& Power() {
        return _power;
    }
    MemoryMonitor& Memory() {
        return _memory;
    }
    Storage& Files() {
        return _storage;
    }
    Clock& Time() {
        return _clock;
    }
    OnScreenKeyboard& Keyboard() {
        return _keyboard;
    }
    InputQueue& Input() {
        return _queue;
    }
    TouchSource& Touch() {
        return _touch;
    }
    SideButtonSource& Buttons() {
        return _buttons;
    }
    TextEngine& Fonts() {
        return Text();
    }

    // Everything about this hardware, probed at Begin().
    const DeviceSpecs& Specs() const {
        return _specs;
    }
    // The configuration Begin() was called with -- so a screen that overrides
    // a policy can restore the app's setting rather than a library default.
    const DeviceConfig& Config() const {
        return _config;
    }
    // Re-probes the mutable parts (SD presence, CPU frequency).
    void RefreshSpecs();

    // ------------------------------------------------------------ battery --
    // Shortcut for the most-asked question, so status bars do not need two
    // hops. Pull-only: this reads the ADC when you call it and at no other
    // time. Nothing in Step() touches the battery.
    BatteryState GetBattery() {
        return _power.GetBattery();
    }

    // ------------------------------------------------------------ app loop --
    // Attaches an app, spawns the input task, and runs the UI loop forever.
    // Call from setup(); it does not return.
    //
    // Two tasks, because a panel update blocks for 450ms under GC16 and input
    // sampled only between updates is input mostly missed (issue #107):
    //
    //   input task  core 0, high priority -- polls the GT911 at kInputPollHz
    //                                        and pushes to the queue
    //   UI (this)   core 1, normal        -- owns the widget tree: drain,
    //                                        tick, paint, flush
    //
    // The widget tree is owned exclusively by the UI task, so only InputQueue
    // crosses the boundary and only it needs guarding. Touch is on I2C and the
    // panel on SPI, so the two tasks do not contend for a bus.
    [[noreturn]] void Run(App& app);

    // One UI iteration, for callers driving the loop themselves (tests, or an
    // app with its own outer loop). Returns true when the panel was touched.
    // Does NOT poll hardware -- it drains whatever the input task has queued.
    bool Step();

    App* CurrentApp() const {
        return _app;
    }
    void SetApp(App* app);

    // --------------------------------------------------------------- idle --
    void NoteActivity();
    uint32_t IdleMs() const;
    // Invoked once when idle_prompt_ms elapses. Return true to cancel the
    // pending shutdown -- e.g. after the user dismisses a prompt.
    void OnIdlePrompt(std::function<bool()> handler) {
        _on_idle_prompt = std::move(handler);
    }

    // ------------------------------------------------------------- power ---
    [[noreturn]] void Shutdown() {
        _display.WaitIdle();
        _power.Shutdown();
    }
    void Sleep(uint32_t wake_after_seconds = 0);

    // -------------------------------------------------------------- debug ---
    void PrintBanner(Print& out);
    // Frames per second and flush counts -- for the demo's diagnostics screen.
    uint32_t LoopCount() const {
        return _loop_count;
    }

   private:
    Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Polls the hardware sources into the queue. Runs on the input task.
    void PollSources();
    // Drains the queue into the widget tree. Runs on the UI task.
    void PumpInput();
    void CheckIdle();
    void CheckPeriodicRefresh();

    // FreeRTOS entry point; forwards to PollSources() forever.
    static void InputTaskEntry(void* self);

    // 50 Hz. Fast enough that a deliberate tap cannot fall between samples,
    // slow enough to leave the core mostly idle. The GT911 itself reports at
    // roughly 100 Hz, so this halves its rate rather than chasing it.
    static constexpr uint32_t kInputPollHz = 50;
    static constexpr uint32_t kInputStackBytes = 4096;

    DeviceConfig _config;
    DeviceSpecs _specs;

    Display _display;
    PowerManager _power;
    MemoryMonitor _memory;
    Storage _storage;
    Clock _clock;

    InputQueue _queue;
    TouchSource _touch;
    SideButtonSource _buttons;
    OnScreenKeyboard _keyboard;

    // Measurement target for TextEngine; see text.h.
    M5EPD_Canvas* _scratch = nullptr;

    App* _app = nullptr;
    std::function<bool()> _on_idle_prompt;

    uint32_t _last_activity_ms = 0;
    uint32_t _loop_count = 0;
    uint32_t _flushes_since_clean = 0;
    bool _idle_prompt_shown = false;
    bool _begun = false;
};

// Free shortcuts, so a component can ask a question without threading a Device
// reference through every constructor.
inline Device& Dev() {
    return Device::Get();
}
inline BatteryState GetBattery() {
    return Device::Get().GetBattery();
}

}  // namespace m5ui
