#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <ui/Interaction.hpp>
#include <ui/NodeProps.hpp>

namespace playground::ui {
enum class ActionSource { Pointer, Keyboard, Gamepad, Assistive, Program };
enum class ActionResult { Applied, Unchanged, Unsupported, Unavailable, Stale };
enum class CheckState { Off, On, Mixed };

struct Activate {};

struct CommitEdit {};

struct CancelEdit {};

struct CustomAction {
  std::int32_t id;
};

struct Focus {};

struct Increment {
  int direction{1};
};

struct SetValue {
  double value;
};

struct SetChecked {
  CheckState value;
};

struct SetExpanded {
  bool value;
};

struct SelectItem {
  std::string key;
};

struct ScrollIntoView {};

struct TextSelection {
  std::size_t anchor{}, caret{};
  bool operator==(const TextSelection &) const = default;
};

struct ReplaceSelectedText {
  std::string value;
};

using UIAction =
    std::variant<Activate, CommitEdit, CancelEdit, CustomAction, Focus,
                 Increment, SetValue, SetChecked, SetExpanded, SelectItem,
                 ScrollIntoView, TextSelection, ReplaceSelectedText>;
enum class SemanticAction {
  Activate,
  Focus,
  Increment,
  Decrement,
  SetValue,
  SetChecked,
  Expand,
  Collapse,
  Select,
  ScrollIntoView,
  SetSelection,
  ReplaceText
};

struct RangeValue {
  double value{}, minimum{}, maximum{100}, step{1};
  void validate() const;
  double adjusted(int direction) const;
  bool operator==(const RangeValue &) const = default;
};

struct SemanticTextRun {
  std::string text;
  std::vector<std::size_t> byteOffsets;
  math::Rect bounds;
  std::vector<float> positions, widths;
  bool rtl{};
  bool operator==(const SemanticTextRun &) const = default;
};

struct SemanticCustomAction {
  std::int32_t id;
  std::string description;
  bool operator==(const SemanticCustomAction &) const = default;
};

struct SemanticState {
  SemanticProps description;
  bool readOnly{}, selected{}, required{}, invalid{}, protectedText{};
  std::optional<CheckState> checked;
  std::optional<bool> expanded;
  std::optional<RangeValue> range;
  std::optional<TextSelection> selection;
  std::optional<NodeId> activeDescendant;
  std::vector<SemanticTextRun> textRuns;
  std::vector<SemanticAction> actions;
  std::vector<SemanticCustomAction> customActions;
  bool operator==(const SemanticState &) const = default;
};

struct SemanticNode {
  NodeId id;
  NodeId parent;
  math::Rect bounds;
  SemanticState state;
  bool operator==(const SemanticNode &) const = default;
};

struct SemanticSnapshot {
  std::uint64_t session{};
  NodeId focus;
  std::vector<SemanticNode> nodes;
  bool operator==(const SemanticSnapshot &) const = default;
};
} // namespace playground::ui
