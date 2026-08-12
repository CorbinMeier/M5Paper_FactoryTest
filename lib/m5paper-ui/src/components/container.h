#pragma once
// Container, Column, Row, Card, Divider, Spacer (issues #45, #46).
//
// Thin wrappers over the layout solvers. They exist so a screen's Build() reads
// as structure rather than as arithmetic on y offsets.

#include "../core/widget.h"

namespace m5ui {

// Lays children out along one axis.
class Container : public Widget {
   public:
    explicit Container(Axis axis = Axis::Vertical);

    Container* SetAxis(Axis a);
    Container* SetGap(int16_t gap);
    Container* SetJustify(Justify j);
    Container* SetCrossAlign(Align a);
    Container* SetBackground(uint8_t grey); // 255 = transparent
    Container* SetBorder(uint8_t grey, int16_t width = tok::kBorderWidth);
    Container* SetRadius(int16_t radius);

    Size Measure(const Constraints& c) override;
    void Layout(const Rect& bounds) override;
    void DrawSelf(PaintContext& ctx) override;

   protected:
    Axis _axis = Axis::Vertical;
    int16_t _gap = tok::kSpaceSm;
    Justify _justify = Justify::Start;
    Align _cross_align = Align::Stretch;
    uint8_t _background = 255;
    uint8_t _border_color = 255;
    int16_t _border_width = 0;
    int16_t _radius = 0;
};

// Sugar, so screens read declaratively.
class Column : public Container {
   public:
    explicit Column(int16_t gap = tok::kSpaceSm);
};

class Row : public Container {
   public:
    explicit Row(int16_t gap = tok::kSpaceSm);
};

// A surfaced, bordered container with standard padding.
class Card : public Container {
   public:
    explicit Card(Axis axis = Axis::Vertical);
};

// A hairline rule. Sizes to 1px on the cross axis.
class Divider : public Widget {
   public:
    explicit Divider(Axis axis = Axis::Horizontal);
    Size Measure(const Constraints& c) override;
    void DrawSelf(PaintContext& ctx) override;

   private:
    Axis _axis;
};

// Fixed or flexible empty space.
class Spacer : public Widget {
   public:
    explicit Spacer(int16_t size = 0); // 0 = flex:1, absorbing leftover space
    Size Measure(const Constraints& c) override;

   private:
    int16_t _size;
};

}  // namespace m5ui
