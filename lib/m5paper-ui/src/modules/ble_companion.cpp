#include "ble_companion.h"

// PlatformIO's LDF compiles every .cpp under lib/m5paper-ui/src for any app
// that pulls in this library, regardless of which headers that app actually
// includes -- so this file was compiling (and failing, no NimBLE dependency)
// for demo-all too, not just calendar-demo. #include-based opt-in (rule 1 in
// platformio.ini) cannot reach a whole translation unit like this one; a -D
// feature gate is the documented escape hatch (rule 3, same file) for
// exactly this case. Only calendar-demo's build_flags define this.
#ifdef M5UI_ENABLE_BLE_COMPANION

#include <NimBLEDevice.h>

namespace m5ui {

namespace {

constexpr const char* kServiceUuid = "7d2a1000-9c3e-4b6f-9a2e-8f4b0e5d6a01";
constexpr const char* kTimeCharUuid = "7d2a1001-9c3e-4b6f-9a2e-8f4b0e5d6a01";
constexpr const char* kWeatherCharUuid = "7d2a1002-9c3e-4b6f-9a2e-8f4b0e5d6a01";
constexpr const char* kStatusCharUuid = "7d2a1003-9c3e-4b6f-9a2e-8f4b0e5d6a01";

// Little-endian packing helpers -- the wire format is defined byte-by-byte in
// ble_companion.h, not left to struct layout/endianness of whatever compiles
// this, since the far end is eventually an Android app.
void PutU32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
uint32_t GetU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
void PutI16(uint8_t* p, int16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(((uint16_t)v) >> 8);
}
int16_t GetI16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// Encodes a WeatherSnapshot as { u32 updated_at, i16 temp, u8 condition,
// u8 hour_count, hour_count * (i16, u8) }. Shared by HandleWeatherWrite's
// mirror-back-on-read and EncodeStatus.
size_t EncodeWeather(const WeatherSnapshot& w, uint8_t* out, size_t capacity) {
    const size_t needed = 8 + (size_t)w.hour_count * 3;
    if (capacity < needed) return 0;

    PutU32(out, w.updated_at_epoch);
    PutI16(out + 4, w.current_temp_c_x10);
    out[6] = w.current_condition;
    out[7] = w.hour_count;
    for (uint8_t i = 0; i < w.hour_count; ++i) {
        uint8_t* h = out + 8 + i * 3;
        PutI16(h, w.hours[i].temp_c_x10);
        h[2] = w.hours[i].condition;
    }
    return needed;
}

}  // namespace

// ------------------------------------------------------------- callbacks ----

class BleCompanionCharCallbacks : public NimBLECharacteristicCallbacks {
   public:
    explicit BleCompanionCharCallbacks(BleCompanion* owner) : _owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo&) override {
        const NimBLEAttValue& value = characteristic->getValue();
        const std::string uuid = characteristic->getUUID().toString();
        if (uuid == kTimeCharUuid) {
            _owner->HandleTimeWrite(value.data(), value.length());
        } else if (uuid == kWeatherCharUuid) {
            _owner->HandleWeatherWrite(value.data(), value.length());
        }
    }

   private:
    BleCompanion* _owner;
};

class BleCompanionServerCallbacks : public NimBLEServerCallbacks {
   public:
    explicit BleCompanionServerCallbacks(BleCompanion* owner) : _owner(owner) {}

    void onConnect(NimBLEServer*, NimBLEConnInfo&) override {
        portENTER_CRITICAL(&_owner->_mux);
        _owner->_connected = true;
        portEXIT_CRITICAL(&_owner->_mux);
    }
    void onDisconnect(NimBLEServer* server, NimBLEConnInfo&, int) override {
        portENTER_CRITICAL(&_owner->_mux);
        _owner->_connected = false;
        portEXIT_CRITICAL(&_owner->_mux);
        // A peripheral stops advertising once connected; resume so a second
        // phone (or a reconnect) can find it.
        server->getAdvertising()->start();
    }

   private:
    BleCompanion* _owner;
};

// ------------------------------------------------------------- BleCompanion --

