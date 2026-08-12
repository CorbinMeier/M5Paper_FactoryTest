#pragma once
// Home menu. Deliberately a VirtualList rather than a grid of image switches:
// the stock home screen allocated six 228x228 two-state canvases (~312 KB) to
// draw six icons.

#include "demo_screen.h"

namespace demo {

class HomeScreen : public DemoScreen {
   public:
    HomeScreen();

    bool OnBack() override {
        return true; // home is the root; back does nothing
    }

   protected:
    void BuildContent(m5ui::Column& column) override;

   private:
    struct MenuEntry {
        const char* route;
        const char* title;
        const char* subtitle;
    };

    static const MenuEntry kMenu[];
    static const uint32_t kMenuCount;
};

}  // namespace demo
