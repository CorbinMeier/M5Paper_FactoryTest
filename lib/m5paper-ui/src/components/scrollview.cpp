#include "scrollview.h"

namespace m5ui {

ScrollView::ScrollView() : Widget("scrollview") {}

Widget* ScrollView::SetContent(Widget* content) {
    if (_content != nullptr) Remove(_content);
    _content = content;
    if (_content != nullptr) Add(_content);
    _offset = 0;
    Invalidate();
    return _content;
}

int16_t ScrollView::MaxOffset() const {
    const int16_t viewport = ContentRect().h;
    return _content_h > viewport ? (int16_t)(_content_h - viewport) : 0;
}

void ScrollView::ClampOffset() {
    const int16_t maximum = (int16_t)(MaxOffset() + _rubber_band);
    const int16_t minimum = (int16_t)(0 - _rubber_band);
    if (_offset > maximum) _offset = maximum;
    if (_offset < minimum) _offset = minimum;
}

void ScrollView::ScrollTo(int16_t offset, bool animate) {
    (void)animate; // e-ink has no animation to offer; kept for API symmetry
    if (_offset == offset) return;
    _offset = offset;
    ClampOffset();
    Invalidate();
}

void ScrollView::ScrollBy(int16_t delta) {
    ScrollTo((int16_t)(_offset + delta));
}

void ScrollView::ScrollToTop() {
    ScrollTo(0);
}

void ScrollView::ScrollToBottom() {
    ScrollTo(MaxOffset());
}

void ScrollView::ScrollIntoView(const Rect& r) {
    const int16_t viewport = ContentRect().h;
    if (r.y < _offset) {
        ScrollTo(r.y);
    } else if (r.Bottom() > _offset + viewport) {
        ScrollTo((int16_t)(r.Bottom() - viewport));
    }
}

void ScrollView::SetShowScrollbar(bool show) {
    if (_show_scrollbar == show) return;
    _show_scrollbar = show;
    Invalidate();
}

void ScrollView::SetRubberBand(int16_t px) {
    _rubber_band = px;
}

Size ScrollView::Measure(const Constraints& c) {
    // A scroll view takes whatever it is given; its content is what varies.
    return Size{c.max_w, c.max_h};
}

void ScrollView::Layout(const Rect& bounds) {
    SetFrame(bounds);
    if (_content == nullptr) return;

    const Rect box = ContentRect();
    const int16_t track = _show_scrollbar
                              ? (int16_t)(box.w - tok::kScrollbarWidth - tok::kSpaceXs)
                              : box.w;

    // Content is laid out at its natural height, unbounded vertically -- the
    // viewport crops it rather than the layout compressing it.
    const Size natural = _content->Measure(Constraints::Loose(track, INT16_MAX));
    _content_h = natural.h;

    // Content coordinates: origin at the top of the scrollable area, not the
    // viewport. Draw() applies the offset.
    _content->Layout(Rect{box.x, box.y, track, _content_h});
    ClampOffset();
}

void ScrollView::Draw(PaintContext& ctx) {
    if (!_visible || _content == nullptr) return;

    const Rect viewport = ctx.ToCanvas(_frame);
    if (viewport.IsEmpty()) return;

    // Children draw through a shifted, clipped context so nothing escapes the
    // viewport and everything lands at the scrolled position.
    PaintContext inner = ctx;
    inner.clip = viewport.Intersection(ctx.clip);
    inner.scroll_y = (int16_t)(ctx.scroll_y + _offset);

    DrawSelf(ctx);
    _content->Draw(inner);

    if (_show_scrollbar) DrawScrollbar(ctx);
    if (_focused) DrawFocusRing(ctx);
    _needs_paint = false;
}

void ScrollView::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;
    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;
    ctx.canvas->fillRect(r.x, r.y, r.w, r.h, tok::kSurface);
}

void ScrollView::DrawScrollbar(PaintContext& ctx) {
    const int16_t maximum = MaxOffset();
    if (maximum <= 0 || ctx.canvas == nullptr) return; // nothing to scroll

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    const int16_t track_h = r.h;
    const int16_t viewport = ContentRect().h;

    // Thumb length is proportional to the visible fraction, with a floor so it
    // stays grabbable on a very long document.
    int16_t thumb_h = (int16_t)(((int32_t)track_h * viewport) / _content_h);
    if (thumb_h < 24) thumb_h = 24;

    const int16_t clamped = _offset < 0 ? 0 : (_offset > maximum ? maximum : _offset);
    const int16_t thumb_y =
        (int16_t)(r.y + ((int32_t)(track_h - thumb_h) * clamped) / maximum);

    const int16_t x = (int16_t)(r.Right() - tok::kScrollbarWidth);
    ctx.canvas->fillRect(x, thumb_y, tok::kScrollbarWidth, thumb_h, tok::kFg);
}

bool ScrollView::HandleEvent(const InputEvent& e) {
    if (!_visible || !_enabled) return false;

    // ---- keys: side buttons and a BLE keyboard page the view -------------
    if (e.IsKey() && e.kind != InputKind::KeyUp) {
        const int16_t page = (int16_t)(PageHeight() * 4 / 5); // keep context
        switch (e.keycode) {
            case key::kPageUp:   ScrollBy((int16_t)-page); return true;
            case key::kPageDown: ScrollBy(page); return true;
            case key::kHome:     ScrollToTop(); return true;
            case key::kEnd:      ScrollToBottom(); return true;
            case key::kUp:       ScrollBy(-tok::kListRowH); return true;
            case key::kDown:     ScrollBy(tok::kListRowH); return true;
            default: break;
        }
    }

    if (!e.IsPointer() && e.kind != InputKind::Scroll) return false;
    if (!_frame.Contains(e.x, e.y)) return false;

    switch (e.kind) {
        case InputKind::Scroll:
            ScrollBy((int16_t)(-e.dz * tok::kListRowH));
            return true;

        case InputKind::PointerDown:
            _drag_start_offset = _offset;
            // Not yet a drag -- let the child see the press so a button inside
            // can show its pressed state.
            break;

        case InputKind::PointerMove:
            // 1:1: the content moves exactly as far as the finger did.
            _dragging = true;
            ScrollBy((int16_t)-e.dy);
            return true;

        case InputKind::Fling: {
            // No inertial animation on e-ink. One proportional jump, then a
            // clean settle, reads better than a stuttering decay.
            const int16_t jump = (int16_t)(-e.dz / 4);
            ScrollBy(jump);
            _dragging = false;
            _rubber_band = 0;
            ClampOffset();
            Invalidate();
            return true;
        }

        case InputKind::PointerUp:
            if (_dragging) {
                _dragging = false;
                ClampOffset();
                Invalidate(); // settle with a clean pass
                return true;
            }
            break;

        default:
            break;
    }

    // Not consumed by scrolling -- hand it to the content, in its coordinates.
    if (_content != nullptr) {
        InputEvent shifted = e;
        shifted.y = (int16_t)(e.y + _offset);
        return _content->HandleEvent(shifted);
    }
    return false;
}

}  // namespace m5ui
