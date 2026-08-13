#pragma once
// Unified input event queue (issue #28) -- the keystone of the input stack.
//
// Every source (touch panel, the three side buttons, a BLE keyboard, a BLE
// mouse wheel) pushes into one queue. The main loop drains it and dispatches
// through the focus manager. No widget and no screen reads M5.TP or M5.Btn*
// directly.

#include <freertos/FreeRTOS.h>
#include <stdint.h>

#include <functional>

#include "geometry.h"

namespace m5ui {

enum class InputSource : uint8_t {
    None = 0,
    Touch,
    SideButton,
    BleKeyboard,
    BleMouse,
    Synthetic // injected by tests or by the on-screen keyboard
};

enum class InputKind : uint8_t {
    None = 0,
    PointerDown,
    PointerMove,
    PointerUp,
    Tap,       // synthesised from a down/up pair inside the slop + time budget
    LongPress,
    Fling,     // released while moving; dz carries velocity in px/s
    Key,       // keycode + modifiers, on press
    KeyRepeat,
    KeyUp,
    Scroll,    // dz = wheel detents or accumulated drag delta
    SystemMenu // Push button held past kSystemMenuHoldMs (issue #115) --
               // global, so it is neither IsPointer() nor IsKey()
};

// Physical side buttons, delivered as Key events so one dispatch path serves
// both them and a BLE keyboard.
enum class SideButton : uint8_t { Left = 0, Push, Right };

// Keycodes are USB HID usage IDs so a BLE HID keyboard needs no translation.
namespace key {
constexpr uint16_t kEnter = 0x28;
constexpr uint16_t kEscape = 0x29;
constexpr uint16_t kBackspace = 0x2A;
constexpr uint16_t kTab = 0x2B;
constexpr uint16_t kSpace = 0x2C;
constexpr uint16_t kRight = 0x4F;
constexpr uint16_t kLeft = 0x50;
constexpr uint16_t kDown = 0x51;
constexpr uint16_t kUp = 0x52;
constexpr uint16_t kPageUp = 0x4B;
constexpr uint16_t kPageDown = 0x4E;
constexpr uint16_t kHome = 0x4A;
constexpr uint16_t kEnd = 0x4D;
}  // namespace key

namespace mod {
constexpr uint8_t kCtrl = 1 << 0;
constexpr uint8_t kShift = 1 << 1;
constexpr uint8_t kAlt = 1 << 2;
constexpr uint8_t kGui = 1 << 3;
}  // namespace mod

struct InputEvent {
    InputSource src = InputSource::None;
    InputKind kind = InputKind::None;
    int16_t x = 0;
    int16_t y = 0;
    int16_t dx = 0;
    int16_t dy = 0;
    int16_t dz = 0;         // scroll detents, or fling velocity
    uint16_t keycode = 0;   // HID usage id
    uint16_t codepoint = 0; // decoded character, 0 when not printable
    uint8_t modifiers = 0;
    uint32_t t = 0; // millis() at capture

    bool IsPointer() const {
        return kind == InputKind::PointerDown || kind == InputKind::PointerMove ||
               kind == InputKind::PointerUp || kind == InputKind::Tap ||
               kind == InputKind::LongPress || kind == InputKind::Fling;
    }
    bool IsKey() const {
        return kind == InputKind::Key || kind == InputKind::KeyRepeat ||
               kind == InputKind::KeyUp;
    }
    bool Has(uint8_t m) const {
        return (modifiers & m) != 0;
    }

    static InputEvent MakeKey(uint16_t keycode, uint16_t codepoint = 0,
                              uint8_t modifiers = 0,
                              InputSource src = InputSource::Synthetic);
    static InputEvent MakeTap(int16_t x, int16_t y);
};

// Fixed-capacity ring. No dynamic allocation, so a source can push from an ISR
// or a driver task without touching the heap. Oldest events are dropped when
// full -- a stale pointer-move is worth less than the newest one.
class InputQueue {
   public:
    static constexpr uint8_t kCapacity = 32;

    bool Push(const InputEvent& e);
    bool Pop(InputEvent& out);
    void Clear();

    bool IsEmpty() const {
        return _count == 0;
    }
    uint8_t Count() const {
        return _count;
    }
    uint32_t DroppedCount() const {
        return _dropped;
    }

   private:
    // The producer runs on the input task and the consumer on the UI task
    // (issue #107), so every mutation is guarded. A lock-free SPSC ring is not
    // available here: drop-oldest advances _head, which is the consumer's
    // index, and _count is written from both sides. The critical sections are
    // a handful of instructions, so contention is not a concern.
    //
    // Task context only -- Push() from an ISR would need the _ISR variants.
    mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    InputEvent _buf[kCapacity];
    uint8_t _head = 0;
    uint8_t _tail = 0;
    uint8_t _count = 0;
    uint32_t _dropped = 0;
};

// Base for anything that produces events. Device owns the concrete sources and
// polls them once per frame.
class InputSourceBase {
   public:
    virtual ~InputSourceBase() = default;
    virtual void Begin() {}
    // Reads the hardware and pushes whatever it found. Must not block.
    virtual void Poll(InputQueue& queue) = 0;
    virtual const char* Name() const = 0;
};

// Touch panel. Owns gesture recognition: raw down/move/up become Tap,
// LongPress, Fling and Scroll so no widget re-implements the slop math.
class TouchSource : public InputSourceBase {
   public:
    void Begin() override;
    void Poll(InputQueue& queue) override;
    const char* Name() const override {
        return "touch";
    }

    bool IsDown() const {
        return _down;
    }
    Point LastPoint() const {
        return Point{_last_x, _last_y};
    }

   private:
    bool _down = false;
    bool _dragging = false;
    bool _long_press_sent = false;
    int16_t _down_x = 0, _down_y = 0;
    int16_t _last_x = 0, _last_y = 0;
    uint32_t _down_t = 0;
    uint32_t _last_move_t = 0;
    int16_t _velocity = 0;
};

// The three side buttons, delivered as Key events with synthetic keycodes so
// they route through the focus manager like any other key.
//
// Push (G38) is special-cased: it does not repeat like Left/Right. A short
// press still delivers a single Key on release (so it keeps acting as Enter),
// but a hold past kSystemMenuHoldMs fires one SystemMenu event instead and
// suppresses the Key entirely -- otherwise the seconds spent reaching that
// threshold would spam Enter into whatever currently has focus.
class SideButtonSource : public InputSourceBase {
   public:
    void Poll(InputQueue& queue) override;
    const char* Name() const override {
        return "buttons";
    }

    // Default mapping: Left -> PageUp, Push -> Enter, Right -> PageDown.
    // Issue #41 binds these to scrolling; overridable per app.
    void SetMapping(SideButton b, uint16_t keycode);

   private:
    uint16_t _map[3] = {key::kPageUp, key::kEnter, key::kPageDown};
    bool _was_down[3] = {false, false, false};
    uint32_t _down_t[3] = {0, 0, 0};
    uint32_t _last_repeat[3] = {0, 0, 0};
    bool _system_menu_sent[3] = {false, false, false}; // Push only
};

}  // namespace m5ui
