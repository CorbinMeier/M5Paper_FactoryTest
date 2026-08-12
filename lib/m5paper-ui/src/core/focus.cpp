#include "focus.h"

#include <algorithm>

namespace m5ui {

namespace {

// Squared distance -- avoids a sqrt in the spatial search inner loop.
int32_t Dist2(const Rect& a, const Rect& b) {
    const int32_t dx = a.CenterX() - b.CenterX();
    const int32_t dy = a.CenterY() - b.CenterY();
    return dx * dx + dy * dy;
}

bool IsInDirection(const Rect& from, const Rect& to, FocusDirection dir) {
    switch (dir) {
        case FocusDirection::Up:    return to.CenterY() < from.CenterY();
        case FocusDirection::Down:  return to.CenterY() > from.CenterY();
        case FocusDirection::Left:  return to.CenterX() < from.CenterX();
        case FocusDirection::Right: return to.CenterX() > from.CenterX();
        default:                    return true;
    }
}

}  // namespace

void FocusManager::Collect(Widget* w) {
    if (w == nullptr || !w->IsVisible()) return;
    if (w->IsFocusable()) _ring.push_back(w);
    for (Widget* c : w->Children()) Collect(c);
}

void FocusManager::Rebuild(Widget* root) {
    _ring.clear();
    Collect(root);

    // A focused widget that was removed or hidden must not keep focus.
    if (_focused != nullptr && IndexOf(_focused) < 0) {
        _focused->SetFocused(false);
        _focused = nullptr;
    }
}

int FocusManager::IndexOf(Widget* w) const {
    for (size_t i = 0; i < _ring.size(); ++i) {
        if (_ring[i] == w) return (int)i;
    }
    return -1;
}

bool FocusManager::SetFocus(Widget* w) {
    if (w == _focused) return true;
    if (w != nullptr && !w->IsFocusable()) return false;

    if (_focused != nullptr) _focused->SetFocused(false);
    _focused = w;
    if (_focused != nullptr) _focused->SetFocused(true);
    return true;
}

void FocusManager::ClearFocus() {
    SetFocus(nullptr);
}

Widget* FocusManager::Move(FocusDirection dir) {
    if (_ring.empty()) return nullptr;

    if (dir != FocusDirection::Next && dir != FocusDirection::Previous) {
        return MoveSpatial(dir);
    }

    const int current = IndexOf(_focused);
    const int count = (int)_ring.size();
    const int step = dir == FocusDirection::Next ? 1 : -1;

    // Start from the current position, or from the ends when nothing is
    // focused, and take the first entry that is still focusable.
    int idx = current < 0 ? (step > 0 ? 0 : count - 1) : current;
    for (int tried = 0; tried < count; ++tried) {
        idx = (idx + step + count) % count;
        if (_ring[idx]->IsFocusable()) {
            SetFocus(_ring[idx]);
            return _focused;
        }
    }
    return _focused;
}

Widget* FocusManager::MoveSpatial(FocusDirection dir) {
    if (_ring.empty()) return nullptr;
    if (_focused == nullptr) return Move(FocusDirection::Next);

    const Rect from = _focused->Frame();
    Widget* best = nullptr;
    int32_t best_d = INT32_MAX;

    for (Widget* w : _ring) {
        if (w == _focused || !w->IsFocusable()) continue;
        if (!IsInDirection(from, w->Frame(), dir)) continue;

        const int32_t d = Dist2(from, w->Frame());
        if (d < best_d) {
            best_d = d;
            best = w;
        }
    }

    if (best != nullptr) SetFocus(best);
    return _focused;
}

bool FocusManager::DispatchKey(const InputEvent& e) {
    if (!e.IsKey()) return false;

    // Give the focused widget and its ancestors first refusal -- a TextArea
    // wants Up/Down for caret movement, not focus movement.
    for (Widget* w = _focused; w != nullptr; w = w->Parent()) {
        if (w->HandleEvent(e)) return true;
    }

    if (e.kind == InputKind::KeyUp) return false;

    switch (e.keycode) {
        case key::kTab:
            Move(e.Has(mod::kShift) ? FocusDirection::Previous
                                    : FocusDirection::Next);
            return true;
        case key::kUp:    Move(FocusDirection::Up); return true;
        case key::kDown:  Move(FocusDirection::Down); return true;
        case key::kLeft:  Move(FocusDirection::Left); return true;
        case key::kRight: Move(FocusDirection::Right); return true;
        default:          return false;
    }
}

void FocusManager::FocusFromPointer(Widget* root, int16_t x, int16_t y) {
    Widget* hit = root ? root->HitTest(x, y) : nullptr;

    // Walk up to the nearest focusable ancestor: a tap lands on a Label inside
    // a focusable row, and the row is what should take focus.
    for (Widget* w = hit; w != nullptr; w = w->Parent()) {
        if (w->IsFocusable()) {
            SetFocus(w);
            return;
        }
    }
    // Tapping empty space clears focus, which also dismisses the on-screen
    // keyboard when no field is active.
    ClearFocus();
}

}  // namespace m5ui
