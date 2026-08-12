#include "listview.h"

namespace m5ui {

VirtualList::VirtualList() : Widget("virtuallist") {
    _focusable = true;
}

void VirtualList::SetDataSource(CountProvider count, ItemProvider item) {
    _count = std::move(count);
    _item = std::move(item);
    Reload();
}

void VirtualList::SetRowHeight(int16_t h) {
    if (_row_h == h || h <= 0) return;
    _row_h = h;
    Invalidate();
}

void VirtualList::SetOverscan(uint8_t rows) {
    _overscan = rows;
}

void VirtualList::Reload() {
    _count_valid = false;
    Invalidate();
}

int16_t VirtualList::ContentHeight() const {
    const uint32_t n = _count ? _count() : 0;
    const int32_t h = (int32_t)n * _row_h;
    return h > INT16_MAX ? INT16_MAX : (int16_t)h;
}

void VirtualList::ScrollToIndex(uint32_t index) {
    const int16_t top = (int16_t)(index * _row_h);
    const int16_t viewport = ContentRect().h;

    if (top < _offset) {
        _offset = top;
    } else if (top + _row_h > _offset + viewport) {
        _offset = (int16_t)(top + _row_h - viewport);
    }
    if (_offset < 0) _offset = 0;
    Invalidate();
}

void VirtualList::SetSelectedIndex(int32_t index) {
    if (_selected == index) return;
    _selected = index;
    if (index >= 0) ScrollToIndex((uint32_t)index);
    Invalidate();
}

int32_t VirtualList::IndexAt(int16_t y) const {
    const Rect box = ContentRect();
    const int32_t idx = (int32_t)((y - box.y + _offset) / _row_h);
    const uint32_t n = _count ? _count() : 0;
    return (idx < 0 || idx >= (int32_t)n) ? -1 : idx;
}

Size VirtualList::Measure(const Constraints& c) {
    // Inside a scroll view the constraint's max_h is effectively unbounded, so
    // filling it would produce a viewport taller than the panel. Honour an
    // explicit preferred height, then the content height, and only fill as a
    // last resort.
    if (_params.preferred.h > 0) {
        return c.Clamp(Size{c.max_w, _params.preferred.h});
    }
    const int16_t content = ContentHeight();
    return c.Clamp(Size{c.max_w, content > 0 ? content : c.max_h});
}

void VirtualList::DrawRow(PaintContext& ctx, const ListItem& item,
                          const Rect& row, bool selected) {
    const Rect r = ctx.ToCanvas(row);
    if (r.IsEmpty()) return;

    const uint8_t bg = selected ? tok::kInverseBg : tok::kSurface;
    const uint8_t fg = item.disabled
                           ? tok::kFgDisabled
                           : (selected ? tok::kInverseFg : tok::kFg);

    ctx.canvas->fillRect(r.x, r.y, r.w, r.h, bg);
    ctx.canvas->drawFastHLine(r.x, (int16_t)(r.Bottom() - 1), r.w,
                              selected ? bg : tok::kBorder);

    TextStyle title;
    title.size = tok::kTextMd;
    title.color = fg;

    TextStyle sub;
    sub.size = tok::kTextSm;
    sub.color = selected ? tok::kInverseFg : tok::kFgMuted;

    const int16_t pad = tok::kSpaceMd;
    const bool two_line = item.subtitle.length() > 0;

    // Trailing metadata is measured first so the title can be ellipsized
    // against whatever space is actually left.
    int16_t trailing_w = 0;
    if (item.trailing.length() > 0) {
        trailing_w = (int16_t)(Text().MeasureWidth(item.trailing, sub) + pad);
        Text().Draw(*ctx.canvas, item.trailing,
                    (int16_t)(r.Right() - trailing_w + pad - pad),
                    (int16_t)(r.y + (r.h - Text().LineHeight(sub)) / 2), sub);
    }

    const int16_t title_max = (int16_t)(r.w - 2 * pad - trailing_w);
    const String title_text = Text().Ellipsize(item.title, title, title_max);

    if (two_line) {
        const int16_t block_h =
            (int16_t)(Text().LineHeight(title) + Text().LineHeight(sub));
        const int16_t y = (int16_t)(r.y + (r.h - block_h) / 2);

        Text().Draw(*ctx.canvas, title_text, (int16_t)(r.x + pad), y, title);
        Text().Draw(*ctx.canvas, Text().Ellipsize(item.subtitle, sub, title_max),
                    (int16_t)(r.x + pad),
                    (int16_t)(y + Text().LineHeight(title)), sub);
    } else {
        Text().Draw(*ctx.canvas, title_text, (int16_t)(r.x + pad),
                    (int16_t)(r.y + (r.h - Text().LineHeight(title)) / 2), title);
    }
}

void VirtualList::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr || !_count || !_item) return;

    const Rect box = ContentRect();
    const Rect clip = ctx.ToCanvas(box);
    if (clip.IsEmpty()) return;

    ctx.canvas->fillRect(clip.x, clip.y, clip.w, clip.h, tok::kSurface);

    const uint32_t total = _count();
    _cached_count = total;
    _count_valid = true;
    if (total == 0) return;

    // The window: only these rows are asked for and drawn.
    int32_t first = (int32_t)(_offset / _row_h) - _overscan;
    if (first < 0) first = 0;

    int32_t last = (int32_t)((_offset + box.h) / _row_h) + _overscan;
    if (last >= (int32_t)total) last = (int32_t)total - 1;

    for (int32_t i = first; i <= last; ++i) {
        const Rect row{box.x, (int16_t)(box.y + i * _row_h - _offset), box.w,
                       _row_h};
        if (!row.Intersects(box)) continue;
        DrawRow(ctx, _item((uint32_t)i), row, i == _selected);
    }

    // 2px right-edge scrollbar (issue #40), drawn only when it means something.
    const int16_t content_h = ContentHeight();
    if (content_h > box.h) {
        int16_t thumb_h = (int16_t)(((int32_t)box.h * box.h) / content_h);
        if (thumb_h < 24) thumb_h = 24;
        const int16_t max_offset = (int16_t)(content_h - box.h);
        const int16_t thumb_y =
            (int16_t)(clip.y + ((int32_t)(box.h - thumb_h) * _offset) / max_offset);
        ctx.canvas->fillRect((int16_t)(clip.Right() - tok::kScrollbarWidth),
                             thumb_y, tok::kScrollbarWidth, thumb_h, tok::kFg);
    }
}

