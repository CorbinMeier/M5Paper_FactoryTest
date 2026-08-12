#include "calendar_app.h"

#include "calendar_screen.h"

namespace calendar {

CalendarApp::CalendarApp() : App("calendar-demo") {}

void CalendarApp::OnStart() {
    Register("calendar", []() { return new CalendarScreen(); });
    SetHome("calendar");
}

}  // namespace calendar
