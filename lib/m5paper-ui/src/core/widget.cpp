#include "widget.h"

#include <algorithm>

namespace m5ui {

Widget::Widget(const char* type_name) : _type_name(type_name) {}

Widget::~Widget() {
    for (Widget* c : _children) delete c;
    _children.clear();
}

// ---------------------------------------------------------------- tree ----

Widget* Widget::Add(Widget* child) {
    if (child == nullptr) return nullptr;
    child->_parent = this;
    _children.push_back(child);
    Invalidate();
    return child;
}

void Widget::Remove(Widget* child) {
    auto it = std::find(_children.begin(), _children.end(), child);
    if (it == _children.end()) return;
    (*it)->_parent = nullptr;
    delete *it;
    _children.erase(it);
    Invalidate();
}

void Widget::ClearChildren() {
    for (Widget* c : _children) delete c;
    _children.clear();
    Invalidate();
}

Widget* Widget::FindById(uint32_t id) {
    if (_id == id) return this;
    for (Widget* c : _children) {
        if (Widget* found = c->FindById(id)) return found;
    }
    return nullptr;
}

// ------------------------------------------------------------ geometry ----

void Widget::SetFrame(const Rect& r) {
    if (r.x == _frame.x && r.y == _frame.y && r.w == _frame.w &&
        r.h == _frame.h) {
        return;
    }
    // Both the old and new positions need repainting.
    InvalidateRect(_frame);
    _frame = r;
    Invalidate();
}

Size Widget::Measure(const Constraints& c) {
    if (_params.preferred.w > 0 || _params.preferred.h > 0) {
        return c.Clamp(_params.preferred);
    }
    // No intrinsic size -- fill what we are given.
    return Size{c.max_w, c.max_h};
}

void Widget::Layout(const Rect& bounds) {
    SetFrame(bounds);
    const Rect content = ContentRect();
    for (Widget* c : _children) c->Layout(content);
}

// --------------------------------------------------------------- paint ----

void Widget::Invalidate() {
    InvalidateRect(_frame);
}

void Widget::InvalidateRect(const Rect& r) {
    _needs_paint = true;
    // Propagate upward: the screen root is what actually talks to Display.
    if (_parent != nullptr) _parent->InvalidateRect(r);
}

DrawIntentHint Widget::PaintIntent() const {
    return DrawIntentHint::Static;
}

void Widget::Draw(PaintContext& ctx) {
    if (!_visible) return;
    if (!ctx.ToCanvas(_frame).Intersects(ctx.clip)) return;

    DrawSelf(ctx);

    for (Widget* c : _children) {
        if (c->_visible) c->Draw(ctx);
    }

    if (_focused) DrawFocusRing(ctx);
    _needs_paint = false;
}

void Widget::DrawFocusRing(PaintContext& ctx) {
    if (ctx.canvas == nullptr) return;
    const Rect r = ctx.ToCanvas(_frame.Inflated(tok::kSpaceXs));
    if (r.IsEmpty()) return;

    for (int16_t i = 0; i < tok::kFocusRingWidth; ++i) {
        ctx.canvas->drawRoundRect((int16_t)(r.x + i), (int16_t)(r.y + i),
                                  (int16_t)(r.w - 2 * i), (int16_t)(r.h - 2 * i),
                                  tok::kRadiusMd, tok::kFg);
    }
}

// --------------------------------------------------------------- input ----

Widget* Widget::HitTest(int16_t x, int16_t y) {
    if (!_visible || !_enabled) return nullptr;
    if (!_frame.Contains(x, y)) return nullptr;

    // Back to front -- the last child drawn is the one on top.
    for (auto it = _children.rbegin(); it != _children.rend(); ++it) {
        if (Widget* hit = (*it)->HitTest(x, y)) return hit;
    }
    return this;
}

bool Widget::HandleEvent(const InputEvent& e) {
    if (!_visible || !_enabled) return false;

    if (e.IsPointer()) {
        if (!_frame.Contains(e.x, e.y)) {
            // A press that started here and drifted out must still release.
            if (_pressed && e.kind == InputKind::PointerUp) {
                _pressed = false;
                Invalidate();
            }
            return false;
        }

        for (auto it = _children.rbegin(); it != _children.rend(); ++it) {
            if ((*it)->HandleEvent(e)) return true;
        }

        switch (e.kind) {
            case InputKind::PointerDown:
                if (_on_tap || _focusable) {
                    _pressed = true;
                    Invalidate();
                    return true;
                }
                break;
            case InputKind::PointerUp:
                if (_pressed) {
                    _pressed = false;
                    Invalidate();
                    return true;
                }
                break;
            case InputKind::Tap:
                if (_on_tap) {
                    _on_tap(*this);
                    return true;
                }
                break;
            case InputKind::LongPress:
                if (_on_long_press) {
                    _on_long_press(*this);
                    return true;
                }
                break;
            default:
                break;
        }
        return false;
    }

    if (e.IsKey()) {
        if (_on_key && _on_key(*this, e)) return true;
        // A focused widget with a tap handler treats Enter as a tap, so the
        // side buttons and a BLE keyboard activate the same things touch does.
        if (_focused && e.kind == InputKind::Key && e.keycode == key::kEnter &&
            _on_tap) {
            _on_tap(*this);
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------- state ----

void Widget::SetFocused(bool v) {
    if (_focused == v) return;
    _focused = v;
    if (v) {
        OnFocusGained();
    } else {
        OnFocusLost();
    }
}

void Widget::SetVisible(bool v) {
    if (_visible == v) return;
    _visible = v;
    // Invalidate the frame either way: appearing needs a paint, disappearing
    // needs the background restored.
    InvalidateRect(_frame);
}

void Widget::SetEnabled(bool v) {
    if (_enabled == v) return;
    _enabled = v;
    if (!v) _pressed = false;
    Invalidate();
}

}  // namespace m5ui
