#pragma once
// Auto-generated placeholder asset -- swap for the real business
// card JPEG and regenerate (issue #113). PROGMEM keeps it in flash
// rather than internal DRAM, which is scarce (see project CLAUDE.md).
#include <pgmspace.h>
#include <stdint.h>
#include <stddef.h>

namespace card {

constexpr int16_t kCardFrontW = 540;
constexpr int16_t kCardFrontH = 960;
constexpr size_t kCardFrontLen = 43836;
extern const uint8_t kCardFrontJpg[kCardFrontLen] PROGMEM;

}  // namespace card
