#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/ustring.h>
#include <unicode/utext.h>
#include <unicode/utf8.h>

#include <ui/TextEdit.hpp>

namespace playground::ui {
namespace {
void check(UErrorCode status) {
  if (U_FAILURE(status))
    throw std::invalid_argument(u_errorName(status));
}

void validUTF8(std::string_view text) {
  if (text.size() > std::size_t(INT32_MAX))
    throw std::length_error("Text too large for Unicode services");
  for (int32_t i = 0; i < int32_t(text.size());) {
    UChar32 cp;
    U8_NEXT(text.data(), i, int32_t(text.size()), cp);
    if (cp < 0 || cp == 0)
      throw std::invalid_argument("Text must be valid UTF-8 without NUL");
  }
}
} // namespace

std::vector<std::size_t> textBoundaries(std::string_view text,
                                        TextBoundary kind) {
  if (kind != TextBoundary::Grapheme && kind != TextBoundary::Word &&
      kind != TextBoundary::Line)
    throw std::invalid_argument("Unknown text boundary kind");
  validUTF8(text);
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<UText, decltype(&utext_close)> source{
      utext_openUTF8(nullptr, text.data(), int64_t(text.size()), &error),
      utext_close};
  check(error);
  std::unique_ptr<UBreakIterator, decltype(&ubrk_close)> breaks{
      ubrk_open(kind == TextBoundary::Grapheme ? UBRK_CHARACTER
                : kind == TextBoundary::Word   ? UBRK_WORD
                                               : UBRK_LINE,
                "", nullptr, 0, &error),
      ubrk_close};
  check(error);
  ubrk_setUText(breaks.get(), source.get(), &error);
  check(error);
  std::vector<std::size_t> result;
  for (int32_t p = ubrk_first(breaks.get()); p != UBRK_DONE;
       p = ubrk_next(breaks.get()))
    result.push_back(std::size_t(p));
  return result;
}

std::vector<BidiRun> visualTextRuns(std::string_view text) {
  validUTF8(text);
  if (text.empty())
    return {};
  std::vector<UChar> utf16;
  std::vector<std::size_t> offsets;
  for (int32_t i = 0; i < int32_t(text.size());) {
    const auto start = std::size_t(i);
    UChar32 cp;
    U8_NEXT(text.data(), i, int32_t(text.size()), cp);
    offsets.push_back(start);
    if (cp > 0xffff) {
      utf16.push_back(U16_LEAD(cp));
      offsets.push_back(start);
      utf16.push_back(U16_TRAIL(cp));
    } else
      utf16.push_back(UChar(cp));
  }
  offsets.push_back(text.size());
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<UBiDi, decltype(&ubidi_close)> bidi{
      ubidi_openSized(int32_t(utf16.size()), 0, &error), ubidi_close};
  check(error);
  ubidi_setPara(bidi.get(), utf16.data(), int32_t(utf16.size()),
                UBIDI_DEFAULT_LTR, nullptr, &error);
  check(error);
  const auto count = ubidi_countRuns(bidi.get(), &error);
  check(error);
  std::vector<BidiRun> runs;
  for (int32_t i = 0; i < count; ++i) {
    int32_t start{}, length{};
    const auto direction = ubidi_getVisualRun(bidi.get(), i, &start, &length);
    runs.push_back(
        {offsets[start], offsets[start + length], direction == UBIDI_RTL});
  }
  return runs;
}

TextEditModel::TextEditModel(TextEditProps props, std::string value)
    : _props{props} {
  if (!_props.maximumBytes)
    throw std::invalid_argument("Text capacity must be positive");
  setValue(std::move(value));
}

void TextEditModel::validate(std::string_view text) const {
  validUTF8(text);
  if (text.size() > _props.maximumBytes)
    throw std::length_error("Text capacity exceeded");
  if (!_props.multiline && text.find_first_of("\r\n") != text.npos)
    throw std::invalid_argument("Single-line field rejects line breaks");
}

void TextEditModel::setProps(TextEditProps props) {
  if (!props.maximumBytes || _state.text.size() > props.maximumBytes ||
      (!props.multiline &&
       _state.text.find_first_of("\r\n") != std::string::npos))
    throw std::invalid_argument("Incompatible editor properties");
  _props = props;
  _composition.clear();
  if (props.password || props.readOnly) {
    _undo.clear();
    _redo.clear();
  }
  trimHistory();
}

void TextEditModel::setValue(std::string text) {
  validate(text);
  _state = {std::move(text), {}};
  _undo.clear();
  _redo.clear();
  _composition.clear();
}

void TextEditModel::setSelection(TextSelection selection) {
  const auto boundaries = textBoundaries(_state.text, TextBoundary::Grapheme);
  if (!std::binary_search(boundaries.begin(), boundaries.end(),
                          selection.anchor) ||
      !std::binary_search(boundaries.begin(), boundaries.end(),
                          selection.caret))
    throw std::invalid_argument("Selection must lie on grapheme boundaries");
  _state.selection = selection;
  _composition.clear();
}

void TextEditModel::setComposition(std::string value, int start, int length) {
  validate(value);
  if (_props.readOnly)
    return;
  std::vector<std::size_t> scalars{0};
  for (int32_t i = 0; i < int32_t(value.size());) {
    UChar32 cp;
    U8_NEXT(value.data(), i, int32_t(value.size()), cp);
    scalars.push_back(std::size_t(i));
  }
  const auto anchor = start < 0
                          ? scalars.size() - 1
                          : std::min(std::size_t(start), scalars.size() - 1);
  const auto caret =
      std::min(anchor + std::size_t(std::max(0, length)), scalars.size() - 1);
  _compositionSelection = {scalars[anchor], scalars[caret]};
  _composition = std::move(value);
}

void TextEditModel::trimHistory() {
  auto bytes = [](const auto &history) {
    std::size_t total{};
    for (const auto &s : history)
      total += s.text.size() + sizeof(State);
    return total;
  };
  while (!_undo.empty() && bytes(_undo) + bytes(_redo) > _props.historyBytes)
    _undo.pop_front();
  while (!_redo.empty() && bytes(_undo) + bytes(_redo) > _props.historyBytes)
    _redo.pop_front();
}

void TextEditModel::remember() {
  if (!_props.password && _props.historyBytes)
    _undo.push_back(_state);
  _redo.clear();
  trimHistory();
}

bool TextEditModel::replace(std::string_view value) {
  if (_props.readOnly)
    return false;
  auto [begin, end] =
      std::minmax(_state.selection.anchor, _state.selection.caret);
  if (value.size() > _props.maximumBytes - (_state.text.size() - (end - begin)))
    throw std::length_error("Text capacity exceeded");
  std::string next = _state.text;
  next.replace(begin, end - begin, value);
  validate(next);
  auto boundaries = textBoundaries(next, TextBoundary::Grapheme);
  const auto caret = *std::lower_bound(boundaries.begin(), boundaries.end(),
                                       begin + value.size());
  if (next == _state.text) {
    _state.selection = {caret, caret};
    _composition.clear();
    return false;
  }
  remember();
  _state.text = std::move(next);
  _state.selection = {caret, caret};
  _composition.clear();
  return true;
}

void TextEditModel::move(int direction, bool extend, bool word) {
  auto boundary = textBoundaries(_state.text, word ? TextBoundary::Word
                                                   : TextBoundary::Grapheme);
  auto position = _state.selection.caret;
  if (!extend && _state.selection.anchor != position)
    position = direction < 0 ? std::min(position, _state.selection.anchor)
                             : std::max(position, _state.selection.anchor);
  else if (direction < 0) {
    auto it = std::lower_bound(boundary.begin(), boundary.end(), position);
    if (it != boundary.begin())
      position = *--it;
  } else {
    auto it = std::upper_bound(boundary.begin(), boundary.end(), position);
    if (it != boundary.end())
      position = *it;
  }
  // Word boundaries are additionally snapped to a legal grapheme boundary.
  auto graphemes = textBoundaries(_state.text, TextBoundary::Grapheme);
  position = *std::lower_bound(graphemes.begin(), graphemes.end(), position);
  setSelection({extend ? _state.selection.anchor : position, position});
}

bool TextEditModel::erase(bool backward, bool word) {
  if (_props.readOnly)
    return false;
  auto old = _state.selection;
  if (old.anchor == old.caret)
    move(backward ? -1 : 1, true, word);
  try {
    const bool changed = replace({});
    if (changed && !_undo.empty())
      _undo.back().selection = old;
    if (!changed)
      _state.selection = old;
    return changed;
  } catch (...) {
    _state.selection = old;
    throw;
  }
}

void TextEditModel::selectAll() { setSelection({0, _state.text.size()}); }

std::string TextEditModel::selectedText() const {
  if (_props.password)
    return {};
  auto [a, b] = std::minmax(_state.selection.anchor, _state.selection.caret);
  return _state.text.substr(a, b - a);
}

bool TextEditModel::undo() {
  if (_props.readOnly || _props.password || _undo.empty())
    return false;
  _redo.push_back(_state);
  _state = std::move(_undo.back());
  _undo.pop_back();
  _composition.clear();
  trimHistory();
  return true;
}

bool TextEditModel::redo() {
  if (_props.readOnly || _props.password || _redo.empty())
    return false;
  _undo.push_back(_state);
  _state = std::move(_redo.back());
  _redo.pop_back();
  _composition.clear();
  trimHistory();
  return true;
}
} // namespace playground::ui