bool VirtualList::HandleEvent(const InputEvent& e) {
    if (!_visible || !_enabled || !_count) return false;

    const int16_t viewport = ContentRect().h;
    const int16_t content_h = ContentHeight();
    const int16_t max_offset =
        content_h > viewport ? (int16_t)(content_h - viewport) : 0;

    auto scroll_by = [&](int16_t delta) {
        const int16_t before = _offset;
        _offset = (int16_t)(_offset + delta);
        if (_offset < 0) _offset = 0;
        if (_offset > max_offset) _offset = max_offset;
        if (_offset != before) Invalidate();
    };

    if (e.IsKey() && e.kind != InputKind::KeyUp) {
        switch (e.keycode) {
            case key::kPageDown: scroll_by(viewport); return true;
            case key::kPageUp:   scroll_by((int16_t)-viewport); return true;
            case key::kHome:     scroll_by((int16_t)-content_h); return true;
            case key::kEnd:      scroll_by(content_h); return true;
            case key::kDown:
                SetSelectedIndex(_selected + 1 < (int32_t)_count() ? _selected + 1 : _selected);
                return true;
            case key::kUp:
                SetSelectedIndex(_selected > 0 ? _selected - 1 : 0);
                return true;
            case key::kEnter:
                if (_selected >= 0 && _on_select) _on_select((uint32_t)_selected);
                return true;
            default: break;
        }
        return false;
    }

    if (!e.IsPointer()) return false;
    if (!_frame.Contains(e.x, e.y)) return false;

    switch (e.kind) {
        case InputKind::PointerMove:
            _dragging = true;
            scroll_by((int16_t)-e.dy); // 1:1 with the finger
            return true;

        case InputKind::Fling:
            scroll_by((int16_t)(-e.dz / 4));
            _dragging = false;
            return true;

        case InputKind::PointerUp:
            _dragging = false;
            return true;

        case InputKind::Tap: {
            // A tap that ended a drag is a scroll, not a selection.
            if (_dragging) return true;
            const int32_t idx = IndexAt(e.y);
            if (idx < 0) return true;
            SetSelectedIndex(idx);
            if (_on_select) _on_select((uint32_t)idx);
            return true;
        }

        case InputKind::LongPress: {
            const int32_t idx = IndexAt(e.y);
            if (idx >= 0 && _on_long_press) _on_long_press((uint32_t)idx);
            return true;
        }

        default:
            return true;
    }
}

}  // namespace m5ui
