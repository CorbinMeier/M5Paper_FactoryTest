#include "demo_screen.h"

using namespace m5ui;

namespace demo {

DemoScreen::DemoScreen(const char* name, const String& title, bool show_back)
    : Screen(name), _title(title), _show_back(show_back) {}

void DemoScreen::Build() {
    // The root is a full-screen column: status bar, app bar, then the content
    // area taking whatever is left.
    Column* page = new Column(0);
    Root()->Add(page);

    _status = new StatusBar();
    _status->SetTitle("M5Paper");
    _status->SetShowMemory(true);
    page->Add(_status);

    _bar = new AppBar(_title);
    if (_show_back) _bar->SetBackAction();
    BuildActions(*_bar);
    page->Add(_bar);

    _scroll = new ScrollView();
    _scroll->SetFlex(1); // absorbs the remaining height
    page->Add(_scroll);

    _content = new Column(tok::kSpaceMd);
    _content->SetPadding(EdgeInsets::All(tok::kSpaceMd));
    _scroll->SetContent(_content);

    BuildContent(*_content);

    SetChromeInsets(tok::kStatusBarH, 0);
}

void DemoScreen::Tick() {
    // The status bar rate-limits itself, so calling this every loop is cheap.
    if (_status != nullptr) _status->Refresh();
}

Row* InfoRow(const String& caption, const String& value) {
    Row* row = new Row(tok::kSpaceSm);

    Label* left = new Label(caption, tok::kTextSm);
    left->SetColor(tok::kFgMuted);
    row->Add(left);

    row->Add(new Spacer()); // pushes the value to the right edge

    Label* right = new Label(value, tok::kTextSm);
    right->SetAlign(TextAlign::Right);
    row->Add(right);

    return row;
}

}  // namespace demo