void BleCompanion::Begin(const char* device_name) {
    NimBLEDevice::init(device_name);

    NimBLEServer* server = NimBLEDevice::createServer();
    // Static, not heap-tracked by BleCompanion: NimBLE keeps its own
    // ownership of registered callback objects and outlives this call either
    // way, so there is nothing to free on Stop() (library rule 2, issue #87,
    // is about file-scope objects at static init -- these are function-local
    // statics, constructed on first Begin(), which is safe).
    static BleCompanionServerCallbacks server_callbacks(this);
    static BleCompanionCharCallbacks char_callbacks(this);
    server->setCallbacks(&server_callbacks);

    NimBLEService* service = server->createService(kServiceUuid);

    NimBLECharacteristic* time_char = service->createCharacteristic(
        kTimeCharUuid, NIMBLE_PROPERTY::WRITE);
    time_char->setCallbacks(&char_callbacks);

    NimBLECharacteristic* weather_char = service->createCharacteristic(
        kWeatherCharUuid, NIMBLE_PROPERTY::WRITE);
    weather_char->setCallbacks(&char_callbacks);

    NimBLECharacteristic* status_char =
        service->createCharacteristic(kStatusCharUuid, NIMBLE_PROPERTY::READ);
    _status_char = status_char;

    server->start();
    _server = server;

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(kServiceUuid);
    advertising->start();
    _advertising = true;
}

void BleCompanion::Stop() {
    if (_advertising) {
        NimBLEDevice::getAdvertising()->stop();
        _advertising = false;
    }
}

bool BleCompanion::IsAdvertising() const {
    return _advertising;
}

bool BleCompanion::IsConnected() const {
    portENTER_CRITICAL(&_mux);
    const bool connected = _connected;
    portEXIT_CRITICAL(&_mux);
    return connected;
}

bool BleCompanion::TakeTimeSync(uint32_t& epoch, int16_t& tz_minutes) {
    portENTER_CRITICAL(&_mux);
    const bool had = _time_pending;
    if (had) {
        epoch = _time_synced_at_epoch;
        tz_minutes = _tz_offset_minutes;
        _time_pending = false;
    }
    portEXIT_CRITICAL(&_mux);
    return had;
}

bool BleCompanion::TakeWeatherUpdate(WeatherSnapshot& out) {
    portENTER_CRITICAL(&_mux);
    const bool had = _weather_pending;
    if (had) {
        out = _weather;
        _weather_pending = false;
    }
    portEXIT_CRITICAL(&_mux);
    return had;
}

WeatherSnapshot BleCompanion::LastWeather() const {
    portENTER_CRITICAL(&_mux);
    const WeatherSnapshot snapshot = _weather;
    portEXIT_CRITICAL(&_mux);
    return snapshot;
}

void BleCompanion::HandleTimeWrite(const uint8_t* data, size_t len) {
    if (len < 6) return; // malformed -- ignore rather than half-apply

    portENTER_CRITICAL(&_mux);
    _time_synced_at_epoch = GetU32(data);
    _tz_offset_minutes = GetI16(data + 4);
    _time_pending = true;
    portEXIT_CRITICAL(&_mux);
}

void BleCompanion::HandleWeatherWrite(const uint8_t* data, size_t len) {
    if (len < 8) return;

    const uint8_t hour_count = data[7];
    if (hour_count > WeatherSnapshot::kMaxHours) return;
    if (len < 8 + (size_t)hour_count * 3) return; // truncated payload

    WeatherSnapshot snapshot;
    snapshot.updated_at_epoch = GetU32(data);
    snapshot.current_temp_c_x10 = GetI16(data + 4);
    snapshot.current_condition = data[6];
    snapshot.hour_count = hour_count;
    for (uint8_t i = 0; i < hour_count; ++i) {
        const uint8_t* h = data + 8 + i * 3;
        snapshot.hours[i].temp_c_x10 = GetI16(h);
        snapshot.hours[i].condition = h[2];
    }

    portENTER_CRITICAL(&_mux);
    _weather = snapshot;
    _weather_pending = true;
    portEXIT_CRITICAL(&_mux);

    if (_status_char != nullptr) {
        uint8_t buf[8 + WeatherSnapshot::kMaxHours * 3 + 4];
        const size_t n = EncodeStatus(buf, sizeof(buf));
        if (n > 0) {
            static_cast<NimBLECharacteristic*>(_status_char)->setValue(buf, n);
        }
    }
}

size_t BleCompanion::EncodeStatus(uint8_t* out, size_t out_capacity) const {
    const WeatherSnapshot snapshot = LastWeather();
    const size_t weather_len = EncodeWeather(snapshot, out, out_capacity);
    if (weather_len == 0) return 0;
    if (out_capacity < weather_len + 4) return 0;

    PutU32(out + weather_len, LastSyncedEpoch());
    return weather_len + 4;
}

}  // namespace m5ui

#endif  // M5UI_ENABLE_BLE_COMPANION
