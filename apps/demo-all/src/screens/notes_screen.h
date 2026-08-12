#pragma once
// A cut-down Notes app -- the vertical slice through storage, keyboard and
// text editing that the real Notes target (issue #90) will build on.
//
// Two modes on one screen: a list of notes, and an editor. Kept together
// because the editor's back action needs the list's state.

#include "demo_screen.h"

namespace demo {

class NotesScreen : public DemoScreen {
   public:
    NotesScreen();

    bool OnBack() override;
    void OnEnter() override;

   protected:
    void BuildContent(m5ui::Column& column) override;
    void BuildActions(m5ui::AppBar& bar) override;

   private:
    void ReloadIndex();
    void OpenNote(uint32_t index);
    void NewNote();
    void SaveCurrent();
    void DeleteNote(uint32_t index);
    void ShowList();
    void ShowEditor();

    m5ui::VirtualList* _list = nullptr;
    m5ui::TextArea* _editor = nullptr;
    m5ui::Label* _status_line = nullptr;
    m5ui::OnScreenKeyboard* _keyboard = nullptr;

    std::vector<m5ui::FileInfo> _notes;
    String _open_path;
    bool _editing = false;
    bool _dirty = false;
};

}  // namespace demo
