#pragma once
// ProgressBar and Meter (issue #58).
//
// Meter is the read-only sibling: same geometry, but labelled with a value and
// intended for gauges (battery, free DRAM, storage used) rather than task
// progress.

#include <Arduino.h>

#include "../core/widget.h"

namespace m5ui {

class ProgressBar : public Widget {
   public:
    ProgressBar();

    // 0..1. Values outside the range are clamped.
    void SetValue(float value);
    float Value() const {
        return _value;
    }
    // Indeterminate progress cannot animate on e-ink, so it renders as a
    // hatched track rather than a moving stripe.
    void SetIndeterminate(bool v);
    void SetThickness(int16_t px);

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;
    DrawIntentHint PaintIntent() const override {
        return DrawIntentHint::Text;
    }

   protected:
    float _value = 0.0f;
    bool _indeterminate = false;
    int16_t _thickness = 12;
};

class Meter : public ProgressBar {
   public:
    explicit Meter(const String& caption = "");

    void SetCaption(const String& caption);
    // Text shown at the right, e.g. "213 KB" or "74%". Free-form so the caller
    // controls units.
    void SetValueText(const String& text);
    // Below this fraction the fill draws hatched -- a low-battery or
    // low-memory warning without needing a colour the panel does not have.
    void SetWarnBelow(float fraction);

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;

   private:
    String _caption;
    String _value_text;
    float _warn_below = 0.0f;
};

}  // namespace m5ui
