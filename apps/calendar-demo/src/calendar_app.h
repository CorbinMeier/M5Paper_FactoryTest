#pragma once
// One-screen app: registers "calendar" as the (only, home) route.

#include <m5paper_ui.h>

namespace calendar {

class CalendarApp : public m5ui::App {
   public:
    CalendarApp();
    void OnStart() override;
};

}  // namespace calendar
