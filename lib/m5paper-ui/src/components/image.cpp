#include "image.h"

namespace m5ui {

Image::Image() : Widget("image") {}

void Image::SetSource(const uint8_t* jpg_data, size_t jpg_len, int16_t w,
                      int16_t h) {
    _data = jpg_data;
    _len = jpg_len;
    _w = w;
    _h = h;
    Invalidate();
}

Size Image::Measure(const Constraints& c) {
    if (_w <= 0 || _h <= 0) return c.Clamp(Size{c.max_w, c.max_h});
    return c.Clamp(Size{_w, _h});
}

void Image::DrawSelf(PaintContext& ctx) {
    if (ctx.canvas == nullptr || _data == nullptr || _len == 0) return;

    const Rect r = ctx.ToCanvas(_frame);
    if (r.IsEmpty()) return;

    // maxWidth/maxHeight let the decoder downscale to fit the frame rather
    // than the widget having to pre-scale the source JPEG.
    ctx.canvas->drawJpg(_data, _len, r.x, r.y, r.w, r.h);
}

}  // namespace m5ui
