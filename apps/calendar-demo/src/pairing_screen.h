#pragma once
// Interactive BLE pairing (issue #114): shows the numeric-comparison code
// NimBLE hands back and lets the user confirm or reject it with on-screen
// touch buttons -- the same code Android's own pairing dialog shows at the
// same time, so the user is comparing two screens, not trusting one blindly.

#include <m5paper_ui.h>

#include "modules/ble_companion.h"

namespace calendar {

class PairingScreen : public m5ui::Screen {
   public:
    explicit PairingScreen(m5ui::BleCompanion& ble);

    void Build() override;
    void Tick() override;

    // True once a pairing attempt has resolved (paired or failed) and the
    // result has been shown -- the caller driving the loop watches this to
    // know when to stop waiting.
    bool IsDone() const {
        return _state == State::Done;
    }

   private:
    enum class State { WaitingForConnection, ShowingCode, Confirming, Done };

    void SetStatus(const String& text);
    void ShowCode(uint32_t pin);
    void OnConfirm();
    void OnReject();

    m5ui::BleCompanion& _ble;
    State _state = State::WaitingForConnection;

    m5ui::Label* _status = nullptr;
    m5ui::Label* _code_label = nullptr;
    m5ui::Button* _confirm_button = nullptr;
    m5ui::Button* _reject_button = nullptr;
};

}  // namespace calendar
