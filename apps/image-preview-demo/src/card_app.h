#pragma once
// One-screen app: registers "card" as the (only, home) route.

#include <m5paper_ui.h>

namespace card {

class CardApp : public m5ui::App {
   public:
    CardApp();
    void OnStart() override;
};

}  // namespace card
