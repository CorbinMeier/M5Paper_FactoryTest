#include "display.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "tokens.h"

namespace m5ui {

namespace {

// Debt charged per flush, by mode. GC16 leaves nothing behind; A2 is the
// dirtiest. These are ratios, not measurements -- calibrate on glass.
uint8_t DebtFor(m5epd_update_mode_t mode) {
    switch (mode) {
        case UPDATE_MODE_GC16: return 0;
        case UPDATE_MODE_GL16: return 1;
        case UPDATE_MODE_DU4:  return 2;
        case UPDATE_MODE_DU:   return 3;
        case UPDATE_MODE_A2:   return 4;
        default:               return 2;
    }
}

}  // namespace

void Display::Begin() {
    if (_begun) return;

    M5.EPD.SetRotation(M5EPD_Driver::ROTATE_90);
    M5.EPD.Clear(true);

    // 259 KB -- necessarily PSRAM. createCanvas falls back to PSRAM when the
    // internal heap cannot serve it, which is why the firmware boots at all.
    _canvas.createCanvas(kDisplayW, kDisplayH);
    _canvas.fillCanvas(tok::kSurface);

    _begun = true;
    InvalidateAll();
}

// ------------------------------------------------------------- dirty -------

void Display::Invalidate(const Rect& r) {
    const Rect clipped = r.Clipped();
    if (clipped.IsEmpty()) return;
    _dirty = _dirty.Union(clipped);
}

void Display::InvalidateAll() {
    _dirty = Rect::FullScreen();
}

// -------------------------------------------------------- ghost debt -------

void Display::AddDebt(const Rect& area, uint8_t amount) {
    if (amount == 0) return;
    const Rect a = area.AlignedOut(kTileSize).Clipped();

    for (int16_t ty = a.y / kTileSize; ty < (a.Bottom() + kTileSize - 1) / kTileSize; ++ty) {
        if (ty < 0 || ty >= kTilesY) continue;
        for (int16_t tx = a.x / kTileSize; tx < (a.Right() + kTileSize - 1) / kTileSize; ++tx) {
            if (tx < 0 || tx >= kTilesX) continue;
            const uint16_t next = (uint16_t)_debt[ty][tx] + amount;
            _debt[ty][tx] = next > 255 ? 255 : (uint8_t)next;
        }
    }
}

void Display::ClearDebt(const Rect& area) {
    const Rect a = area.AlignedOut(kTileSize).Clipped();
    for (int16_t ty = a.y / kTileSize; ty < (a.Bottom() + kTileSize - 1) / kTileSize; ++ty) {
        if (ty < 0 || ty >= kTilesY) continue;
        for (int16_t tx = a.x / kTileSize; tx < (a.Right() + kTileSize - 1) / kTileSize; ++tx) {
            if (tx < 0 || tx >= kTilesX) continue;
            _debt[ty][tx] = 0;
        }
    }
}

bool Display::AreaExceedsDebt(const Rect& area) const {
    const Rect a = area.AlignedOut(kTileSize).Clipped();
    for (int16_t ty = a.y / kTileSize; ty < (a.Bottom() + kTileSize - 1) / kTileSize; ++ty) {
        if (ty < 0 || ty >= kTilesY) continue;
        for (int16_t tx = a.x / kTileSize; tx < (a.Right() + kTileSize - 1) / kTileSize; ++tx) {
            if (tx < 0 || tx >= kTilesX) continue;
            if (_debt[ty][tx] >= kGhostDebtLimit) return true;
        }
    }
    return false;
}

uint8_t Display::GhostDebtAt(int16_t tx, int16_t ty) const {
    if (tx < 0 || tx >= kTilesX || ty < 0 || ty >= kTilesY) return 0;
    return _debt[ty][tx];
}

uint16_t Display::TotalGhostDebt() const {
    uint16_t sum = 0;
    for (int16_t ty = 0; ty < kTilesY; ++ty) {
        for (int16_t tx = 0; tx < kTilesX; ++tx) sum += _debt[ty][tx];
    }
    return sum;
}

// -------------------------------------------------------------- push -------

void Display::PushRegion(const Rect& area, m5epd_update_mode_t mode) {
    if (area.IsEmpty()) return;

    // Wait for any in-flight update BEFORE touching controller memory. The
    // gram writes below have no interlock of their own, and UpdateArea's
    // internal CheckAFSR() comes too late -- by then the frame buffer the
    // IT8951 is actively driving has already been overwritten, which strands
    // the panel mid-waveform showing the refresh flash (issue #105).
    WaitIdle();

    // Whole panel: the canvas can push itself, which is both simpler and what
    // the driver is optimised for.
    if (area.w >= kDisplayW && area.h >= kDisplayH) {
        _canvas.pushCanvas(0, 0, mode);
        return;
    }

    // Partial: hand the driver the sub-rectangle out of the shadow canvas.
    // 4bpp packs two pixels per byte, so x and width must be even -- Flush()
    // already aligns to 4.
    uint8_t* base = (uint8_t*)_canvas.frameBuffer(1);
    if (base == nullptr) {
        _canvas.pushCanvas(0, 0, mode);
        return;
    }

    const uint32_t stride = (uint32_t)kDisplayW / 2;

    // WritePartGram4bpp takes no stride: it reads w*h/2 contiguous bytes as a
    // standalone sub-image. Handing it a pointer into the full-width canvas
    // makes it walk across canvas rows instead of region rows (issue #106), so
    // the region has to be copied out packed first.
    const uint8_t* packed = PackRegion(base, area, stride);
    if (packed == nullptr) {
        // Out of PSRAM. A whole-canvas push is slower but correct, and losing
        // the frame outright would be worse.
        _canvas.pushCanvas(0, 0, mode);
        return;
    }

    M5.EPD.WritePartGram4bpp(area.x, area.y, area.w, area.h, packed);
    M5.EPD.UpdateArea(area.x, area.y, area.w, area.h, mode);
}

uint8_t* Display::PackRegion(const uint8_t* base, const Rect& area,
                             uint32_t stride) {
    // 4bpp: two pixels per byte. Flush() aligns x and width to 4, so these
    // divisions are exact and rows stay byte-aligned.
    const uint32_t row_bytes = (uint32_t)area.w / 2;
    const uint32_t needed = row_bytes * (uint32_t)area.h;

    if (needed > _pack_capacity) {
        uint8_t* grown =
            (uint8_t*)heap_caps_realloc(_pack, needed, MALLOC_CAP_SPIRAM);
        if (grown == nullptr) return nullptr;
        _pack = grown;
        _pack_capacity = needed;
    }

    const uint8_t* src = base + (uint32_t)area.y * stride + (uint32_t)area.x / 2;
    for (int16_t row = 0; row < area.h; ++row) {
        memcpy(_pack + (uint32_t)row * row_bytes, src, row_bytes);
        src += stride;
    }
    return _pack;
}

// ------------------------------------------------------------ policy -------

m5epd_update_mode_t Display::ChoosePolicy(const Rect& area, DrawIntent intent) {
    if (intent == DrawIntent::Clean) return UPDATE_MODE_GC16;

    // Accumulated residue overrides the caller's preference -- this is the
    // whole point of the debt ledger.
    if (AreaExceedsDebt(area)) return UPDATE_MODE_GC16;

    // A region covering most of the panel is not worth a partial waveform;
    // the flash is coming either way, so take the clean one.
    const uint32_t area_px = (uint32_t)area.w * (uint32_t)area.h;
    const uint32_t screen_px = (uint32_t)kDisplayW * (uint32_t)kDisplayH;
    if (area_px > (screen_px * 3) / 4) return UPDATE_MODE_GC16;

    switch (intent) {
        case DrawIntent::Animated: return UPDATE_MODE_A2;
        case DrawIntent::Text:     return UPDATE_MODE_DU4;
        case DrawIntent::Static:
        default:                   return UPDATE_MODE_GL16;
    }
}

// ------------------------------------------------------------- flush -------

void Display::Flush(DrawIntent intent) {
    if (!_begun || _dirty.IsEmpty()) return;

    const Rect area = _dirty.AlignedOut(4).Clipped(); // 4px keeps 4bpp aligned
    const m5epd_update_mode_t mode = ChoosePolicy(area, intent);

    PushRegion(area, mode);

    if (mode == UPDATE_MODE_GC16) {
        ClearDebt(area);
    } else {
        AddDebt(area, DebtFor(mode));
    }

    _dirty = Rect{};
    _flush_count++;
}

void Display::FlushTextRegion(const Rect& r) {
    if (!_begun) return;
    const Rect area = r.AlignedOut(4).Clipped();
    if (area.IsEmpty()) return;

    const m5epd_update_mode_t mode =
        AreaExceedsDebt(area) ? UPDATE_MODE_GC16 : UPDATE_MODE_DU4;
    PushRegion(area, mode);

    if (mode == UPDATE_MODE_GC16) {
        ClearDebt(area);
    } else {
        AddDebt(area, DebtFor(mode));
    }
    _flush_count++;

    // The region was pushed independently -- drop it from any pending union so
    // the next Flush does not repaint it.
    if (_dirty.Intersects(area) && area.Union(_dirty).w == area.w &&
        area.Union(_dirty).h == area.h) {
        _dirty = Rect{};
    }
}

void Display::RefreshFull() {
    if (!_begun) return;
    // pushCanvas() writes full gram with no interlock either, so it needs the
    // same guard PushRegion() has (issue #105).
    WaitIdle();
    _canvas.pushCanvas(0, 0, UPDATE_MODE_GC16);
    ClearDebt(Rect::FullScreen());
    _dirty = Rect{};
    _flush_count++;
    _full_refresh_count++;
}

void Display::RefreshGhostOnly() {
    if (!_begun) return;

    // Union of every tile over the limit, so one push clears them together --
    // several small GC16 pushes flash more than one larger one.
    Rect area;
    for (int16_t ty = 0; ty < kTilesY; ++ty) {
        for (int16_t tx = 0; tx < kTilesX; ++tx) {
            if (_debt[ty][tx] < kGhostDebtLimit) continue;
            area = area.Union(Rect{(int16_t)(tx * kTileSize),
                                   (int16_t)(ty * kTileSize), kTileSize,
                                   kTileSize});
        }
    }
    if (area.IsEmpty()) return;

    PushRegion(area, UPDATE_MODE_GC16);
    ClearDebt(area);
    _flush_count++;
}

void Display::WaitIdle() {
    // Blocks until the panel controller reports its update FSM is idle. Call
    // before deep sleep or power-down so a half-driven frame is not left on
    // the glass.
    M5.EPD.CheckAFSR();
}

}  // namespace m5ui
