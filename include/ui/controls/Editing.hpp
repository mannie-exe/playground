#pragma once
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <ui/RuntimeServices.hpp>
#include <ui/Semantics.hpp>

namespace playground::ui {
enum class ChangeReason { Input, Enter, Blur, Step, Drag, Submit, Cancel };

struct ChangeContext {
  ActionSource source{ActionSource::Program};
  ChangeReason reason{ChangeReason::Input};
};

struct ValidationIssue {
  std::string code, message;
  bool operator==(const ValidationIssue &) const = default;
};

using ValidationResult = std::optional<ValidationIssue>;
enum class ValidationMode { OnEdit, OnCommit, OnSubmit };
enum class ParseState { Empty, Incomplete, Invalid, Valid };

struct NumberParse {
  ParseState state{ParseState::Invalid};
  double value{};
};

struct NumberCodec {
  std::function<NumberParse(std::string_view)> parse;
  std::function<std::string(double)> format;
};

NumberParse parseNumber(std::string_view);
std::string formatNumber(double);
ValidationResult validateNumber(double, const RangeValue &, bool integer,
                                std::optional<double> multiple = {});

class DraftEditor {
public:
  virtual ~DraftEditor() = default;
  virtual ValidationResult validateDraft() const = 0;
  virtual void showValidation(ValidationResult) = 0;
  virtual bool commitDraft(ChangeContext = {}) = 0;
  virtual void revertDraft(ChangeContext = {ActionSource::Program,
                                            ChangeReason::Cancel}) = 0;
  virtual bool draftDirty() const = 0;
};

class NumericEditor : public virtual DraftEditor {
public:
  virtual double acceptedNumber() const = 0;
  virtual void setNumericRange(RangeValue) = 0;
  virtual void setNumericInteraction(bool enabled, bool readOnly) = 0;
  virtual ActionResult adjustNumber(int, ChangeContext) = 0;
  virtual Connection onNumberEdited(
      support::MoveOnlyFunction<void(double, ChangeContext)>) = 0;
  virtual Connection onDraftChanged(support::MoveOnlyFunction<void()>) = 0;
};
} // namespace playground::ui
