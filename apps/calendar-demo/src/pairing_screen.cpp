#include "pairing_screen.h"

using namespace m5ui;

namespace calendar {

PairingScreen::PairingScreen(BleCompanion& ble) : Screen("pairing"), _ble(ble) {}

void PairingScreen::Build() {
    Column* root = new Column(tok::kSpaceLg);
    root->SetPadding(EdgeInsets::All(tok::kSpaceXl));
    root->SetJustify(Justify::Center);
    root->SetCrossAlign(Align::Center);
    Root()->Add(root);

    Heading* heading = new Heading("Pairing", 1);
    heading->SetColor(tok::kFg);
    heading->SetAlign(TextAlign::Center);
    root->Add(heading);

    _status = new Label("Waiting for phone to connect...", tok::kTextMd);
    _status->SetColor(tok::kFg);
    _status->SetAlign(TextAlign::Center);
    root->Add(_status);

    _code_label = new Label("", tok::kText2Xl);
    _code_label->SetColor(tok::kFg);
    _code_label->SetBold(true);
    _code_label->SetAlign(TextAlign::Center);
    root->Add(_code_label);

    Row* buttons = new Row(tok::kSpaceMd);
    root->Add(buttons);

    // m5ui::Button, qualified -- M5EPD.h declares a global Button that
    // `using namespace m5ui` makes ambiguous otherwise (issue #95).
    _confirm_button = new m5ui::Button("Confirm", ButtonVariant::Filled);
    _confirm_button->SetEnabled(false);
    _confirm_button->OnPress([this]() { OnConfirm(); });
    buttons->Add(_confirm_button);

    _reject_button = new m5ui::Button("Reject", ButtonVariant::Outline);
    _reject_button->SetEnabled(false);
    _reject_button->OnPress([this]() { OnReject(); });
    buttons->Add(_reject_button);
}

void PairingScreen::Tick() {
    if (_state == State::WaitingForConnection) {
        uint32_t pin;
        if (_ble.TakePendingPasskey(pin)) ShowCode(pin);
    } else if (_state == State::Confirming) {
        bool success;
        if (_ble.TakeAuthResult(success)) {
            SetStatus(success ? "Paired!" : "Pairing failed.");
            _code_label->SetText("");
            _state = State::Done;
        }
    }
}

void PairingScreen::ShowCode(uint32_t pin) {
    // Always exactly 6 digits -- NimBLE's numeric-comparison pin is 0-999999,
    // and Android's dialog zero-pads the same way.
    char buf[8];
    snprintf(buf, sizeof(buf), "%06u", (unsigned)(pin % 1000000));
    _code_label->SetText(String(buf));
    SetStatus("Confirm this matches your phone:");
    _confirm_button->SetEnabled(true);
    _reject_button->SetEnabled(true);
    _state = State::ShowingCode;
}

void PairingScreen::OnConfirm() {
    _ble.ConfirmPasskey(true);
    _confirm_button->SetEnabled(false);
    _reject_button->SetEnabled(false);
    SetStatus("Confirming...");
    _state = State::Confirming;
}

void PairingScreen::OnReject() {
    _ble.ConfirmPasskey(false);
    _confirm_button->SetEnabled(false);
    _reject_button->SetEnabled(false);
    SetStatus("Rejected.");
    // Still wait for TakeAuthResult -- NimBLE reports the failure on its own
    // once the peer's side unwinds, rather than this screen assuming it.
    _state = State::Confirming;
}

void PairingScreen::SetStatus(const String& text) {
    _status->SetText(text);
}

}  // namespace calendar
