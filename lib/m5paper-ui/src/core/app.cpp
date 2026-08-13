#include "app.h"

#include <string.h>

#include "system_menu.h"

namespace m5ui {

App::App(const char* name) : _name(name) {
    // Registered directly, bypassing Register(), so it never becomes _home
    // (Register() defaults _home to the first route it sees).
    _routes[kSystemMenuRoute] = []() -> Screen* { return new SystemMenuScreen(); };
}

App::~App() {
    for (Screen* s : _stack) delete s;
    _stack.clear();
}

void App::Register(const String& route, ScreenFactory factory) {
    _routes[route] = std::move(factory);
    if (_home.length() == 0) _home = route;
}

Screen* App::Top() const {
    return _stack.empty() ? nullptr : _stack.back();
}

void App::Start() {
    if (_started) return;
    _started = true;
    OnStart();
    if (_stack.empty() && _home.length() > 0) Push(_home);
}

bool App::Push(const String& route) {
    auto it = _routes.find(route);
    if (it == _routes.end()) {
        log_e("no such route: %s", route.c_str());
        return false;
    }

    Screen* screen = it->second();
    if (screen == nullptr) return false;

    if (Screen* prev = Top()) prev->OnPause();

    screen->SetOwner(this);
    screen->EnsureBuilt();
    _stack.push_back(screen);
    screen->OnEnter();
    screen->InvalidateAll();

    EvictColdScreens();
    return true;
}

bool App::Pop() {
    if (_stack.size() <= 1) return false;

    Screen* top = _stack.back();
    top->OnExit();
    _stack.pop_back();
    delete top;

    Screen* now = Top();
    if (now != nullptr) {
        now->EnsureBuilt(); // may have been evicted while buried
        now->OnResume();
        now->InvalidateAll();
    }
    return true;
}

bool App::Replace(const String& route) {
    auto it = _routes.find(route);
    if (it == _routes.end()) return false;

    Screen* screen = it->second();
    if (screen == nullptr) return false;

    if (!_stack.empty()) {
        Screen* top = _stack.back();
        top->OnExit();
        _stack.pop_back();
        delete top;
    }

    screen->SetOwner(this);
    screen->EnsureBuilt();
    _stack.push_back(screen);
    screen->OnEnter();
    screen->InvalidateAll();
    return true;
}

void App::PopToRoot() {
    while (_stack.size() > 1) {
        Screen* top = _stack.back();
        top->OnExit();
        _stack.pop_back();
        delete top;
    }
    if (Screen* now = Top()) {
        now->EnsureBuilt();
        now->OnResume();
        now->InvalidateAll();
    }
}

bool App::OpenSystemMenu() {
    if (Screen* top = Top()) {
        if (strcmp(top->Name(), kSystemMenuRoute) == 0) return false;
    }
    return Push(kSystemMenuRoute);
}

void App::EvictColdScreens() {
    if (kColdDepth == 0 || _stack.size() <= kColdDepth) return;

    // Everything below the cold threshold gives back its widget tree. The
    // Screen object stays so navigation state and scroll positions survive.
    const size_t cutoff = _stack.size() - kColdDepth;
    for (size_t i = 0; i < cutoff; ++i) {
        if (_stack[i]->IsBuilt()) _stack[i]->Teardown();
    }
}

void App::DispatchEvent(const InputEvent& e) {
    Screen* top = Top();
    if (top == nullptr) return;

    if (top->HandleEvent(e)) return;

    // Unclaimed back gesture: the Left side button, mapped to PageUp by
    // default, is not back -- only Escape is, and Screen already handled it.
    // Anything reaching here is genuinely unhandled.
}

void App::Tick() {
    if (Screen* top = Top()) top->Tick();
}

void App::Paint(Display& display) {
    if (Screen* top = Top()) top->Paint(display);
}

DrawIntent App::PaintIntent() const {
    Screen* top = Top();
    return top ? top->PaintIntent() : DrawIntent::Static;
}

}  // namespace m5ui
