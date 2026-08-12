#include "screen.h"

#include "app.h"

namespace m5ui {

namespace {

// The root of a screen's tree. Its job beyond a plain Widget is to accumulate
// the dirty union, so Paint() can hand Display one rect rather than repainting
// the whole panel on any change.
class ScreenRoot : public Widget {
   public:
    ScreenRoot() : Widget("screen-root") {}

    void InvalidateRect(const Rect& r) override {
        _needs_paint = true;
        _dirty = _dirty.Union(r.Clipped());
    }

    Rect TakeDirty() {
        const Rect d = _dirty;
        _dirty = Rect{};
        return d;
    }

    void DrawSelf(PaintContext& ctx) override {
        if (ctx.canvas == nullptr) return;
        const Rect r = ctx.ToCanvas(_frame);
        if (r.IsEmpty()) return;
        ctx.canvas->fillRect(r.x, r.y, r.w, r.h, tok::kSurface);
    }

   private:
    Rect _dirty;
};

}  // namespace

Screen::Screen(const char* name) : _name(name) {}

Screen::~Screen() {
    Teardown();
}

void Screen::EnsureBuilt() {
    if (_built) return;

    _root = new ScreenRoot();
    _root->SetFrame(Rect::FullScreen());
    Build();
    _built = true;
    _needs_layout = true;
}

void Screen::Teardown() {
    if (_root != nullptr) {
        _focus.ClearFocus();
        delete _root; // deletes the whole subtree
        _root = nullptr;
    }
    _built = false;
    _needs_layout = true;
}

void Screen::SetChromeInsets(int16_t top, int16_t bottom) {
    if (_chrome_top == top && _chrome_bottom == bottom) return;
    _chrome_top = top;
    _chrome_bottom = bottom;
    _needs_layout = true;
}

Rect Screen::ContentBounds() const {
    return Rect{0, _chrome_top, kDisplayW,
                (int16_t)(kDisplayH - _chrome_top - _chrome_bottom)};
}

void Screen::PerformLayout() {
    if (_root == nullptr) return;
    if (!_needs_layout) return;

    _root->Layout(Rect::FullScreen());
    _focus.Rebuild(_root);
    _needs_layout = false;
}

void Screen::InvalidateAll() {
    if (_root != nullptr) _root->InvalidateRect(Rect::FullScreen());
    _needs_layout = true;
}

void Screen::Paint(Display& display) {
    if (_root == nullptr) return;
    PerformLayout();

    ScreenRoot* root = static_cast<ScreenRoot*>(_root);
    const Rect dirty = root->TakeDirty();
    if (dirty.IsEmpty()) return;

    PaintContext ctx;
    ctx.canvas = &display.Canvas();
    ctx.clip = dirty;

    // Clear the dirty area to the page background before repainting it --
    // widgets draw their own content, not the space they vacated.
    ctx.canvas->fillRect(dirty.x, dirty.y, dirty.w, dirty.h, tok::kSurface);

    _root->Draw(ctx);
    display.Invalidate(dirty);
}

bool Screen::HandleEvent(const InputEvent& e) {
    if (_root == nullptr) return false;

    if (e.IsKey()) {
        // Escape and the Left side button are back, unless the screen claims
        // them first.
        if (_focus.DispatchKey(e)) return true;
        if (e.kind == InputKind::Key && e.keycode == key::kEscape) {
            if (!OnBack() && _app != nullptr) _app->Pop();
            return true;
        }
        return false;
    }

    if (e.kind == InputKind::PointerDown) {
        _focus.FocusFromPointer(_root, e.x, e.y);
    }
    return _root->HandleEvent(e);
}

}  // namespace m5ui
