#include <algorithm>
#include <string>
#include <vector>

#include <support/Test.hpp>
#include <support/TextFlow.hpp>
#include <support/Unicode.hpp>
#include <ui/TextEdit.hpp>

using namespace playground;

int main() {
  return test::run([] {
    struct Example {
      std::string text;
      std::vector<std::size_t> offsets;
    };
    const std::string combining = "a\xCC\x81";
    const std::string family =
        "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    const std::string flag = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
    // Unicode 17 and 18 differ here. All consumers must follow the linked ICU
    // policy together, including when an installed ICU newer than 78 is used.
    const std::string indic = "\xE0\xA5\x8D\xE0\xA4\xA4"; // U+094D U+0924
    const auto indicOffsets =
        support::textBoundaries(indic, support::TextBoundary::Grapheme);
    const std::vector<Example> examples{
        {"", {0}}, {"abc", {0, 1, 2, 3}},
        {combining + "x", {0, combining.size(), combining.size() + 1}},
        {family + "x", {0, family.size(), family.size() + 1}},
        {flag, {0, flag.size()}}, {indic, indicOffsets},
        {"\r\nx", {0, 2, 3}}};
    for (const auto &[text, offsets] : examples) {
      test::require(support::textBoundaries(text, support::TextBoundary::Grapheme) ==
                        offsets,
                    "shared ICU graphemes preserve UTF-8 byte offsets");
      test::require(ui::graphemeBoundaries(text) == offsets &&
                        ui::textBoundaries(text, ui::TextBoundary::Grapheme) == offsets,
                    "label and editor/accessibility routes share boundaries");
      ui::TextEditModel edit{{.multiline = true}, text};
      for (std::size_t offset = 0; offset <= text.size(); ++offset) {
        if (std::binary_search(offsets.begin(), offsets.end(), offset)) {
          edit.setSelection({offset, offset});
          test::require(edit.selection().caret == offset,
                        "editor accepts every label boundary");
        } else {
          const auto before = edit.selection();
          test::rejects([&] { edit.setSelection({offset, offset}); },
                        "editor rejects every non-boundary byte offset");
          test::require(edit.selection() == before,
                        "invalid selection preserves state");
        }
      }
      edit.setSelection({0, 0});
      for (std::size_t i = 1; i < offsets.size(); ++i) {
        edit.move(1, false);
        test::require(edit.selection().caret == offsets[i],
                      "caret movement follows label boundaries");
      }
    }
    test::require(ui::truncateText(indic, {.truncation = ui::TextTruncation::EllipsisEnd,
                                          .ellipsis = "."},
                                   [](std::string_view text) { return text.size() <= 4; }) ==
                      (indicOffsets.size() == 3 ? indic.substr(0, 3) + "." : "."),
                  "Indic truncation follows the same ICU boundary as editing");
    ui::TextEditModel indicEdit{{}, indic};
    test::require(indicEdit.erase(false) && indicEdit.value() == indic.substr(indicOffsets[1]),
                  "Indic forward delete agrees with truncation");
    test::require(indicEdit.undo() && indicEdit.value() == indic &&
                      indicEdit.selection().caret == 0,
                  "Indic deletion undo preserves byte-positioned caret");

    const std::vector<std::string> invalid{
        "\xFF", "\x80", "\xC0\xAF", "\xE2\x82", "\xED\xA0\x80",
        "\xF4\x90\x80\x80", std::string{"a\0b", 3}, std::string(1, '\0')};
    for (const auto &text : invalid) {
      test::rejects([&] { support::validateTextUTF8(text); },
                    "shared validator rejects malformed UTF-8 and NUL");
      test::rejects([&] { ui::graphemeBoundaries(text); },
                    "label boundaries reject malformed UTF-8 and NUL");
      for (const auto kind : {ui::TextBoundary::Grapheme, ui::TextBoundary::Word,
                              ui::TextBoundary::Line})
        test::rejects([&] { ui::textBoundaries(text, kind); },
                      "all editor/accessibility boundary kinds reject invalid text");
      test::rejects([&] { ui::TextEditModel edit{{}, text}; },
                    "editor values reject invalid text");
      test::rejects([&] { ui::visualTextRuns(text); },
                    "bidi uses the same validation policy");
      for (const auto mode : {ui::TextTruncation::None,
                              ui::TextTruncation::EllipsisEnd}) {
        test::rejects([&] {
          ui::truncateText(text, {.truncation = mode},
                           [](std::string_view) { return true; });
        }, "truncation validates even when the whole value fits");
        test::rejects([&] {
          ui::truncateText("valid", {.truncation = mode, .ellipsis = text},
                           [](std::string_view) { return true; });
        }, "truncation validates unused ellipsis consistently with TextProps");
      }
    }
  });
}
