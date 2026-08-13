#pragma once
// BLE companion peripheral (issue #112): a phone app connects and pushes the
// current time and hourly weather. Opt-in -- #include this only in an app
// that wants BLE; it pulls in NimBLE-Arduino, which is not free (a spike
// measured ~19 KB of internal DRAM for a bare advertiser, see issue #112).
//
// This is a GATT *peripheral* (the M5Paper advertises, the phone connects as
// a BLE central) -- the opposite role from the BLE HID epic (#24), where the
// M5Paper would be a central pairing with a keyboard/mouse. Long-term this is
// meant to back a pager-style Android companion app (separate future
// project, not in this repo); for now it only needs to accept time and
// weather writes, verifiable with a generic BLE tool (nRF Connect).
//
// GATT layout, documented here so the eventual Android app and this class
// never drift:
//
//   service   7d2a1000-9c3e-4b6f-9a2e-8f4b0e5d6a01
//     time    7d2a1001-9c3e-4b6f-9a2e-8f4b0e5d6a01  write, 6 bytes LE:
//                 uint32 epoch_seconds, int16 tz_offset_minutes
//     weather 7d2a1002-9c3e-4b6f-9a2e-8f4b0e5d6a01  write, 8 + 3*hour_count
//             bytes LE:
//                 uint32 updated_at_epoch
//                 int16  current_temp_c_x10   (Celsius * 10, e.g. 235 = 23.5C)
//                 uint8  current_condition    (WeatherCondition)
//                 uint8  hour_count           (<= kMaxHours)
//                 hour_count * { int16 temp_c_x10; uint8 condition; }
//     status  7d2a1003-9c3e-4b6f-9a2e-8f4b0e5d6a01  read, same layout as
//             weather plus a trailing uint32 time_synced_at_epoch -- lets a
//             BLE debug tool confirm a write landed without guessing.
//
// Payloads over ~20 bytes need the client to negotiate a larger ATT MTU
// (BLE's default is 23 bytes, 20 of it usable) -- the eventual Android app
// should call requestMtu() before writing weather with more than ~4 hours.
// NimBLE negotiates on this side automatically; nothing here changes for it.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <stdint.h>

namespace m5ui {

enum class WeatherCondition : uint8_t {
    Unknown = 0,
    Clear,
    PartlyCloudy,
    Cloudy,
    Rain,
    Snow,
    Thunderstorm,
    Fog,
};

struct WeatherHour {
    int16_t temp_c_x10 = 0;
    uint8_t condition = 0; // WeatherCondition
};

struct WeatherSnapshot {
    static constexpr uint8_t kMaxHours = 12;

    uint32_t updated_at_epoch = 0; // 0 = never received
    int16_t current_temp_c_x10 = 0;
    uint8_t current_condition = 0; // WeatherCondition
    uint8_t hour_count = 0;
    WeatherHour hours[kMaxHours];

    bool HasData() const {
        return updated_at_epoch != 0;
    }
};

// Pull-based, like InputQueue (issue #107) -- the NimBLE host callbacks run
// on NimBLE's own task, not the UI task, so writes land behind a spinlock and
// the app drains them once per loop tick rather than a callback firing from
// the wrong task context.
class BleCompanion {
   public:
    // Starts advertising under `device_name`. Safe to call once; Stop() then
    // Begin() again to restart.
    void Begin(const char* device_name = "M5Paper");
    void Stop();

    bool IsAdvertising() const;
    bool IsConnected() const;

    // True and clears the pending flag exactly once per write -- call this
    // every loop tick from the UI task. Returns false (and leaves `epoch`/
    // `tz_minutes` untouched) when nothing new has arrived.
    bool TakeTimeSync(uint32_t& epoch, int16_t& tz_minutes);
    bool TakeWeatherUpdate(WeatherSnapshot& out);

    // Last-known values, independent of whether they have been "taken" --
    // for a screen that just wants to display current state.
    uint32_t LastSyncedEpoch() const {
        return _time_synced_at_epoch;
    }
    int16_t TimeZoneOffsetMinutes() const {
        return _tz_offset_minutes;
    }
    WeatherSnapshot LastWeather() const;

    // Called by the GATT callbacks (NimBLE task). Not for app code.
    void HandleTimeWrite(const uint8_t* data, size_t len);
    void HandleWeatherWrite(const uint8_t* data, size_t len);
    // Serializes Weather() + LastSyncedEpoch() into the status characteristic
    // wire format, for the read callback.
    size_t EncodeStatus(uint8_t* out, size_t out_capacity) const;

   private:
    mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    uint32_t _time_synced_at_epoch = 0;
    int16_t _tz_offset_minutes = 0;
    bool _time_pending = false;

    WeatherSnapshot _weather;
    bool _weather_pending = false;

    void* _server = nullptr; // NimBLEServer*, opaque so NimBLE stays out of the header
    void* _status_char = nullptr; // NimBLECharacteristic*, same reason
    bool _advertising = false;
    bool _connected = false;

    friend class BleCompanionServerCallbacks;
    friend class BleCompanionCharCallbacks;
};

}  // namespace m5ui
