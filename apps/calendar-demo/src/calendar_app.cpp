#include "calendar_app.h"

#include "calendar_screen.h"

namespace calendar {

CalendarApp::CalendarApp() : App("calendar-demo") {}

void CalendarApp::OnStart() {
    Register("calendar", [this]() { return new CalendarScreen(_weather); });
    SetHome("calendar");
}

}  // namespace calendar
