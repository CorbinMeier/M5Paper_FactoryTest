#pragma once
// The only screen in image-preview-demo: one full-screen Image, cycled by
// the left/right side buttons (G37/G39 -> SideButton::Left/Right, mapped to
// key::kPageUp/kPageDown by default -- see lib/m5paper-ui/src/core/input.h).

#include <m5paper_ui.h>

namespace card {

struct CardImage {
    const uint8_t* jpg;
    size_t len;
    int16_t w;
    int16_t h;
    const char* caption;
};

class CardScreen : public m5ui::Screen {
   public:
    CardScreen();

    void Build() override;
    bool HandleEvent(const m5ui::InputEvent& e) override;

   private:
    void ShowIndex(uint8_t index);

    m5ui::Image* _image = nullptr;
    m5ui::Label* _caption = nullptr;
    uint8_t _index = 0;
};

}  // namespace card
