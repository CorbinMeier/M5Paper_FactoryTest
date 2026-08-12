#pragma once
// ScrollView with 1:1 finger tracking (#39), a 2px edge scrollbar (#40) and
// side-button paging (#41).
//
// "1:1" is the whole point on e-ink: the panel cannot animate, so smooth
// scrolling is not on offer. What it *can* do is follow the finger exactly
// during a drag using A2 updates, then settle with a clean pass on release.
// Anything that lags behind the finger reads as broken.

#include "../core/widget.h"

namespace m5ui {

class ScrollView : public Widget {
   public:
    ScrollView();

    // The single child that scrolls. Takes ownership; replaces any existing.
    Widget* SetContent(Widget* content);
    Widget* Content() const {
        return _content;
    }

    void ScrollTo(int16_t offset, bool animate = false);
    void ScrollBy(int16_t delta);
    void ScrollToTop();
    void ScrollToBottom();
    // Scrolls the minimum distance to bring `r` (content coordinates) into view.
    void ScrollIntoView(const Rect& r);

    int16_t Offset() const {
        return _offset;
    }
    int16_t MaxOffset() const;
    int16_t PageHeight() const {
        return ContentRect().h;
    }

    void SetShowScrollbar(bool show);
    // Overscroll resistance at the ends, 0 = hard stop.
    void SetRubberBand(int16_t px);

    Size Measure(const Constraints& c) override;
    void Layout(const Rect& bounds) override;
    void Draw(PaintContext& ctx) override;
    void DrawSelf(PaintContext& ctx) override;
    bool HandleEvent(const InputEvent& e) override;

    // Dragging wants A2 so it can keep up; a settled view wants a clean pass.
    DrawIntentHint PaintIntent() const override {
        return _dragging ? DrawIntentHint::Animated : DrawIntentHint::Static;
    }

   private:
    void DrawScrollbar(PaintContext& ctx);
    void ClampOffset();

    Widget* _content = nullptr;
    int16_t _offset = 0;      // pixels scrolled down; 0 = top
    int16_t _content_h = 0;
    int16_t _rubber_band = 0;
    bool _dragging = false;
    bool _show_scrollbar = true;
    int16_t _drag_start_offset = 0;
};

}  // namespace m5ui
