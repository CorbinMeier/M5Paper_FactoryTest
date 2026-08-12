#pragma once
// Rendering diagnostics: update modes, ghost debt, flush counts.
//
// This screen is how the update-mode policy gets tuned -- the debt heatmap
// shows which tiles are carrying residue and the counters show what the policy
// engine actually chose.

#include "demo_screen.h"

namespace demo {

class RenderScreen : public DemoScreen {
   public:
    RenderScreen();

    void Tick() override;

   protected:
    void BuildContent(m5ui::Column& column) override;

   private:
    m5ui::Label* _stats = nullptr;
    m5ui::Label* _counter = nullptr;
    uint32_t _counter_value = 0;
    uint32_t _last_tick_ms = 0;
};

}  // namespace demo
