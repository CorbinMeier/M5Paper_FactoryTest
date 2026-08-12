#include "components_screen.h"

using namespace m5ui;

namespace demo {

namespace {

// A grey swatch with its level printed on it -- useful for judging which
// levels actually survive a DU update on real glass.
class Swatch : public Widget {
   public:
    explicit Swatch(uint8_t level) : Widget("swatch"), _level(level) {}

    Size Measure(const Constraints& c) override {
        return c.Clamp(Size{28, 44});
    }

    void DrawSelf(PaintContext& ctx) override {
        if (ctx.canvas == nullptr) return;
        const Rect r = ctx.ToCanvas(Frame());
        if (r.IsEmpty()) return;

        ctx.canvas->fillRect(r.x, r.y, r.w, r.h, _level);
        ctx.canvas->drawRect(r.x, r.y, r.w, r.h, tok::kBorder);

        TextStyle style;
        style.size = tok::kTextXs;
        // Flip the label so it stays readable at both ends of the ramp.
        style.color = _level > 7 ? tok::kFg : tok::kInverseFg;
        Text().Draw(*ctx.canvas, String(_level), (int16_t)(r.x + 6),
                    (int16_t)(r.y + r.h / 2 - 8), style);
    }

   private:
    uint8_t _level;
};

}  // namespace

ComponentsScreen::ComponentsScreen() : DemoScreen("components", "Components") {}

void ComponentsScreen::BuildContent(Column& column) {
    BuildTypography(column);
    BuildButtons(column);
    BuildInputs(column);
    BuildIndicators(column);
    BuildGreyRamp(column);
}

void ComponentsScreen::BuildTypography(Column& column) {
    Card* card = new Card();
    column.Add(card);

    card->Add(new Heading("Typography", 3));
    card->Add(new Heading("Heading 1", 1));
    card->Add(new Heading("Heading 2", 2));

    Label* body = new Label(
        "Body text at the default size. This paragraph is long enough to wrap, "
        "which exercises the greedy word-wrap in the text engine and the "
        "measurement cache behind it.",
        tok::kTextMd);
    card->Add(body);

    Label* clamped = new Label(
        "This label is clamped to two lines, so the rest is ellipsized rather "
        "than pushing the layout down the page. Everything after this point "
        "should be invisible.",
        tok::kTextSm);
    clamped->SetMaxLines(2);
    clamped->SetColor(tok::kFgMuted);
    card->Add(clamped);
}

void ComponentsScreen::BuildButtons(Column& column) {
    Card* card = new Card();
    column.Add(card);
    card->Add(new Heading("Buttons", 3));

    Row* row = new Row(tok::kSpaceSm);
    card->Add(row);

    _tap_feedback = new Label("No button pressed yet.", tok::kTextSm);
    _tap_feedback->SetColor(tok::kFgMuted);

    Button* filled = new Button("Filled", ButtonVariant::Filled);
    filled->OnPress([this]() { _tap_feedback->SetText("Filled pressed."); });
    row->Add(filled);

    Button* outline = new Button("Outline", ButtonVariant::Outline);
    outline->OnPress([this]() { _tap_feedback->SetText("Outline pressed."); });
    row->Add(outline);

    Button* ghost = new Button("Ghost", ButtonVariant::Ghost);
    ghost->OnPress([this]() { _tap_feedback->SetText("Ghost pressed."); });
    row->Add(ghost);

    Button* disabled = new Button("Disabled", ButtonVariant::Outline);
    disabled->SetEnabled(false);
    row->Add(disabled);

    card->Add(_tap_feedback);
}

void ComponentsScreen::BuildInputs(Column& column) {
    Card* card = new Card();
    column.Add(card);
    card->Add(new Heading("Inputs", 3));

    TextInput* name = new TextInput();
    name->SetPlaceholder("Tap to focus -- the keyboard raises itself");
    card->Add(name);

    TextInput* password = new TextInput();
    password->SetPassword(true);
    password->SetPlaceholder("Password");
    card->Add(password);

    Label* echo = new Label("", tok::kTextSm);
    echo->SetColor(tok::kFgMuted);
    name->OnChange([echo](const String& value) {
        echo->SetText(value.length() ? "You typed: " + value
                                     : String("Nothing typed yet."));
    });
    card->Add(echo);

    TextArea* notes = new TextArea(4);
    notes->SetPlaceholder("A multi-line field. Enter inserts a newline.");
    card->Add(notes);
}

void ComponentsScreen::BuildIndicators(Column& column) {
    Card* card = new Card();
    column.Add(card);
    card->Add(new Heading("Indicators", 3));

    _progress = new ProgressBar();
    _progress->SetValue(_progress_value);
    card->Add(_progress);

    Row* controls = new Row(tok::kSpaceSm);
    card->Add(controls);

    Button* less = new Button("-10%", ButtonVariant::Outline);
    less->OnPress([this]() {
        _progress_value -= 0.1f;
        _progress->SetValue(_progress_value);
    });
    controls->Add(less);

    Button* more = new Button("+10%", ButtonVariant::Outline);
    more->OnPress([this]() {
        _progress_value += 0.1f;
        _progress->SetValue(_progress_value);
    });
    controls->Add(more);

    ProgressBar* busy = new ProgressBar();
    busy->SetIndeterminate(true);
    card->Add(busy);

    Meter* meter = new Meter("A meter, warning below 40%");
    meter->SetValue(0.28f);
    meter->SetWarnBelow(0.40f);
    meter->SetValueText("28%");
    card->Add(meter);
}

void ComponentsScreen::BuildGreyRamp(Column& column) {
    Card* card = new Card();
    column.Add(card);
    card->Add(new Heading("Grey levels", 3));

    Label* note = new Label(
        "All 16 levels the panel can address. Mid-greys survive GC16 but smear "
        "under DU -- which is why the token palette stays near the ends.",
        tok::kTextXs);
    note->SetColor(tok::kFgMuted);
    card->Add(note);

    Row* ramp = new Row(2);
    card->Add(ramp);
    for (uint8_t level = 0; level < kGreyLevels; ++level) {
        ramp->Add(new Swatch(level));
    }
}

}  // namespace demo
