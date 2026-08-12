#include "render_screen.h"

using namespace m5ui;

namespace demo {

namespace {

constexpr uint32_t kCounterPeriodMs = 1000;

// Ghost-debt heatmap: one cell per tile, darker where more residue has
// accumulated. This is the debt ledger made visible.
class GhostDebtMap : public Widget {
   public:
    GhostDebtMap() : Widget("ghostmap") {}

    Size Measure(const Constraints& c) override {
        // Preserve the panel's aspect ratio so the map reads as the screen.
        const int16_t cell = (int16_t)(c.max_w / kTilesX);
        return c.Clamp(Size{(int16_t)(cell * kTilesX), (int16_t)(cell * kTilesY)});
    }

    void DrawSelf(PaintContext& ctx) override {
        if (ctx.canvas == nullptr) return;
        const Rect r = ctx.ToCanvas(Frame());
        if (r.IsEmpty()) return;

        Display& panel = Device::Get().Panel();
        const int16_t cw = (int16_t)(r.w / kTilesX);
        const int16_t ch = (int16_t)(r.h / kTilesY);

        for (int16_t ty = 0; ty < kTilesY; ++ty) {
            for (int16_t tx = 0; tx < kTilesX; ++tx) {
                const uint8_t debt = panel.GhostDebtAt(tx, ty);
                // Map debt onto the grey ramp: clean tiles white, tiles at the
                // limit black.
                const uint8_t ratio =
                    debt >= Display::kGhostDebtLimit
                        ? 0
                        : (uint8_t)(kWhite - (debt * kWhite) / Display::kGhostDebtLimit);

                const int16_t x = (int16_t)(r.x + tx * cw);
                const int16_t y = (int16_t)(r.y + ty * ch);
                ctx.canvas->fillRect(x, y, cw, ch, ratio);
                ctx.canvas->drawRect(x, y, cw, ch, tok::kBorder);
            }
        }
    }
};

}  // namespace

RenderScreen::RenderScreen() : DemoScreen("render", "Rendering") {}

void RenderScreen::BuildContent(Column& column) {
    // ---- counter: exercises the fast-text path ---------------------------
    Card* fast = new Card();
    column.Add(fast);
    fast->Add(new Heading("Fast-text path", 3));

    Label* note = new Label(
        "This counter updates once a second through FlushTextRegion(), which "
        "pushes only its own rect with DU4 rather than the whole dirty union.",
        tok::kTextXs);
    note->SetColor(tok::kFgMuted);
    fast->Add(note);

    _counter = new Label("0", tok::kText2Xl);
    _counter->SetAlign(TextAlign::Center);
    fast->Add(_counter);

    // ---- policy counters --------------------------------------------------
    Card* stats = new Card();
    column.Add(stats);
    stats->Add(new Heading("Policy engine", 3));

    _stats = new Label("", tok::kTextSm);
    stats->Add(_stats);

    Row* actions = new Row(tok::kSpaceSm);
    stats->Add(actions);

    m5ui::Button* full =
        new m5ui::Button("Full refresh", ButtonVariant::Outline);
    full->OnPress([]() { Device::Get().Panel().RefreshFull(); });
    actions->Add(full);

    m5ui::Button* ghost =
        new m5ui::Button("Clear ghosts", ButtonVariant::Outline);
    ghost->OnPress([]() { Device::Get().Panel().RefreshGhostOnly(); });
    actions->Add(ghost);

    // ---- debt heatmap -----------------------------------------------------
    Card* map = new Card();
    column.Add(map);
    map->Add(new Heading("Ghost debt by tile", 3));

    Label* legend = new Label(
        "White = clean, black = at the debt limit. Tiles that reach the limit "
        "force a GC16 pass on their next flush regardless of intent.",
        tok::kTextXs);
    legend->SetColor(tok::kFgMuted);
    map->Add(legend);

    map->Add(new GhostDebtMap());
}

void RenderScreen::Tick() {
    DemoScreen::Tick();

    const uint32_t now = millis();
    if (now - _last_tick_ms < kCounterPeriodMs) return;
    _last_tick_ms = now;

    _counter_value++;
    _counter->SetText(String(_counter_value));

    Display& panel = Device::Get().Panel();
    _stats->SetText("flushes " + String(panel.FlushCount()) +
                    "  |  full refreshes " + String(panel.FullRefreshCount()) +
                    "  |  total debt " + String(panel.TotalGhostDebt()) +
                    "\ndropped input events " +
                    String(Device::Get().Input().DroppedCount()));

    // Push just the counter, not the whole screen -- this is the point of the
    // fast-text path.
    panel.FlushTextRegion(_counter->Frame());
}

}  // namespace demo
