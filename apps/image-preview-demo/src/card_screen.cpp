#include "card_screen.h"

#include "assets/card_back.h"
#include "assets/card_front.h"

using namespace m5ui;

namespace card {

namespace {

const CardImage kImages[] = {
    {::card::kCardFrontJpg, ::card::kCardFrontLen, ::card::kCardFrontW,
     ::card::kCardFrontH, "front"},
    {::card::kCardBackJpg, ::card::kCardBackLen, ::card::kCardBackW,
     ::card::kCardBackH, "back"},
};
constexpr uint8_t kImageCount = sizeof(kImages) / sizeof(kImages[0]);

}  // namespace

CardScreen::CardScreen() : Screen("card") {}

void CardScreen::Build() {
    Column* root = new Column(0);
    // Add to the framework-owned ScreenRoot, don't replace it (issue #111).
    Root()->Add(root);

    _image = new Image();
    _image->SetFlex(1);
    root->Add(_image);

    ShowIndex(_index);
}

void CardScreen::ShowIndex(uint8_t index) {
    _index = (uint8_t)(index % kImageCount);
    const CardImage& img = kImages[_index];
    _image->SetSource(img.jpg, img.len, img.w, img.h);
}

bool CardScreen::HandleEvent(const InputEvent& e) {
    // Only side-button presses drive navigation; everything else (touch,
    // repeats, key-up) falls through to the normal widget dispatch.
    if (e.src == InputSource::SideButton && e.kind == InputKind::Key) {
        if (e.keycode == key::kPageDown) {  // G39, right
            ShowIndex((uint8_t)(_index + 1));
            InvalidateAll();
            return true;
        }
        if (e.keycode == key::kPageUp) {  // G37, left
            ShowIndex((uint8_t)(_index + kImageCount - 1));
            InvalidateAll();
            return true;
        }
    }
    return Screen::HandleEvent(e);
}

}  // namespace card
