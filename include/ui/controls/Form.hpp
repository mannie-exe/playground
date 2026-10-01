#pragma once
#include <ui/Node.hpp>
#include <ui/controls/Editing.hpp>

namespace playground::ui {
struct FieldError {
  std::string key;
  ValidationIssue issue;
};

// Form owns coordination, never an application model or a filesystem operation.
class Form {
  struct Entry {
    std::string key;
    NodeHandle<Node> node;
    std::function<void()> reveal;
    std::function<void(ValidationResult)> showError;
  };

  struct State {
    std::vector<std::shared_ptr<Entry>> entries;
  };

  std::shared_ptr<State> _state{std::make_shared<State>()};
  std::vector<FieldError> _errors;

public:
  Connection
  registerField(std::string key, NodeHandle<Node>,
                std::function<void()> reveal = {},
                std::function<void(ValidationResult)> showError = {});
  bool validate();
  bool commit(ChangeContext = {ActionSource::Program, ChangeReason::Submit});
  void reject(std::string key, ValidationIssue);
  void focusFirstInvalid();
  bool dirty() const;
  void revert();

  const std::vector<FieldError> &errors() const noexcept { return _errors; }
};
} // namespace playground::ui
