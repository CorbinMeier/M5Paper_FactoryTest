#include "input.h"

#include <M5EPD.h>

#include "i2c_lock.h"
#include "tokens.h"

namespace m5ui {

namespace {
constexpr uint32_t kKeyRepeatDelayMs = 450;
constexpr uint32_t kKeyRepeatRateMs = 90;

int16_t Abs16(int16_t v) {
    return v < 0 ? (int16_t)-v : v;
}
}  // namespace

// ---------------------------------------------------------- InputEvent ----

InputEvent InputEvent::MakeKey(uint16_t keycode, uint16_t codepoint,
                               uint8_t modifiers, InputSource src) {
    InputEvent e;
    e.src = src;
    e.kind = InputKind::Key;
    e.keycode = keycode;
    e.codepoint = codepoint;
    e.modifiers = modifiers;
    e.t = millis();
    return e;
}

InputEvent InputEvent::MakeTap(int16_t x, int16_t y) {
    InputEvent e;
    e.src = InputSource::Synthetic;
    e.kind = InputKind::Tap;
    e.x = x;
    e.y = y;
    e.t = millis();
    return e;
}

// ---------------------------------------------------------- InputQueue ----

bool InputQueue::Push(const InputEvent& e) {
    portENTER_CRITICAL(&_mux);
    if (_count == kCapacity) {
        // Drop the oldest rather than the newest: a stale pointer position is
        // worth less than the current one.
        _head = (uint8_t)((_head + 1) % kCapacity);
        _count--;
        _dropped++;
    }
    _buf[_tail] = e;
    _tail = (uint8_t)((_tail + 1) % kCapacity);
    _count++;
    portEXIT_CRITICAL(&_mux);
    return true;
}

bool InputQueue::Pop(InputEvent& out) {
    portENTER_CRITICAL(&_mux);
    if (_count == 0) {
        portEXIT_CRITICAL(&_mux);
        return false;
    }
    out = _buf[_head];
    _head = (uint8_t)((_head + 1) % kCapacity);
    _count--;
    portEXIT_CRITICAL(&_mux);
    return true;
}

void InputQueue::Clear() {
    portENTER_CRITICAL(&_mux);
    _head = _tail = _count = 0;
    portEXIT_CRITICAL(&_mux);
}

// --------------------------------------------------------- TouchSource ----

void TouchSource::Begin() {
    I2CLock lock;
    M5.TP.SetRotation(90);
}

void TouchSource::Poll(InputQueue& queue) {
    // Runs on the input task while the UI task may be reading the RTC on the
    // same bus (issue #107). Scoped so the lock is released before the gesture
    // work below, which touches no hardware.
    bool finger = false;
    int16_t x = _last_x;
    int16_t y = _last_y;
    {
        I2CLock lock;
        M5.TP.update();
        finger = !M5.TP.isFingerUp();
        if (!finger && !_down) {
            return; // idle -- the common case, costs one I2C read
        }
        if (finger) {
            tp_finger_t f = M5.TP.readFinger(0);
            x = (int16_t)f.x;
            y = (int16_t)f.y;
        }
    }

    const uint32_t now = millis();

    InputEvent e;
    e.src = InputSource::Touch;
    e.x = x;
    e.y = y;
    e.t = now;

    if (finger && !_down) {
        // ---- press -------------------------------------------------------
        _down = true;
        _dragging = false;
        _long_press_sent = false;
        _down_x = _last_x = x;
        _down_y = _last_y = y;
        _down_t = _last_move_t = now;
        _velocity = 0;

        e.kind = InputKind::PointerDown;
        queue.Push(e);
    } else if (finger && _down) {
        // ---- move --------------------------------------------------------
        const int16_t dx = (int16_t)(x - _last_x);
        const int16_t dy = (int16_t)(y - _last_y);

        if (!_dragging) {
            const int16_t total_dx = (int16_t)(x - _down_x);
            const int16_t total_dy = (int16_t)(y - _down_y);
            if (Abs16(total_dx) > (int16_t)tok::kDragSlopPx ||
                Abs16(total_dy) > (int16_t)tok::kDragSlopPx) {
                _dragging = true;
            } else if (!_long_press_sent && (now - _down_t) >= tok::kLongPressMs) {
                _long_press_sent = true;
                e.kind = InputKind::LongPress;
                queue.Push(e);
                _last_x = x;
                _last_y = y;
                return;
            }
        }

        if (_dragging && (dx != 0 || dy != 0)) {
            const uint32_t dt = now - _last_move_t;
            if (dt > 0) _velocity = (int16_t)((dy * 1000) / (int32_t)dt);

            e.kind = InputKind::PointerMove;
            e.dx = dx;
            e.dy = dy;
            queue.Push(e);
            _last_move_t = now;
        }
        _last_x = x;
        _last_y = y;
    } else {
        // ---- release -----------------------------------------------------
        _down = false;
        e.x = _last_x;
        e.y = _last_y;
        e.kind = InputKind::PointerUp;
        queue.Push(e);

        if (_dragging) {
            // A fling only counts if the finger was still moving on release.
            if (Abs16(_velocity) > 200 && (now - _last_move_t) < 80) {
                e.kind = InputKind::Fling;
                e.dz = _velocity;
                queue.Push(e);
            }
        } else if (!_long_press_sent && (now - _down_t) <= tok::kTapMaxMs) {
            e.kind = InputKind::Tap;
            e.dz = 0;
            queue.Push(e);
        }
        _dragging = false;
        _long_press_sent = false;
    }
}

// ---------------------------------------------------- SideButtonSource ----

void SideButtonSource::SetMapping(SideButton b, uint16_t keycode) {
    _map[(uint8_t)b] = keycode;
}

void SideButtonSource::Poll(InputQueue& queue) {
    M5.update();
    const uint32_t now = millis();

    const bool down[3] = {M5.BtnL.isPressed() != 0, M5.BtnP.isPressed() != 0,
                          M5.BtnR.isPressed() != 0};

    for (uint8_t i = 0; i < 3; ++i) {
        InputEvent e;
        e.src = InputSource::SideButton;
        e.keycode = _map[i];
        e.t = now;

        if (down[i] && !_was_down[i]) {
            _down_t[i] = now;
            _last_repeat[i] = now;
            e.kind = InputKind::Key;
            queue.Push(e);
        } else if (down[i] && _was_down[i]) {
            const bool past_delay = (now - _down_t[i]) >= kKeyRepeatDelayMs;
            const bool due = (now - _last_repeat[i]) >= kKeyRepeatRateMs;
            if (past_delay && due) {
                _last_repeat[i] = now;
                e.kind = InputKind::KeyRepeat;
                queue.Push(e);
            }
        } else if (!down[i] && _was_down[i]) {
            e.kind = InputKind::KeyUp;
            queue.Push(e);
        }
        _was_down[i] = down[i];
    }
}

}  // namespace m5ui
