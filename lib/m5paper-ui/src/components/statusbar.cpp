#include "statusbar.h"

#include "../core/app.h"
#include "../core/device.h"
#include "../core/screen.h"

namespace m5ui {

namespace {
constexpr int16_t kGaugeW = 34;
constexpr int16_t kGaugeH = 18;
constexpr int16_t kGaugeNubW = 3;
}  // namespace

// ------------------------------------------------------------ StatusBar ----

StatusBar::StatusBar() : Widget("statusbar") {
    SetPadding(EdgeInsets::Symmetric(0, tok::kSpaceSm));
    Refresh();
}

void StatusBar::SetTitle(const String& title) {
    if (_title == title) return;
    _title = title;
    Invalidate();
}

void StatusBar::SetShowClock(bool show) {
    _show_clock = show;
    Invalidate();
}

void StatusBar::SetShowBattery(bool show) {
    _show_battery = show;
    Invalidate();
}

void StatusBar::SetShowMemory(bool show) {
    _show_memory = show;
    Invalidate();
}

void StatusBar::SetWifiConnected(bool connected) {
    if (_wifi == connected) return;
    _wifi = connected;
    Invalidate();
}

void StatusBar::SetBleConnected(bool connected) {
    if (_ble == connected) return;
    _ble = connected;
    Invalidate();
}

void StatusBar::Refresh() {
    const uint32_t now = millis();
    if (_last_refresh_ms != 0 && (now - _last_refresh_ms) < tok::kStatusPollMs) {
        return;
    }
    _last_refresh_ms = now;

    Device& dev = Device::Get();
    if (!dev.IsBegun()) return;

    bool changed = false;

    if (_show_clock) {
        const String t = dev.Time().TimeString();
        if (t != _clock_text) {
            _clock_text = t;
            changed = true;
        }
    }
    if (_show_battery) {
        const BatteryState b = dev.GetBattery();
        if (b.percent != _battery_percent || b.IsCharging() != _battery_charging) {
            _battery_percent = b.percent;
            _battery_charging = b.IsCharging();
            changed = true;
        }
    }
    if (_show_memory) {
        const uint32_t kb = dev.Memory().FreeInternal() / 1024;
        // Only repaint on a meaningful move -- a 1 KB jitter is not worth a
        // panel update.
        if (kb / 4 != _free_dram_kb / 4) {
            _free_dram_kb = kb;
            changed = true;
        }
    }

    if (changed) Invalidate();
}

Size StatusBar::Measure(const Constraints& c) {
    return Size{c.max_w, tok::kStatusBarH};
}

void StatusBar::DrawBatteryGauge(PaintContext& ctx, int16_t right_edge) {
    const Rect r = ctx.ToCanvas(_frame);
    const int16_t x = (int16_t)(right_edge - kGaugeW - kGaugeNubW);
    const int16_t y = (int16_t)(r.y + (r.h - kGaugeH) / 2);

    // Body, nub, then a fill proportional to charge.
    ctx.canvas->drawRoundRect(x, y, kGaugeW, kGaugeH, 2, tok::kFg);
    ctx.canvas->fillRect((int16_t)(x + kGaugeW), (int16_t)(y + kGaugeH / 4),
                         kGaugeNubW, (int16_t)(kGaugeH / 2), tok::kFg);

    const int16_t inner_w = (int16_t)(kGaugeW - 6);
    int16_t fill_w = (int16_t)(((int32_t)inner_w * _battery_percent) / 100);
    if (fill_w < 1 && _battery_percent > 0) fill_w = 1;
    if (fill_w > 0) {
        ctx.canvas->fillRect((int16_t)(x + 3), (int16_t)(y + 3), fill_w,
                             (int16_t)(kGaugeH - 6), tok::kFg);
    }

    if (_battery_charging) {
        // A bolt would need an asset; a caret in the middle reads clearly at
        // 18px and costs nothing.
        const int16_t cx = (int16_t)(x + kGaugeW / 2);
        ctx.canvas->fillTriangle(cx, (int16_t)(y + 2), (int16_t)(cx - 4),
                                 (int16_t)(y + kGaugeH / 2 + 1), (int16_t)(cx + 4),
                                 (int16_t)(y + kGaugeH / 2 + 1), tok::kInverseFg);
    }
}

void StatusBar::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    ctx.canvas->fillRect(r.x, r.y, r.w, r.h, tok::kSurface);
    ctx.canvas->drawFastHLine(r.x, (int16_t)(r.Bottom() - 1), r.w, tok::kBorder);

    TextStyle style;
    style.size = tok::kTextSm;
    style.color = tok::kFg;
    const int16_t text_y = (int16_t)(r.y + (r.h - Text().LineHeight(style)) / 2);

    // ---- left: title -----------------------------------------------------
    if (_title.length() > 0) {
        Text().Draw(*ctx.canvas, _title, (int16_t)(r.x + _padding.left), text_y,
                    style);
    }

