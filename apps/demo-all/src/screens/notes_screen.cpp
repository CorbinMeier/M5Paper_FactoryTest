#include "notes_screen.h"

using namespace m5ui;

namespace demo {

namespace {
constexpr const char* kNotesDir = "notes";
constexpr uint16_t kPreviewChars = 60;
}  // namespace

NotesScreen::NotesScreen() : DemoScreen("notes", "Notes") {}

void NotesScreen::BuildActions(AppBar& bar) {
    bar.AddAction("New", [this]() { NewNote(); });
    bar.AddAction("Save", [this]() { SaveCurrent(); });
}

void NotesScreen::BuildContent(Column& column) {
    Device::Get().Files().MakeDir(kNotesDir);

    _status_line = new Label("", tok::kTextXs);
    _status_line->SetColor(tok::kFgMuted);
    column.Add(_status_line);

    // ---- list mode --------------------------------------------------------
    _list = new VirtualList();
    _list->SetRowHeight(tok::kListRowH);
    _list->SetPreferredSize(Size{kDisplayW, 600});
    _list->SetDataSource(
        [this]() { return (uint32_t)_notes.size(); },
        [this](uint32_t index) {
            ListItem item;
            const FileInfo& f = _notes[index];
            item.title = f.name;
            item.subtitle = FormatBytes(f.size);
            return item;
        });
    _list->OnSelect([this](uint32_t index) { OpenNote(index); });
    _list->OnItemLongPress([this](uint32_t index) { DeleteNote(index); });
    column.Add(_list);

    // ---- editor mode ------------------------------------------------------
    _editor = new TextArea(10);
    _editor->SetPlaceholder("Start typing. Tap Save when you are done.");
    _editor->SetVisible(false);
    _editor->OnChange([this](const String&) {
        _dirty = true;
        if (_status_line) _status_line->SetText(_open_path + " -- unsaved");
    });
    column.Add(_editor);

    // The on-screen keyboard is owned by Device so BLE and touch input share
    // one queue; the screen only decides where it sits.
    _keyboard = &Device::Get().Keyboard();
    if (_keyboard->Parent() == nullptr) column.Add(_keyboard);

    ReloadIndex();
    ShowList();
}

void NotesScreen::OnEnter() {
    ReloadIndex();
}

void NotesScreen::ReloadIndex() {
    Storage& files = Device::Get().Files();
    if (!files.IsReady()) {
        if (_status_line) _status_line->SetText("No storage backend mounted.");
        return;
    }

    // Bounded read -- the list is windowed, so there is no reason to
    // materialise more than a screenful plus overscan.
    _notes = files.List(kNotesDir, 0, 128);
    if (_list != nullptr) _list->Reload();

    if (_status_line != nullptr && !_editing) {
        _status_line->SetText(String(_notes.size()) + " notes on " +
                              (files.Backend() == StorageBackend::SdCard
                                   ? "SD"
                                   : "SPIFFS") +
                              ", " + FormatBytes(files.FreeBytes()) + " free");
    }
}

void NotesScreen::ShowList() {
    _editing = false;
    _list->SetVisible(true);
    _editor->SetVisible(false);
    if (_keyboard != nullptr) _keyboard->Hide();
    _bar->SetTitle("Notes");
    InvalidateAll();
}

void NotesScreen::ShowEditor() {
    _editing = true;
    _list->SetVisible(false);
    _editor->SetVisible(true);
    Focus().SetFocus(_editor); // raises the keyboard via OnFocusGained()
    _bar->SetTitle(_open_path);
    InvalidateAll();
}

void NotesScreen::OpenNote(uint32_t index) {
    if (index >= _notes.size()) return;

    _open_path = String(kNotesDir) + "/" + _notes[index].name;
    _editor->SetValue(Device::Get().Files().ReadString(_open_path));
    _dirty = false;
    _status_line->SetText(_open_path);
    ShowEditor();
}

void NotesScreen::NewNote() {
    // Name from the clock, so notes sort chronologically without an index file.
    Clock& clock = Device::Get().Time();
    String name = clock.DateString() + "-" + clock.TimeString();
    name.replace(":", "");
    name.replace(" ", "");

    _open_path = String(kNotesDir) + "/" + name + ".txt";
    _editor->SetValue("");
    _dirty = true;
    ShowEditor();
}

void NotesScreen::SaveCurrent() {
    if (!_editing || _open_path.length() == 0) return;

    // Atomic write: a power loss mid-save leaves the previous version intact.
    const bool ok = Device::Get().Files().WriteString(_open_path, _editor->Value());
    _dirty = !ok;
    _status_line->SetText(ok ? _open_path + " -- saved"
                             : _open_path + " -- SAVE FAILED");
    if (ok) ReloadIndex();
}

void NotesScreen::DeleteNote(uint32_t index) {
    if (index >= _notes.size()) return;

    const String path = String(kNotesDir) + "/" + _notes[index].name;
    if (Device::Get().Files().Remove(path)) {
        _status_line->SetText(path + " -- deleted");
        ReloadIndex();
    }
}

bool NotesScreen::OnBack() {
    // Back out of the editor first; a second back leaves the screen.
    if (!_editing) return false;

    if (_dirty) SaveCurrent();
    ShowList();
    ReloadIndex();
    return true;
}

}  // namespace demo
