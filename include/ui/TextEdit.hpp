#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include <ui/Semantics.hpp>

namespace playground::ui {
enum class TextBoundary { Grapheme, Word, Line };
std::vector<std::size_t> textBoundaries(std::string_view, TextBoundary);

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
  void validate(std::string_view) const;
  void remember();
  void trimHistory();

public:
  explicit TextEditModel(TextEditProps props = {}, std::string value = {});

  const TextEditProps &props() const noexcept { return _props; }

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
  void move(int direction, bool extend, bool word = false);
  void selectAll();
  std::string selectedText() const;
  bool undo();
  bool redo();
};
} // namespace playground::ui
