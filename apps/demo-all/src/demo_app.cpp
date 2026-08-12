#include "demo_app.h"

#include "screens/components_screen.h"
#include "screens/device_info_screen.h"
#include "screens/home_screen.h"
#include "screens/notes_screen.h"
#include "screens/render_screen.h"

using namespace m5ui;

namespace demo {

DemoApp::DemoApp() : App("demo-all") {
    // Factories, not instances: a screen's widget tree is not allocated until
    // the user navigates to it, and is torn down again once it is buried
    // deeper than App::kColdDepth.
    Register("home", []() -> Screen* { return new HomeScreen(); });
    Register("device", []() -> Screen* { return new DeviceInfoScreen(); });
    Register("components", []() -> Screen* { return new ComponentsScreen(); });
    Register("notes", []() -> Screen* { return new NotesScreen(); });
    Register("render", []() -> Screen* { return new RenderScreen(); });

    SetHome("home");
}

void DemoApp::OnStart() {
    Device& dev = Device::Get();

    // The left side button is back everywhere in this app; the other two keep
    // their default paging behaviour.
    dev.Buttons().SetMapping(SideButton::Left, key::kEscape);

    // Idle policy: warn on the panel before cutting power, rather than the
    // device simply vanishing.
    dev.OnIdlePrompt([]() {
        log_i("idle -- shutting down shortly");
        return false; // do not cancel
    });
}

}  // namespace demo
