#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include <support/Unicode.hpp>
#include <ui/Semantics.hpp>

namespace playground::ui {
using support::textBoundaries;
using support::TextBoundary;

struct BidiRun {
  std::size_t begin{}, end{};
  bool rtl{};
};

std::vector<BidiRun> visualTextRuns(std::string_view);

struct TextEditProps {
  bool multiline{}, readOnly{}, password{};
  std::size_t maximumBytes{64 * 1024};
  std::size_t historyBytes{256 * 1024};
  bool operator==(const TextEditProps &) const = default;
};

struct TextInputState {
  bool multiline{}, readOnly{}, password{};
  math::Rect caret;
  bool composing{};
};

class TextInputClient {
public:
  virtual ~TextInputClient() = default;
  virtual TextInputState textInputState() const = 0;
};

class TextEditModel {
  struct State {
    std::string text;
    TextSelection selection;
  };

  TextEditProps _props;
  State _state;
  std::string _composition;
  TextSelection _compositionSelection;
  std::deque<State> _undo, _redo;
  void remember();
  void trimHistory();
  bool eraseSelection(TextSelection previous);

public:
  explicit TextEditModel(TextEditProps props = {}, std::string value = {});

  const TextEditProps &props() const noexcept { return _props; }

  void validateValue(std::string_view) const;

  void setProps(TextEditProps);

  const std::string &value() const noexcept { return _state.text; }

  TextSelection selection() const noexcept { return _state.selection; }

  const std::string &composition() const noexcept { return _composition; }

  TextSelection compositionSelection() const noexcept {
    return _compositionSelection;
  }

  void setValue(std::string);
  void setSelection(TextSelection);
  void setComposition(std::string, int start = -1, int length = 0);

  void cancelComposition() noexcept { _composition.clear(); }

  bool replace(std::string_view);
  bool erase(bool backward, bool word = false);
  // Delete between the caret and a UTF-8 grapheme boundary, as one undo step.
  bool eraseTo(std::size_t offset);
  void move(int direction, bool extend, bool word = false);
  void selectAll();
  std::string selectedText() const;
  bool undo();
  bool redo();
};
} // namespace playground::ui
