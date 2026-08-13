#pragma once
// Image (issue #113): a widget that paints a JPEG byte buffer full-frame.
//
// The buffer is caller-owned (flash-resident C array, typically) -- the
// widget never copies or frees it. Swapping SetSource() to a different
// buffer/size is how a gallery advances between images; each swap
// invalidates so the next Paint() decodes the new picture.

#include "../core/widget.h"

namespace m5ui {

class Image : public Widget {
   public:
    Image();

    // data must outlive the widget (or the next SetSource call). w/h are the
    // decoded image's natural size in px; the widget draws at its frame
    // origin and lets M5EPD's decoder downscale to fit if the frame is
    // smaller.
    void SetSource(const uint8_t* jpg_data, size_t jpg_len, int16_t w,
                   int16_t h);

    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;

   private:
    const uint8_t* _data = nullptr;
    size_t _len = 0;
    int16_t _w = 0;
    int16_t _h = 0;
};

}  // namespace m5ui
