#pragma once
// Gallery: one of every component, so a change to the library is visible on a
// single screen.

#include "demo_screen.h"

namespace demo {

class ComponentsScreen : public DemoScreen {
   public:
    ComponentsScreen();

   protected:
    void BuildContent(m5ui::Column& column) override;

   private:
    void BuildTypography(m5ui::Column& column);
    void BuildButtons(m5ui::Column& column);
    void BuildInputs(m5ui::Column& column);
    void BuildIndicators(m5ui::Column& column);
    void BuildGreyRamp(m5ui::Column& column);

    m5ui::Label* _tap_feedback = nullptr;
    m5ui::ProgressBar* _progress = nullptr;
    float _progress_value = 0.35f;
};

}  // namespace demo
