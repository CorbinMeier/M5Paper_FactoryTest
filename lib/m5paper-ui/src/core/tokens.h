#pragma once
// Design tokens (issue #32). Spacing, grey levels and the type scale, in one
// place, so components stop hardcoding pixel constants.
//
// Greys are chosen for e-ink legibility rather than a linear ramp: the panel
// renders mid-greys poorly under DU updates, so the palette stays near the
// ends of the range.

#include <stdint.h>

namespace m5ui {
namespace tok {

// -------------------------------------------------------------- spacing ----
// A 4px base grid. Named sizes, not raw numbers, at call sites.

constexpr int16_t kSpace0 = 0;
constexpr int16_t kSpaceXs = 4;
constexpr int16_t kSpaceSm = 8;
constexpr int16_t kSpaceMd = 16;
constexpr int16_t kSpaceLg = 24;
constexpr int16_t kSpaceXl = 32;
constexpr int16_t kSpace2Xl = 48;

// ----------------------------------------------------------------- grey ----

constexpr uint8_t kFg = 0;         // primary text
constexpr uint8_t kFgMuted = 5;    // secondary text, timestamps
constexpr uint8_t kFgDisabled = 9; // disabled labels
constexpr uint8_t kBorder = 8;     // hairlines, dividers
constexpr uint8_t kBorderStrong = 4;
constexpr uint8_t kSurfaceSunken = 12; // input wells, pressed states
constexpr uint8_t kSurface = 15;       // page background
constexpr uint8_t kInverseFg = 15;     // text on a dark fill
constexpr uint8_t kInverseBg = 0;      // selected / pressed fill

// ----------------------------------------------------------------- type ----
// Point sizes handed to the TTF renderer.

constexpr uint8_t kTextXs = 16;
constexpr uint8_t kTextSm = 20;
constexpr uint8_t kTextMd = 24; // body default
constexpr uint8_t kTextLg = 28;
constexpr uint8_t kTextXl = 36;
constexpr uint8_t kText2Xl = 48;

constexpr uint8_t kH1 = kText2Xl;
constexpr uint8_t kH2 = kTextXl;
constexpr uint8_t kH3 = kTextLg;

// Multiplied by the font size to get baseline-to-baseline distance.
constexpr float kLineHeight = 1.35f;

// ---------------------------------------------------------------- shape ----

constexpr int16_t kRadiusSm = 4;
constexpr int16_t kRadiusMd = 8;
constexpr int16_t kRadiusLg = 16;
constexpr int16_t kBorderWidth = 2;
constexpr int16_t kFocusRingWidth = 3;
constexpr int16_t kScrollbarWidth = 2; // issue #40

// ---------------------------------------------------------------- sizes ----

constexpr int16_t kTouchTargetMin = 48; // never render a tappable box smaller
constexpr int16_t kStatusBarH = 44;
constexpr int16_t kAppBarH = 64;
constexpr int16_t kListRowH = 72;
constexpr int16_t kControlH = 56;

// --------------------------------------------------------------- timing ----

constexpr uint32_t kTapMaxMs = 400;        // press+release inside this = tap
constexpr uint32_t kLongPressMs = 600;
constexpr uint32_t kDragSlopPx = 8;        // movement before a tap becomes a drag
constexpr uint32_t kStatusPollMs = 10000;  // status bar refresh cadence
// Holding the Push side button (G38) this long opens the system menu
// (issue #115). Long relative to kLongPressMs -- this is a deliberate,
// device-wide gesture, not a per-widget interaction.
constexpr uint32_t kSystemMenuHoldMs = 5000;

}  // namespace tok
}  // namespace m5ui
