#include "device.h"

#include <esp_task_wdt.h>
#include <time.h>

#include "i2c_lock.h"

namespace m5ui {

// ---------------------------------------------------------------- Clock ----

void Clock::Begin() {
    rtc_time_t t;
    {
        I2CLock lock;
        M5.RTC.getTime(&t);
    }
    // The RTC keeps running on coin-cell backup, so a plausible time here means
    // it was set at some point -- treat that as synced until NTP says otherwise.
    _synced = !(t.hour == 0 && t.min == 0 && t.sec == 0);
}

String Clock::TimeString(bool with_seconds) {
    rtc_time_t t;
    {
        I2CLock lock;
        M5.RTC.getTime(&t);
    }

    char buf[12];
    if (with_seconds) {
        snprintf(buf, sizeof(buf), "%2d:%02d:%02d", t.hour, t.min, t.sec);
    } else {
        snprintf(buf, sizeof(buf), "%2d:%02d", t.hour, t.min);
    }
    return String(buf);
}

String Clock::DateString() {
    rtc_date_t d;
    {
        I2CLock lock;
        M5.RTC.getDate(&d);
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.year, d.mon, d.day);
    return String(buf);
}

uint32_t Clock::EpochSeconds() {
    rtc_time_t t;
    rtc_date_t d;
    {
        I2CLock lock;
        M5.RTC.getTime(&t);
        M5.RTC.getDate(&d);
    }

    struct tm tm_val = {};
    tm_val.tm_year = d.year - 1900;
    tm_val.tm_mon = d.mon - 1;
    tm_val.tm_mday = d.day;
    tm_val.tm_hour = t.hour;
    tm_val.tm_min = t.min;
    tm_val.tm_sec = t.sec;
    return (uint32_t)mktime(&tm_val);
}

void Clock::SetTimeZoneOffsetMinutes(int16_t minutes) {
    _tz_minutes = minutes;
}

bool Clock::SyncFromNtp(const char* server, uint32_t timeout_ms) {
    configTime(_tz_minutes * 60, 0, server);

    // Poll rather than block indefinitely -- the stock firmware's NTP path
    // could hang the boot (issue: "Fix system crash when connecting to NTP").
    const uint32_t deadline = millis() + timeout_ms;
    struct tm info = {};
    while (millis() < deadline) {
        if (getLocalTime(&info, 100)) {
            rtc_time_t t = {(int8_t)info.tm_hour, (int8_t)info.tm_min,
                            (int8_t)info.tm_sec};
            rtc_date_t d = {(int8_t)info.tm_wday, (int8_t)(info.tm_mon + 1),
                            (int8_t)info.tm_mday, (int16_t)(info.tm_year + 1900)};
            {
                I2CLock lock;
                M5.RTC.setTime(&t);
                M5.RTC.setDate(&d);
            }
            _synced = true;
            return true;
        }
        esp_task_wdt_reset();
        delay(50);
    }
    return false;
}

// --------------------------------------------------------------- Device ----

Device& Device::Get() {
    // Function-local static: built on first use, never at static-init time.
    static Device instance;
    return instance;
}

bool Device::Begin(const DeviceConfig& config) {
    if (_begun) return true;
    _config = config;

    // M5.begin(touch, sd, serial, battery_adc, rtc) -- the framework owns SD
    // through Storage, so it is mounted separately below.
    M5.begin(true, false, _config.serial_log, false, _config.rtc);
    M5.enableMainPower();

    if (_config.serial_log) {
        Serial.begin(_config.serial_baud);
    }

    _display.Begin();

    // A small off-screen canvas so text can be measured without a draw target.
    // 4bpp, so this is ~7 KB -- worth it to keep measurement off the shadow
    // framebuffer.
    _scratch = new M5EPD_Canvas(&M5.EPD);
    _scratch->createCanvas(kDisplayW, 40);
    SetScratchCanvas(_scratch);
    Text().Begin();

    _power.Begin();

    if (_config.touch) _touch.Begin();
    if (_config.rtc) _clock.Begin();
    if (_config.storage) _storage.Begin(_config.storage_root);

    _keyboard.SetTarget(&_queue);

    _specs.Probe();
    _specs.display.touch_present = _config.touch;

    if (_config.print_spec_banner && _config.serial_log) {
        PrintBanner(Serial);
    }

    _last_activity_ms = millis();
    _begun = true;
    return true;
}

void Device::RefreshSpecs() {
    _specs.Probe();
    _specs.display.touch_present = _config.touch;
}

void Device::PrintBanner(Print& out) {
    out.println();
    out.println(_specs.ToString());

    const BatteryState b = _power.GetBattery();
    out.printf("battery    %u mV, %u%%%s\n", b.millivolts, b.percent,
               b.usb_present ? " (usb)" : "");
    _memory.DumpTo(out);
    out.println();
}

void Device::SetApp(App* app) {
    _app = app;
    if (_app != nullptr) _app->Start();
}

// ----------------------------------------------------------- input pump ----

void Device::PollSources() {
    if (_config.touch) _touch.Poll(_queue);
    if (_config.side_buttons) _buttons.Poll(_queue);
}

void Device::InputTaskEntry(void* self) {
    Device* device = static_cast<Device*>(self);
    esp_task_wdt_add(nullptr);

    const TickType_t period = pdMS_TO_TICKS(1000 / kInputPollHz);
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        device->PollSources();
        esp_task_wdt_reset();
        // Fixed cadence rather than a trailing delay, so a slow I2C read does
        // not stretch the sampling interval and let a tap slip through.
        vTaskDelayUntil(&last_wake, period);
    }
}

