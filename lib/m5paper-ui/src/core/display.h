#pragma once
// Display wrapper: shadow framebuffer, dirty-region tracking, ghost-debt
// accounting and the update-mode policy engine (issues #34-#38).
//
// The panel has no free lunch: fast update modes (DU/A2/DU4) leave residue,
// and the clean mode (GC16) takes ~600 ms and flashes. The policy engine picks
// a mode per flush from what actually changed and how much residue each tile
// has accumulated, so callers say "draw" rather than naming a waveform.

#include <M5EPD.h>
#include <stdint.h>

#include "geometry.h"

namespace m5ui {

// Rendering intent, declared by the caller. The policy engine maps this plus
// accumulated ghost debt onto a concrete m5epd_update_mode_t.
enum class DrawIntent : uint8_t {
    Static,   // page content, legibility matters -- prefers GC16
    Text,     // small text region updating often -- prefers DU4/A2
    Animated, // dragging, scrolling, caret blink -- prefers A2, accepts residue
    Clean     // force a full clean flush now
};

// Tile grid for ghost accounting. 540x960 at 60px = 9x16 = 144 tiles, one
// byte each.
constexpr int16_t kTileSize = 60;
constexpr int16_t kTilesX = kDisplayW / kTileSize;
constexpr int16_t kTilesY = kDisplayH / kTileSize;

class Display {
   public:
    // Debt a tile may accumulate before the policy engine forces a clean pass.
    static constexpr uint8_t kGhostDebtLimit = 12;

    void Begin();

    // ------------------------------------------------------------ canvas --
    // The shadow framebuffer. Widgets draw into this; nothing reaches the
    // panel until Flush().
    M5EPD_Canvas& Canvas() {
        return _canvas;
    }

    // ------------------------------------------------------------- dirty --
    void Invalidate(const Rect& r);
    void InvalidateAll();
    bool HasDirty() const {
        return !_dirty.IsEmpty();
    }
    Rect DirtyRect() const {
        return _dirty;
    }

    // ------------------------------------------------------------ flush ---
    // Pushes the dirty region with a mode chosen from `intent` and ghost debt.
    // No-op when nothing is dirty.
    void Flush(DrawIntent intent = DrawIntent::Static);

    // Public escape hatches (issue #37).
    void RefreshFull();      // clean flush of the whole panel, clears all debt
    void RefreshGhostOnly(); // clean flush of tiles over the debt threshold

    // Fast path for a small, rapidly-updating region -- a clock, a counter, a
    // caret (issue #38). Bypasses the dirty union so an unrelated pending
    // region is not dragged along.
    void FlushTextRegion(const Rect& r);

    // ------------------------------------------------------------ debug ---
    uint8_t GhostDebtAt(int16_t tx, int16_t ty) const;
    uint16_t TotalGhostDebt() const;
    uint32_t FlushCount() const {
        return _flush_count;
    }
    uint32_t FullRefreshCount() const {
        return _full_refresh_count;
    }

    // Blocks until the panel controller reports idle. Called before deep sleep
    // or power-down so a partially-driven frame is not left on the glass.
    void WaitIdle();

   private:
    // Pushes one sub-rectangle of the shadow canvas to the panel.
    //
    // UNVERIFIED against the installed M5EPD library (gated on issue #1).
    // M5EPD_Canvas::pushCanvas() only takes an origin, so a partial push has to
    // go through the driver's WritePartGram4bpp + UpdateArea pair. If the real
    // header disagrees, this is the one function to fix -- every other flush
    // path routes through it.
    void PushRegion(const Rect& area, m5epd_update_mode_t mode);

    m5epd_update_mode_t ChoosePolicy(const Rect& area, DrawIntent intent);
    void AddDebt(const Rect& area, uint8_t amount);
    void ClearDebt(const Rect& area);
    bool AreaExceedsDebt(const Rect& area) const;

    M5EPD_Canvas _canvas{&M5.EPD};
    Rect _dirty;
    uint8_t _debt[kTilesY][kTilesX] = {{0}};
    uint32_t _flush_count = 0;
    uint32_t _full_refresh_count = 0;
    bool _begun = false;
};

}  // namespace m5ui