    // ---- centre: clock ---------------------------------------------------
    if (_show_clock && _clock_text.length() > 0) {
        const int16_t w = Text().MeasureWidth(_clock_text, style);
        Text().Draw(*ctx.canvas, _clock_text, (int16_t)(r.CenterX() - w / 2),
                    text_y, style);
    }

    // ---- right: battery, memory, radios ----------------------------------
    int16_t cursor = (int16_t)(r.Right() - _padding.right);

    if (_show_battery) {
        DrawBatteryGauge(ctx, cursor);
        cursor = (int16_t)(cursor - kGaugeW - kGaugeNubW - tok::kSpaceXs);

        const String pct = String(_battery_percent) + "%";
        const int16_t w = Text().MeasureWidth(pct, style);
        cursor = (int16_t)(cursor - w);
        Text().Draw(*ctx.canvas, pct, cursor, text_y, style);
        cursor = (int16_t)(cursor - tok::kSpaceSm);
    }

    if (_show_memory) {
        // Free internal DRAM, not total free -- PSRAM headroom says nothing
        // about whether BLE can start (issue #77).
        const String mem = String(_free_dram_kb) + "K";
        TextStyle muted = style;
        muted.color = tok::kFgMuted;
        const int16_t w = Text().MeasureWidth(mem, muted);
        cursor = (int16_t)(cursor - w);
        Text().Draw(*ctx.canvas, mem, cursor, text_y, muted);
        cursor = (int16_t)(cursor - tok::kSpaceSm);
    }

    if (_ble) {
        const int16_t w = Text().MeasureWidth("BT", style);
        cursor = (int16_t)(cursor - w);
        Text().Draw(*ctx.canvas, "BT", cursor, text_y, style);
        cursor = (int16_t)(cursor - tok::kSpaceSm);
    }
    if (_wifi) {
        const int16_t w = Text().MeasureWidth("WiFi", style);
        cursor = (int16_t)(cursor - w);
        Text().Draw(*ctx.canvas, "WiFi", cursor, text_y, style);
    }
}

// --------------------------------------------------------------- AppBar ----

AppBar::AppBar(const String& title) : Widget("appbar"), _title(title) {
    SetPadding(EdgeInsets::Symmetric(tok::kSpaceSm, tok::kSpaceMd));
}

void AppBar::SetTitle(const String& title) {
    if (_title == title) return;
    _title = title;
    Invalidate();
}

void AppBar::SetBackAction(std::function<void()> handler) {
    if (_back != nullptr) return;

    _back = new Button("< Back", ButtonVariant::Ghost);
    _back->OnPress([handler]() {
        if (handler) {
            handler();
            return;
        }
        // Default: pop the current app stack.
        if (App* app = Device::Get().CurrentApp()) app->Pop();
    });
    Add(_back);
}

Button* AppBar::AddAction(const String& label, std::function<void()> handler) {
    Button* b = new Button(label, ButtonVariant::Ghost);
    b->OnPress(std::move(handler));
    Add(b);
    _actions.push_back(b);
    return b;
}

Size AppBar::Measure(const Constraints& c) {
    return Size{c.max_w, tok::kAppBarH};
}

void AppBar::Layout(const Rect& bounds) {
    SetFrame(bounds);
    const Rect box = ContentRect();

    if (_back != nullptr) {
        const Size s = _back->Measure(Constraints::Loose(box.w, box.h));
        _back->Layout(Rect{box.x, (int16_t)(box.y + (box.h - s.h) / 2), s.w, s.h});
    }

    // Actions pack from the right edge inward, in the order they were added.
    int16_t cursor = box.Right();
    for (auto it = _actions.rbegin(); it != _actions.rend(); ++it) {
        Button* b = *it;
        const Size s = b->Measure(Constraints::Loose(box.w, box.h));
        cursor = (int16_t)(cursor - s.w);
        b->Layout(Rect{cursor, (int16_t)(box.y + (box.h - s.h) / 2), s.w, s.h});
        cursor = (int16_t)(cursor - tok::kSpaceSm);
    }
}

void AppBar::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    ctx.canvas->fillRect(r.x, r.y, r.w, r.h, tok::kSurface);
    ctx.canvas->drawFastHLine(r.x, (int16_t)(r.Bottom() - 1), r.w, tok::kBorder);

    if (_title.length() == 0) return;

    TextStyle style;
    style.size = tok::kTextLg;
    style.bold = true;

    // Centred, but clamped so it cannot slide under the back button.
    const int16_t back_w = _back ? (int16_t)(_back->Frame().w + tok::kSpaceSm) : 0;
    const int16_t w = Text().MeasureWidth(_title, style);
    int16_t x = (int16_t)(r.CenterX() - w / 2);
    if (x < r.x + back_w) x = (int16_t)(r.x + back_w);

    Text().Draw(*ctx.canvas, _title, x,
                (int16_t)(r.y + (r.h - Text().LineHeight(style)) / 2), style);
}

}  // namespace m5ui