void Device::PumpInput() {
    InputEvent e;
    while (_queue.Pop(e)) {
        NoteActivity();

        // The on-screen keyboard gets first refusal on pointer events inside
        // its own frame; otherwise a tap would fall through to the content
        // behind it.
        if (e.IsPointer() && _keyboard.IsShown() && _keyboard.HandleEvent(e)) {
            continue;
        }

        if (_app != nullptr) _app->DispatchEvent(e);
    }
}

// ------------------------------------------------------------ main loop ----

bool Device::Step() {
    _loop_count++;

    PumpInput();
    _memory.Tick();

    if (_app != nullptr) {
        _app->Tick();
        _app->Paint(_display);
    }

    bool painted = false;
    if (_display.HasDirty()) {
        _display.Flush(_app ? _app->PaintIntent() : DrawIntent::Static);
        _flushes_since_clean++;
        painted = true;
    }

    CheckPeriodicRefresh();
    CheckIdle();

    esp_task_wdt_reset();
    return painted;
}

void Device::Run(App& app) {
    if (!_begun) Begin();
    SetApp(&app);

    // Input on core 0, pinned: a panel update blocks this loop for hundreds of
    // milliseconds, and touch sampled only between updates is touch mostly
    // missed (issue #107). Priority above the UI task so a refresh in progress
    // never delays a sample.
    xTaskCreatePinnedToCore(&Device::InputTaskEntry, "m5ui_input",
                            kInputStackBytes, this,
                            tskIDLE_PRIORITY + 3, nullptr, 0);

    for (;;) {
        const bool painted = Step();
        // Nothing changed -- yield rather than spinning the CPU against an
        // e-ink panel that is not going to move.
        if (!painted) delay(10);
    }
}

void Device::CheckPeriodicRefresh() {
    if (_config.periodic_refresh_flushes == 0) return;
    if (_flushes_since_clean < _config.periodic_refresh_flushes) return;

    _display.RefreshGhostOnly();
    _flushes_since_clean = 0;
}

// ----------------------------------------------------------------- idle ----

void Device::NoteActivity() {
    _last_activity_ms = millis();
    _idle_prompt_shown = false;
}

uint32_t Device::IdleMs() const {
    return millis() - _last_activity_ms;
}

void Device::CheckIdle() {
    if (!_config.auto_power_save) return;

    const uint32_t idle = IdleMs();

    if (!_idle_prompt_shown && idle >= _config.idle_prompt_ms) {
        _idle_prompt_shown = true;
        if (_on_idle_prompt && _on_idle_prompt()) {
            NoteActivity(); // the app cancelled the shutdown
            return;
        }
    }

    if (idle >= _config.idle_shutdown_ms) {
        log_i("idle %u ms, shutting down", idle);
        Shutdown();
    }
}

void Device::Sleep(uint32_t wake_after_seconds) {
    _display.WaitIdle();
    _power.Sleep(wake_after_seconds);
}

}  // namespace m5ui
