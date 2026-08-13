#include "calendar_app.h"

#include "calendar_screen.h"
#include "pairing_screen.h"

namespace calendar {

CalendarApp::CalendarApp() : App("calendar-demo") {}

void CalendarApp::OnStart() {
    Register("calendar", [this]() { return new CalendarScreen(_weather); });
    Register("pairing", [this]() { return new PairingScreen(_ble); });
    SetHome(_pairing_requested ? "pairing" : "calendar");
}

}  // namespace calendar
