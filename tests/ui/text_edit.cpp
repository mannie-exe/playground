#include <stdexcept>
#include <string>

#include <support/Test.hpp>
#include <ui/TextEdit.hpp>

using namespace playground;

int main() {
  return test::run([] {
    const std::string combining = "a\xCC\x81",
                      family = "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    test::require(ui::textBoundaries(combining, ui::TextBoundary::Grapheme) ==
                      std::vector<std::size_t>{0, 3},
                  "combining sequence is one grapheme");
    test::require(ui::textBoundaries(family, ui::TextBoundary::Grapheme) ==
                      std::vector<std::size_t>{0, family.size()},
                  "emoji ZWJ is one grapheme");
    ui::TextEditModel edit{{}, combining + family + "x"};
    edit.setSelection({3, 3});
    bool rejected{};
    try {
      edit.setSelection({1, 1});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected && edit.selection().caret == 3,
                  "reject inside grapheme without mutation");
    edit.move(1, false);
    test::require(edit.selection().caret == 3 + family.size(),
                  "move over emoji");
    test::require(edit.erase(true) && edit.value() == combining + "x",
                  "backspace removes entire emoji");
    test::require(edit.undo() &&
                      edit.selection() == ui::TextSelection{3 + family.size(),
                                                            3 + family.size()},
                  "undo restores original caret");
    test::require(edit.redo() && edit.value() == combining + "x",
                  "redo deletion");
    ui::TextEditModel line{{}, combining + " word"};
    line.setSelection({line.value().size(), line.value().size()});
    const auto lineCaret = line.selection();
    test::require(line.eraseTo(3) && line.value() == combining,
                  "delete to explicit grapheme boundary");
    test::require(line.undo() && line.value() == combining + " word" &&
                      line.selection() == lineCaret,
                  "undo targeted deletion restores text and original caret");
    test::require(!line.undo(), "targeted deletion is one undo step");
    test::rejects([&] { line.eraseTo(1); },
                  "targeted deletion rejects interior grapheme offsets");
    test::require(line.value() == combining + " word" &&
                      line.selection() == lineCaret,
                  "invalid targeted deletion preserves text and selection");
    test::require(!line.eraseTo(lineCaret.caret) &&
                      line.selection() == lineCaret,
                  "deleting empty range preserves caret");
    edit.setSelection({0, 3});
    edit.setComposition("b");
    test::require(edit.value() == combining + "x",
                  "preedit doesn't mutate committed value");
    edit.cancelComposition();
    test::require(edit.value() == combining + "x", "cancel is lossless");
    edit.setComposition("b");
    test::require(edit.replace("b") && edit.value() == "bx" &&
                      edit.composition().empty(),
                  "IME commit replaces selected grapheme");
    edit.selectAll();
    edit.replace("hello world");
    edit.move(-1, false);
    test::require(edit.selection().caret == 10, "move one grapheme");
    edit.move(-1, false, true);
    test::require(edit.selection().caret < 10, "word movement");
    const auto before = edit.value();
    rejected = false;
    try {
      edit.replace(std::string(1, char(0x80)));
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected && edit.value() == before,
                  "invalid UTF-8 preserves state");
    rejected = false;
    try {
      edit.replace("\n");
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected, "single line rejects newline");
    ui::TextEditModel small{{.maximumBytes = 3, .historyBytes = 0}, "abc"};
    small.setSelection({3, 3});
    rejected = false;
    try {
      small.replace("d");
    } catch (const std::length_error &) {
      rejected = true;
    }
    test::require(rejected && small.value() == "abc",
                  "capacity checked before publication");
    small.selectAll();
    small.replace("x");
    test::require(!small.undo(), "zero history budget");
    ui::TextEditModel password{{.password = true}, "secret"};
    password.selectAll();
    test::require(password.selectedText().empty(),
                  "password clipboard suppressed");
    password.replace("new");
    test::require(!password.undo(), "password never retained in history");
    auto props = edit.props();
    props.readOnly = true;
    edit.setProps(props);
    test::require(!edit.replace("no") && !edit.erase(true) &&
                      !edit.eraseTo(0) && !edit.undo(),
                  "readonly mutations rejected");
    ui::TextEditModel multi{{.multiline = true}, "a\nb"};
    multi.selectAll();
    test::require(multi.selectedText() == "a\nb", "multiline selection");
    const auto runs = ui::visualTextRuns("abc \xD7\x90\xD7\x91\xD7\x92");
    test::require(runs.size() >= 2 && runs.back().rtl,
                  "ICU resolves mixed bidi runs");
    std::size_t bytes{};
    for (auto run : runs)
      bytes += run.end - run.begin;
    test::require(bytes == 10, "bidi offsets remain UTF-8 bytes");
    rejected = false;
    try {
      ui::textBoundaries("abc", static_cast<ui::TextBoundary>(255));
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected, "invalid boundary mode rejected");
  });
}
