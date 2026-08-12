#pragma once
// VirtualList -- windowed rendering for long lists (issue #42).
//
// The list never materialises a widget per item. It asks the data source for
// the rows currently on screen, plus a small overscan, and draws those. A
// 10,000-note directory costs the same as a 10-note one.
//
// Rows are drawn directly rather than composed from child widgets: one widget
// per visible row would still be ~15 allocations per scroll on this hardware.

#include <Arduino.h>

#include <functional>

#include "../core/widget.h"
#include "label.h"

namespace m5ui {

struct ListItem {
    String title;
    String subtitle;
    String trailing; // right-aligned metadata: size, time, count
    bool selected = false;
    bool disabled = false;
};

class VirtualList : public Widget {
   public:
    // Pull-based, so the caller keeps ownership of the data and the list never
    // copies more than one screenful.
    using CountProvider = std::function<uint32_t()>;
    using ItemProvider = std::function<ListItem(uint32_t index)>;
    using SelectHandler = std::function<void(uint32_t index)>;

    VirtualList();

    void SetDataSource(CountProvider count, ItemProvider item);
    void OnSelect(SelectHandler handler) {
        _on_select = std::move(handler);
    }
    // Long-press a row -- the usual home for delete/rename.
    void OnItemLongPress(SelectHandler handler) {
        _on_long_press = std::move(handler);
    }

    void SetRowHeight(int16_t h);
    int16_t RowHeight() const {
        return _row_h;
    }
    // Rows rendered above and below the viewport, so a fast drag does not
    // reveal blank space before the next paint.
    void SetOverscan(uint8_t rows);

    // Data changed underneath us -- drop cached counts and repaint.
    void Reload();

    void ScrollToIndex(uint32_t index);
    int32_t SelectedIndex() const {
        return _selected;
    }
    void SetSelectedIndex(int32_t index);

    int16_t ContentHeight() const;

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;
    bool HandleEvent(const InputEvent& e) override;
    bool IsFocusable() const override {
        return IsEnabled() && IsVisible();
    }

   private:
    void DrawRow(PaintContext& ctx, const ListItem& item, const Rect& row,
                 bool selected);
    int32_t IndexAt(int16_t y) const;

    CountProvider _count;
    ItemProvider _item;
    SelectHandler _on_select;
    SelectHandler _on_long_press;

    int16_t _row_h = tok::kListRowH;
    uint8_t _overscan = 1;
    int32_t _selected = -1;
    uint32_t _cached_count = 0;
    bool _count_valid = false;

    // Scroll offset in content pixels. The list scrolls itself rather than
    // living inside a ScrollView -- it must know which rows to ask for.
    int16_t _offset = 0;
    bool _dragging = false;
};

}  // namespace m5ui
