#include <ui/collections/ScrollView.hpp>
#include <ui/controls/Form.hpp>

namespace playground::ui {
Connection
Form::registerField(std::string key, NodeHandle<Node> node,
                    std::function<void()> reveal,
                    std::function<void(ValidationResult)> showError) {
  if (key.empty() || !node.get() || !dynamic_cast<DraftEditor *>(node.get()))
    throw std::invalid_argument("Form requires a keyed attached editor");
  for (auto &e : _state->entries)
    if (e->key == key)
      throw std::invalid_argument("Duplicate form field key");
  auto entry = std::make_shared<Entry>(
      Entry{std::move(key), node, std::move(reveal), std::move(showError)});
  _state->entries.push_back(entry);
  return Connection{[state = std::weak_ptr{_state}, entry]() noexcept {
    if (auto s = state.lock())
      std::erase(s->entries, entry);
  }};
}

bool Form::validate() {
  _errors.clear();
  const auto entries = _state->entries;
  for (auto &e : entries) {
    auto *node = e->node.get();
    auto *editor = dynamic_cast<DraftEditor *>(node);
    if (!editor) {
      _errors.push_back(
          {e->key, {"unavailable", "Field is no longer available"}});
      continue;
    }
    auto issue = editor->validateDraft();
    editor->showValidation(issue);
    if (e->showError)
      e->showError(issue);
    if (issue)
      _errors.push_back({e->key, *issue});
  }
  return _errors.empty();
}

bool Form::commit(ChangeContext context) {
  if (!validate()) {
    focusFirstInvalid();
    return false;
  }
  const auto entries = _state->entries;
  for (auto &e : entries) {
    auto *editor = dynamic_cast<DraftEditor *>(e->node.get());
    if (!editor || !editor->commitDraft(context)) {
      reject(e->key, {"changed", "Field changed during submission"});
      return false;
    }
  }
  return true;
}

void Form::reject(std::string key, ValidationIssue issue) {
  _errors.push_back({key, issue});
  const auto entries = _state->entries;
  for (auto &entry : entries)
    if (entry->key == key)
      if (auto *editor = dynamic_cast<DraftEditor *>(entry->node.get()))
        editor->showValidation(issue);
  for (auto &e : entries)
    if (e->key == key && e->showError)
      e->showError(issue);
  focusFirstInvalid();
}

void Form::focusFirstInvalid() {
  if (_errors.empty())
    return;
  const auto entries = _state->entries;
  const auto first = _errors.front().key;
  for (auto &e : entries)
    if (e->key == first) {
      if (e->reveal)
        e->reveal();
      if (auto *node = e->node.get()) {
        node->focusTarget().requestFocusAfterLayout();
        for (auto *parent = node->parent(); parent; parent = parent->parent())
          if (auto *scroll = dynamic_cast<ScrollView *>(parent))
            scroll->scrollIntoView(*node);
      }
      break;
    }
}

bool Form::dirty() const {
  for (auto &e : _state->entries)
    if (auto *editor = dynamic_cast<DraftEditor *>(e->node.get());
        editor && editor->draftDirty())
      return true;
  return false;
}

void Form::revert() {
  const auto entries = _state->entries;
  for (auto &e : entries) {
    if (auto *editor = dynamic_cast<DraftEditor *>(e->node.get()))
      editor->revertDraft();
    if (e->showError)
      e->showError({});
  }
  _errors.clear();
}
} // namespace playground::ui
