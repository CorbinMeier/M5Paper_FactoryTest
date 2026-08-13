#pragma once
// Auto-generated placeholder asset -- swap for the real business
// card JPEG and regenerate (issue #113). PROGMEM keeps it in flash
// rather than internal DRAM, which is scarce (see project CLAUDE.md).
#include <pgmspace.h>
#include <stdint.h>
#include <stddef.h>

namespace card {

constexpr int16_t kCardBackW = 540;
constexpr int16_t kCardBackH = 960;
constexpr size_t kCardBackLen = 24183;
extern const uint8_t kCardBackJpg[kCardBackLen] PROGMEM;

}  // namespace card
