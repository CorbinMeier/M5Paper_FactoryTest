#pragma once
// The demo application: registers every screen and nominates home.

#include <m5paper_ui.h>

namespace demo {

class DemoApp : public m5ui::App {
   public:
    DemoApp();

    void OnStart() override;
};

}  // namespace demo
