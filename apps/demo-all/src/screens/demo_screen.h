#pragma once
// Shared chrome for every demo screen: a status bar at the top, an app bar
// under it, and a scrolling content column below.
//
// Subclasses implement BuildContent() and add widgets to the returned column;
// the chrome, the scroll behaviour and the back action come for free.

#include <m5paper_ui.h>

namespace demo {

class DemoScreen : public m5ui::Screen {
   public:
    explicit DemoScreen(const char* name, const String& title,
                        bool show_back = true);

    void Build() override;
    void Tick() override;

   protected:
    // Add content widgets to `column`. Called once per Build().
    virtual void BuildContent(m5ui::Column& column) = 0;

    // Right-aligned app bar actions. Called after the bar exists.
    virtual void BuildActions(m5ui::AppBar& bar) {
        (void)bar;
    }

    m5ui::StatusBar* _status = nullptr;
    m5ui::AppBar* _bar = nullptr;
    m5ui::ScrollView* _scroll = nullptr;
    m5ui::Column* _content = nullptr;

   private:
    String _title;
    bool _show_back;
};

// A caption/value pair on one row -- the whole device-info screen is these.
m5ui::Row* InfoRow(const String& caption, const String& value);

}  // namespace demo
