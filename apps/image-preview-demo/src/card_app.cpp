#include "card_app.h"

#include "card_screen.h"

namespace card {

CardApp::CardApp() : App("image-preview-demo") {}

void CardApp::OnStart() {
    Register("card", []() { return new CardScreen(); });
    SetHome("card");
}

}  // namespace card
