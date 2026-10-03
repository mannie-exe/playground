#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL3/SDL_mouse.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <SDL3/SDL.h>
#include <accesskit.h>

#if defined(ACCESSKIT_MACOS)
#include <set>

#include <objc/runtime.h>
#endif

#include <platform/sdl/WindowServices.hpp>
#include <support/SDLError.hpp>
#include <ui/TextEdit.hpp>

namespace playground::sdl {
namespace {
accesskit_role role(ui::SemanticRole value) {
  using R = ui::SemanticRole;
  switch (value) {
  case R::Button:
    return ACCESSKIT_ROLE_BUTTON;
  case R::Text:
    return ACCESSKIT_ROLE_LABEL;
  case R::Image:
    return ACCESSKIT_ROLE_IMAGE;
  case R::Checkbox:
    return ACCESSKIT_ROLE_CHECK_BOX;
  case R::Switch:
    return ACCESSKIT_ROLE_SWITCH;
  case R::Radio:
    return ACCESSKIT_ROLE_RADIO_BUTTON;
  case R::RadioGroup:
    return ACCESSKIT_ROLE_RADIO_GROUP;
  case R::Slider:
    return ACCESSKIT_ROLE_SLIDER;
  case R::SpinButton:
    return ACCESSKIT_ROLE_SPIN_BUTTON;
  case R::TextField:
    return ACCESSKIT_ROLE_TEXT_INPUT;
  case R::TextArea:
    return ACCESSKIT_ROLE_MULTILINE_TEXT_INPUT;
  case R::Password:
    return ACCESSKIT_ROLE_PASSWORD_INPUT;
  case R::ListBox:
    return ACCESSKIT_ROLE_LIST_BOX;
  case R::Option:
    return ACCESSKIT_ROLE_LIST_BOX_OPTION;
  case R::Select:
    return ACCESSKIT_ROLE_COMBO_BOX;
  case R::Disclosure:
    return ACCESSKIT_ROLE_DISCLOSURE_TRIANGLE;
  case R::Tabs:
    return ACCESSKIT_ROLE_TAB_LIST;
  case R::Tab:
    return ACCESSKIT_ROLE_TAB;
  case R::Dialog:
    return ACCESSKIT_ROLE_DIALOG;
  case R::Menu:
    return ACCESSKIT_ROLE_MENU;
  case R::MenuItem:
    return ACCESSKIT_ROLE_MENU_ITEM;
  case R::Tooltip:
    return ACCESSKIT_ROLE_TOOLTIP;
  case R::Meter:
    return ACCESSKIT_ROLE_METER;
  case R::Toolbar:
    return ACCESSKIT_ROLE_TOOLBAR;
  case R::AlertDialog:
    return ACCESSKIT_ROLE_ALERT_DIALOG;
  case R::Progress:
    return ACCESSKIT_ROLE_PROGRESS_INDICATOR;
  case R::Status:
    return ACCESSKIT_ROLE_STATUS;
  default:
    return ACCESSKIT_ROLE_GENERIC_CONTAINER;
  }
}

accesskit_action action(ui::SemanticAction value) {
  using A = ui::SemanticAction;
  switch (value) {
  case A::Focus:
    return ACCESSKIT_ACTION_FOCUS;
  case A::Increment:
    return ACCESSKIT_ACTION_INCREMENT;
  case A::Decrement:
    return ACCESSKIT_ACTION_DECREMENT;
  case A::SetValue:
    return ACCESSKIT_ACTION_SET_VALUE;
  case A::Expand:
    return ACCESSKIT_ACTION_EXPAND;
  case A::Collapse:
    return ACCESSKIT_ACTION_COLLAPSE;
  case A::ScrollIntoView:
    return ACCESSKIT_ACTION_SCROLL_INTO_VIEW;
  case A::SetSelection:
    return ACCESSKIT_ACTION_SET_TEXT_SELECTION;
  case A::ReplaceText:
    return ACCESSKIT_ACTION_REPLACE_SELECTED_TEXT;
  default:
    return ACCESSKIT_ACTION_CLICK;
  }
}
} // namespace

struct WindowServices::Impl {
  SDL_Window *window;
  runtime::ActivationToken relativeOwner;
  std::uint64_t relativeGeneration{};
  bool relative{};
  // Owner thread only; attachment clears before root destruction.
  ui::UIRoot *root{};
  std::map<ui::UIRoot *, std::size_t> attachments;
  ui::AccessibilityMode mode{ui::AccessibilityMode::Auto};
  bool publicationDirty{true};
  bool buildFailed{};
  bool windowFocused{};
  std::mutex mutex;
  ui::SemanticSnapshot snapshot;
  using Identity =
      std::tuple<std::uint64_t, std::uint32_t, std::uint64_t, std::size_t>;
  std::map<Identity, std::uint64_t> ids;
  std::uint64_t nextId{1};
  std::string title;
  ui::NodeId textOwner;
  std::uint64_t textSession{};
  std::optional<std::pair<bool, bool>> textOptions;
  bool composing{};
  std::function<void()> wake;
  std::optional<ui::UIRoot::PublicationKey> publishedKey;
  platform::ViewportMapping publishedMapping;
  math::Vec2f publishedNativeScale;

  struct Request {
    std::uint64_t session;
    ui::NodeId target;
    ui::UIAction action;
  };

  std::deque<Request> requests;
#if defined(_WIN32)
  accesskit_windows_subclassing_adapter *adapter{};
#elif defined(ACCESSKIT_MACOS)
  accesskit_macos_subclassing_adapter *adapter{};
#elif defined(__linux__)
  accesskit_unix_adapter *adapter{};
#endif
  explicit Impl(SDL_Window *value) : window{value} {
    if (!value)
      throw std::invalid_argument("Window services require a window");
    title = SDL_GetWindowTitle(value);
  }

  std::uint64_t id(ui::NodeId value, std::size_t run = 0) {
    if (value == ui::NodeId{})
      return 0;
    auto [it, added] = ids.try_emplace(
        {snapshot.session, value.index, value.generation, run}, nextId);
    if (added)
      ++nextId;
    return it->second;
  }

  static accesskit_tree_update *build(void *data) noexcept {
    auto &self = *static_cast<Impl *>(data);
    try {
      std::lock_guard lock{self.mutex};
      self.buildFailed = false;
      const auto &snapshot = self.snapshot;
      const bool enabled = self.mode != ui::AccessibilityMode::Disabled;
      std::unique_ptr<accesskit_tree_update,
                      decltype(&accesskit_tree_update_free)>
          ownedUpdate{accesskit_tree_update_with_focus(
                          enabled ? self.id(snapshot.focus) : 0),
                      accesskit_tree_update_free};
      auto *update = ownedUpdate.get();
      accesskit_tree_update_set_tree_info(update, accesskit_tree_info_new(0));
      using NativeNode =
          std::unique_ptr<accesskit_node, decltype(&accesskit_node_free)>;
      NativeNode ownedWindow{accesskit_node_new(ACCESSKIT_ROLE_WINDOW),
                             accesskit_node_free};
      auto *window = ownedWindow.get();
      accesskit_node_set_label(window, self.title.c_str());
      if (enabled)
        for (const auto &node : snapshot.nodes) {
          NativeNode ownedNode{
              accesskit_node_new(role(node.state.description.role)),
              accesskit_node_free};
          auto *native = ownedNode.get();
          const auto &s = node.state;
          accesskit_node_set_label(native, s.description.name.c_str());
          accesskit_node_set_description(native,
                                         s.description.description.c_str());
          if (!s.protectedText && s.description.value)
            accesskit_node_set_value(native, s.description.value->c_str());
          if (!s.description.enabled)
            accesskit_node_set_disabled(native);
          if (s.readOnly)
            accesskit_node_set_read_only(native);
          if (s.required)
            accesskit_node_set_required(native);
          if (s.invalid)
            accesskit_node_set_invalid(native, ACCESSKIT_INVALID_TRUE);
          if (s.expanded)
            accesskit_node_set_expanded(native, *s.expanded);
          if (s.activeDescendant)
            accesskit_node_set_active_descendant(native,
                                                 self.id(*s.activeDescendant));
          if (s.checked)
            accesskit_node_set_toggled(native,
                                       *s.checked == ui::CheckState::On
                                           ? ACCESSKIT_TOGGLED_TRUE
                                       : *s.checked == ui::CheckState::Mixed
                                           ? ACCESSKIT_TOGGLED_MIXED
                                           : ACCESSKIT_TOGGLED_FALSE);
          if (s.description.role == ui::SemanticRole::Option ||
              s.description.role == ui::SemanticRole::Tab)
            accesskit_node_set_selected(native, s.selected);
          if (s.range) {
            accesskit_node_set_numeric_value(native, s.range->value);
            accesskit_node_set_min_numeric_value(native, s.range->minimum);
            accesskit_node_set_max_numeric_value(native, s.range->maximum);
            accesskit_node_set_numeric_value_step(native, s.range->step);
          }
          if (s.description.role == ui::SemanticRole::Status)
            accesskit_node_set_live(native, ACCESSKIT_LIVE_POLITE);
          accesskit_node_set_bounds(
              native, {node.bounds.left(), node.bounds.top(),
                       node.bounds.right(), node.bounds.bottom()});
          for (auto a : s.actions)
            accesskit_node_add_action(native, action(a));
          for (const auto &a : s.customActions) {
            auto *custom = accesskit_custom_action_new(a.id);
            accesskit_custom_action_set_description(custom,
                                                    a.description.c_str());
            accesskit_node_push_custom_action(native, custom);
          }
          if (!s.customActions.empty())
            accesskit_node_add_action(native, ACCESSKIT_ACTION_CUSTOM_ACTION);
          if (!s.protectedText && s.selection) {
            for (std::size_t i = 0; i < s.textRuns.size(); ++i) {
              const auto &run = s.textRuns[i];
              NativeNode ownedText{accesskit_node_new(ACCESSKIT_ROLE_TEXT_RUN),
                                   accesskit_node_free};
              auto *text = ownedText.get();
              accesskit_node_set_value(text, run.text.c_str());
              accesskit_node_set_bounds(
                  text, {run.bounds.left(), run.bounds.top(),
                         run.bounds.right(), run.bounds.bottom()});
              accesskit_node_set_text_direction(
                  text, run.rtl ? ACCESSKIT_TEXT_DIRECTION_RIGHT_TO_LEFT
                                : ACCESSKIT_TEXT_DIRECTION_LEFT_TO_RIGHT);
              std::vector<std::uint8_t> lengths;
              for (std::size_t j = 1; j < run.byteOffsets.size(); ++j)
                lengths.push_back(
                    std::uint8_t(run.byteOffsets[j] - run.byteOffsets[j - 1]));
              accesskit_node_set_character_lengths(text, lengths.size(),
                                                   lengths.data());
              accesskit_node_set_character_positions(text, run.positions.size(),
                                                     run.positions.data());
              accesskit_node_set_character_widths(text, run.widths.size(),
                                                  run.widths.data());
              accesskit_node_push_child(native, self.id(node.id, i + 1));
              accesskit_tree_update_push_node(update, self.id(node.id, i + 1),
                                              ownedText.release());
            }
            const auto position = [&](std::size_t offset) {
              for (std::size_t i = 0; i < s.textRuns.size(); ++i) {
                const auto &offsets = s.textRuns[i].byteOffsets;
                const auto at =
                    std::find(offsets.begin(), offsets.end(), offset);
                if (at != offsets.end())
                  return accesskit_text_position{
                      self.id(node.id, i + 1),
                      std::size_t(at - offsets.begin())};
              }
              return accesskit_text_position{self.id(node.id), 0};
            };
            if (!s.textRuns.empty())
              accesskit_node_set_text_selection(native,
                                                {position(s.selection->anchor),
                                                 position(s.selection->caret)});
          }
          for (const auto &child : snapshot.nodes)
            if (child.parent == node.id)
              accesskit_node_push_child(native, self.id(child.id));
          if (node.parent == ui::NodeId{})
            accesskit_node_push_child(window, self.id(node.id));
          accesskit_tree_update_push_node(update, self.id(node.id),
                                          ownedNode.release());
        }
      accesskit_tree_update_push_node(update, 0, ownedWindow.release());
      return ownedUpdate.release();
    } catch (...) {
      {
        std::lock_guard lock{self.mutex};
        self.buildFailed = true;
      }
      auto *fallback = accesskit_tree_update_with_focus(0);
      accesskit_tree_update_set_tree_info(fallback, accesskit_tree_info_new(0));
      accesskit_tree_update_push_node(
          fallback, 0, accesskit_node_new(ACCESSKIT_ROLE_WINDOW));
      return fallback;
    }
  }

  static void request(accesskit_action_request *raw, void *data) noexcept {
    std::unique_ptr<accesskit_action_request,
                    decltype(&accesskit_action_request_free)>
        owned{raw, accesskit_action_request_free};
    auto &self = *static_cast<Impl *>(data);
    try {
      std::unique_lock lock{self.mutex};
      if (self.mode == ui::AccessibilityMode::Disabled ||
          self.requests.size() >= 256)
        return;
      ui::NodeId target;
      for (const auto &node : self.snapshot.nodes)
        if (self.id(node.id) == raw->target_node)
          target = node.id;
      if (target == ui::NodeId{})
        return;
      std::optional<ui::UIAction> value;
      switch (raw->action) {
      case ACCESSKIT_ACTION_CUSTOM_ACTION:
        if (raw->data.has_value &&
            raw->data.value.tag == ACCESSKIT_ACTION_DATA_CUSTOM_ACTION)
          value = ui::CustomAction{raw->data.value.custom_action};
        break;
      case ACCESSKIT_ACTION_CLICK:
        value = ui::Activate{};
        break;
      case ACCESSKIT_ACTION_FOCUS:
        value = ui::Focus{};
        break;
      case ACCESSKIT_ACTION_INCREMENT:
        value = ui::Increment{1};
        break;
      case ACCESSKIT_ACTION_DECREMENT:
        value = ui::Increment{-1};
        break;
      case ACCESSKIT_ACTION_EXPAND:
        value = ui::SetExpanded{true};
        break;
      case ACCESSKIT_ACTION_COLLAPSE:
        value = ui::SetExpanded{false};
        break;
      case ACCESSKIT_ACTION_SCROLL_INTO_VIEW:
        value = ui::ScrollIntoView{};
        break;
      case ACCESSKIT_ACTION_SET_VALUE:
        if (raw->data.has_value &&
            raw->data.value.tag == ACCESSKIT_ACTION_DATA_NUMERIC_VALUE)
          value = ui::SetValue{raw->data.value.numeric_value};
        break;
      case ACCESSKIT_ACTION_REPLACE_SELECTED_TEXT:
        if (raw->data.has_value &&
            raw->data.value.tag == ACCESSKIT_ACTION_DATA_VALUE &&
            raw->data.value.value)
          value = ui::ReplaceSelectedText{raw->data.value.value};
        break;
      case ACCESSKIT_ACTION_SET_TEXT_SELECTION:
        if (raw->data.has_value &&
            raw->data.value.tag == ACCESSKIT_ACTION_DATA_SET_TEXT_SELECTION) {
          const auto offset = [&](accesskit_text_position position)
              -> std::optional<std::size_t> {
            for (const auto &node : self.snapshot.nodes)
              if (node.id == target) {
                for (std::size_t i = 0; i < node.state.textRuns.size(); ++i) {
                  const auto &offsets = node.state.textRuns[i].byteOffsets;
                  if (self.id(target, i + 1) == position.node &&
                      position.character_index < offsets.size())
                    return offsets[position.character_index];
                }
              }
            return {};
          };
          const auto selection = raw->data.value.set_text_selection;
          const auto a = offset(selection.anchor), b = offset(selection.focus);
          if (a && b)
            value = ui::TextSelection{*a, *b};
        }
        break;
      default:
        break;
      }
      if (value) {
        self.requests.push_back(
            {self.snapshot.session, target, std::move(*value)});
        auto wake = self.wake;
        lock.unlock();
        if (wake)
          wake();
      }
    } catch (...) {
    }
  }

  void update() {
#if defined(_WIN32)
    if (auto *events = accesskit_windows_subclassing_adapter_update_if_active(
            adapter, build, this))
      accesskit_windows_queued_events_raise(events);
#elif defined(ACCESSKIT_MACOS)
    if (auto *events = accesskit_macos_subclassing_adapter_update_if_active(
            adapter, build, this))
      accesskit_macos_queued_events_raise(events);
    if (auto *events =
            accesskit_macos_subclassing_adapter_update_view_focus_state(
                adapter,
                (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0))
      accesskit_macos_queued_events_raise(events);
#elif defined(__linux__)
    accesskit_unix_adapter_update_if_active(adapter, build, this);
    accesskit_unix_adapter_update_window_focus_state(
        adapter, (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0);
#endif
  }

  ~Impl() {
#if defined(_WIN32)
    if (adapter)
      accesskit_windows_subclassing_adapter_free(adapter);
#elif defined(ACCESSKIT_MACOS)
    if (adapter)
      accesskit_macos_subclassing_adapter_free(adapter);
#elif defined(__linux__)
    if (adapter)
      accesskit_unix_adapter_free(adapter);
#endif
  }
};

WindowServices::WindowServices(SDL_Window *window)
    : _impl{std::make_shared<Impl>(window)} {
  auto *p = _impl.get();
  const auto props = SDL_GetWindowProperties(window);
#if defined(_WIN32)
  if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN))
    throw std::invalid_argument(
        "Attach native accessibility before showing the window");
  p->adapter = accesskit_windows_subclassing_adapter_new(
      static_cast<HWND>(SDL_GetPointerProperty(
          props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)),
      Impl::build, p, Impl::request, p);
#elif defined(ACCESSKIT_MACOS)
  auto *nativeWindow = SDL_GetPointerProperty(
      props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
  if (!nativeWindow)
    throw std::runtime_error(
        "Native macOS accessibility requires a Cocoa window");
  // SDL's private Objective-C class names can change between releases.
  // Resolve the class of this live window instead of assuming one by name.
  const auto windowClass = object_getClass(static_cast<id>(nativeWindow));
  static std::set<Class> patchedWindowClasses;
  if (patchedWindowClasses.insert(windowClass).second)
    accesskit_macos_add_focus_forwarder_to_window_class(
        class_getName(windowClass));
  p->adapter = accesskit_macos_subclassing_adapter_for_window(
      nativeWindow, Impl::build, p, Impl::request, p);
#elif defined(__linux__)
  p->adapter = accesskit_unix_adapter_new(
      Impl::build, p, Impl::request, p, [](void *) {}, p);
#endif
}

WindowServices::~WindowServices() { releaseRelativeMouse(); }

ui::Connection WindowServices::attach(ui::UIRoot &root) {
  root.services().readClipboard = [] {
    std::unique_ptr<char, decltype(&SDL_free)> text{SDL_GetClipboardText(),
                                                    SDL_free};
    if (!text)
      throwSDLError("Read clipboard");
    return std::string{text.get()};
  };
  root.services().writeClipboard = [](std::string_view text) {
    if (!SDL_SetClipboardText(std::string{text}.c_str()))
      throwSDLError("Write clipboard");
  };
  auto disconnect = support::MoveOnlyFunction<void() noexcept>{
      [weak = std::weak_ptr{_impl}, owner = &root]() noexcept {
        if (auto self = weak.lock()) {
          auto at = self->attachments.find(owner);
          if (at == self->attachments.end() || --at->second > 0)
            return;
          self->attachments.erase(at);
          if (self->root != owner)
            return;
          self->root = nullptr;
          try {
            {
              std::lock_guard lock{self->mutex};
              self->snapshot = {};
              self->requests.clear();
            }
            self->update();
          } catch (...) {
          }
          SDL_StopTextInput(self->window);
          self->textOptions.reset();
        }
      }};
  ++_impl->attachments[&root];
  _impl->root = &root;
  return ui::Connection{std::move(disconnect)};
}

void WindowServices::cancelInput() {
  releaseRelativeMouse();
  if (_impl->root) {
    ui::UIEvent cancel{.type = ui::EventType::InputCancel};
    _impl->root->dispatch(cancel);
  }
  SDL_ClearComposition(_impl->window);
  SDL_StopTextInput(_impl->window);
  _impl->composing = false;
  _impl->textOptions.reset();
}

ui::Connection
WindowServices::lockRelativeMouse(runtime::ActivationToken owner) {
  if (!owner.isActive())
    throw std::logic_error("Relative mouse requires an active app");
  if (!(SDL_GetWindowFlags(_impl->window) & SDL_WINDOW_INPUT_FOCUS))
    throw std::logic_error("Relative mouse requires native window focus");
  releaseRelativeMouse();
  if (!SDL_SetWindowRelativeMouseMode(_impl->window, true))
    throw std::runtime_error(std::string{"Cannot lock relative mouse: "} +
                             SDL_GetError());
  _impl->relativeOwner = owner;
  _impl->relative = true;
  const auto generation = ++_impl->relativeGeneration;
  return ui::Connection{[weak = std::weak_ptr{_impl}, generation]() noexcept {
    if (auto self = weak.lock();
        self && self->relativeGeneration == generation) {
      SDL_SetWindowRelativeMouseMode(self->window, false);
      self->relative = false;
      ++self->relativeGeneration;
    }
  }};
}

void WindowServices::releaseRelativeMouse() noexcept {
  if (_impl->relative)
    SDL_SetWindowRelativeMouseMode(_impl->window, false);
  _impl->relative = false;
  ++_impl->relativeGeneration;
}

bool WindowServices::focused() const noexcept {
  return SDL_GetWindowFlags(_impl->window) & SDL_WINDOW_INPUT_FOCUS;
}

bool WindowServices::relativeMouseActive() const noexcept {
  return _impl->relative && _impl->relativeOwner.isActive() &&
         SDL_GetWindowRelativeMouseMode(_impl->window);
}

void WindowServices::setMode(ui::AccessibilityMode value) {
  ui::InteractionProps{.accessibility = value}.validate();
  {
    std::lock_guard lock{_impl->mutex};
    if (_impl->mode != value) {
      _impl->mode = value;
      _impl->publicationDirty = true;
    }
  }
}

void WindowServices::publish(ui::UIRoot &root,
                             const platform::ViewportMapping &mapping) {
  if (!_impl->attachments.contains(&root))
    throw std::logic_error("Publishing UI requires a live window attachment");
  _impl->root = &root;
  auto timing = root.publicationScope();
#if defined(__linux__)
  int x{}, y{}, w{}, h{}, top{}, left{}, bottom{}, right{};
  SDL_GetWindowPosition(_impl->window, &x, &y);
  SDL_GetWindowSize(_impl->window, &w, &h);
  SDL_GetWindowBordersSize(_impl->window, &top, &left, &bottom, &right);
  accesskit_unix_adapter_set_root_window_bounds(
      _impl->adapter,
      {double(x - left), double(y - top), double(x + w + right),
       double(y + h + bottom)},
      {double(x), double(y), double(x + w), double(y + h)});
#endif
  auto *focused = root.resolve(root.focusedNode());
  auto *client = dynamic_cast<ui::TextInputClient *>(focused);
  const bool windowFocused =
      (SDL_GetWindowFlags(_impl->window) & SDL_WINDOW_INPUT_FOCUS) != 0;
  if (client && focused->isInteractionEnabled() && windowFocused &&
      !client->textInputState().readOnly) {
    const auto state = client->textInputState();
    if (_impl->composing && !state.composing)
      SDL_ClearComposition(_impl->window);
    _impl->composing = state.composing;
    const auto options = std::pair{state.multiline, state.password};
    if (_impl->textOptions != options || _impl->textOwner != focused->id() ||
        _impl->textSession != root.workSample().root) {
      SDL_StopTextInput(_impl->window);
      const auto props = SDL_CreateProperties();
      if (!props)
        throwSDLError("Create text input properties");
      const bool configured =
          SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER,
                                state.password
                                    ? SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN
                                    : SDL_TEXTINPUT_TYPE_TEXT) &&
          SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN,
                                 state.multiline) &&
          SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN,
                                 !state.password);
      const bool started =
          configured && SDL_StartTextInputWithProperties(_impl->window, props);
      SDL_DestroyProperties(props);
      if (!started)
        throwSDLError("Start text input");
      _impl->textOptions = options;
      _impl->textOwner = focused->id();
      _impl->textSession = root.workSample().root;
    }
    const auto caret = focused->worldTransform().mapBounds(state.caret);
    SDL_Rect area{
        int(std::floor(caret.left() * mapping.windowUnitsPerLogical.x +
                       mapping.offset.x)),
        int(std::floor(caret.top() * mapping.windowUnitsPerLogical.y +
                       mapping.offset.y)),
        std::max(1,
                 int(std::ceil(caret.w() * mapping.windowUnitsPerLogical.x))),
        std::max(1,
                 int(std::ceil(caret.h() * mapping.windowUnitsPerLogical.y)))};
    if (!SDL_SetTextInputArea(_impl->window, &area, 0))
      throwSDLError("Set IME caret area");
  } else if (_impl->textOptions) {
    SDL_StopTextInput(_impl->window);
    _impl->textOptions.reset();
    _impl->composing = false;
  }
  const auto scale = mapping.windowUnitsPerLogical;
  math::Vec2f nativeScale{1, 1};
#if defined(_WIN32)
  int w{}, h{}, pw{}, ph{};
  SDL_GetWindowSize(_impl->window, &w, &h);
  SDL_GetWindowSizeInPixels(_impl->window, &pw, &ph);
  if (w && h)
    nativeScale = {float(pw) / w, float(ph) / h};
#endif
  const std::string title = SDL_GetWindowTitle(_impl->window);
  {
    std::lock_guard lock{_impl->mutex};
    if (!_impl->publicationDirty &&
        _impl->publishedKey == root.publicationKey() &&
        _impl->publishedMapping == mapping &&
        _impl->publishedNativeScale == nativeScale && _impl->title == title &&
        _impl->windowFocused == windowFocused) {
      root.recordPublication(true);
      return;
    }
  }
  auto next = root.semanticSnapshot();
  root.recordPublication(false);
  const auto publishedKey = root.publicationKey();
  for (auto &node : next.nodes) {
    node.bounds.position = {
        (node.bounds.position.x * scale.x + mapping.offset.x) * nativeScale.x,
        (node.bounds.position.y * scale.y + mapping.offset.y) * nativeScale.y};
    node.bounds.size = {node.bounds.w() * scale.x * nativeScale.x,
                        node.bounds.h() * scale.y * nativeScale.y};
    auto &s = node.state;
    if (s.selection && !s.protectedText) {
      // Empty text and hard line breaks still need addressable text positions.
      const auto &value = s.description.value.value_or("");
      for (std::size_t at = 0; at < value.size(); ++at)
        if (value[at] == '\n')
          s.textRuns.push_back(
              {"\n", {at, at + 1}, node.bounds, {0}, {0}, false});
      if (s.textRuns.empty())
        s.textRuns.push_back({value, {0}, node.bounds, {}, {}, false});
      std::sort(s.textRuns.begin(), s.textRuns.end(),
                [](const auto &a, const auto &b) {
                  return a.byteOffsets.front() < b.byteOffsets.front();
                });
    }
    for (auto &run : s.textRuns) {
      ui::textBoundaries(run.text, ui::TextBoundary::Grapheme);
      if (!math::isFinite(run.bounds) || run.bounds.w() < 0 ||
          run.bounds.h() < 0 ||
          !std::all_of(run.positions.begin(), run.positions.end(),
                       [](float p) { return std::isfinite(p); }) ||
          !std::all_of(run.widths.begin(), run.widths.end(),
                       [](float w) { return std::isfinite(w) && w >= 0; }))
        throw std::invalid_argument("Invalid accessible text geometry");
      if (run.byteOffsets.empty() ||
          run.positions.size() + 1 != run.byteOffsets.size() ||
          run.widths.size() != run.positions.size() ||
          !std::is_sorted(run.byteOffsets.begin(), run.byteOffsets.end()) ||
          run.byteOffsets.back() - run.byteOffsets.front() != run.text.size())
        throw std::invalid_argument("Invalid accessible text run");
      // AccessKit lengths are bytes in a u8. Split pathological long clusters
      // into scalar-sized entries; the editor still rejects non-grapheme edits.
      std::vector<std::size_t> offsets;
      std::vector<float> positions, widths;
      std::size_t local{};
      for (std::size_t j = 1; j < run.byteOffsets.size(); ++j) {
        const auto length = run.byteOffsets[j] - run.byteOffsets[j - 1];
        if (length <= 255) {
          offsets.push_back(run.byteOffsets[j - 1]);
          positions.push_back(run.positions[j - 1]);
          widths.push_back(run.widths[j - 1]);
        } else
          for (std::size_t k = 0; k < length;) {
            offsets.push_back(run.byteOffsets[j - 1] + k);
            positions.push_back(run.positions[j - 1]);
            widths.push_back(run.widths[j - 1]);
            const auto c = static_cast<unsigned char>(run.text[local + k]);
            k += c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
          }
        local += length;
      }
      offsets.push_back(run.byteOffsets.back());
      run.byteOffsets = std::move(offsets);
      run.positions = std::move(positions);
      run.widths = std::move(widths);
      if (run.bounds != node.bounds) {
        run.bounds.position = {
            (run.bounds.left() * scale.x + mapping.offset.x) * nativeScale.x,
            (run.bounds.top() * scale.y + mapping.offset.y) * nativeScale.y};
        run.bounds.size = {run.bounds.w() * scale.x * nativeScale.x,
                           run.bounds.h() * scale.y * nativeScale.y};
      }
      for (auto &p : run.positions)
        p *= scale.x * nativeScale.x;
      for (auto &w : run.widths)
        w *= scale.x * nativeScale.x;
    }
  }
  {
    std::lock_guard lock{_impl->mutex};
    for (const auto &node : next.nodes) {
      const auto &s = node.state;
      ui::textBoundaries(s.description.name, ui::TextBoundary::Grapheme);
      ui::textBoundaries(s.description.description, ui::TextBoundary::Grapheme);
      if (s.description.value)
        ui::textBoundaries(*s.description.value, ui::TextBoundary::Grapheme);
      for (const auto &a : s.customActions)
        ui::textBoundaries(a.description, ui::TextBoundary::Grapheme);
    }
    _impl->snapshot = std::move(next);
    _impl->publicationDirty = true;
    _impl->publishedKey = publishedKey;
    _impl->publishedMapping = mapping;
    _impl->publishedNativeScale = nativeScale;
    _impl->title = title;
    _impl->windowFocused = windowFocused;
    std::erase_if(_impl->ids, [&](const auto &entry) {
      return std::get<0>(entry.first) != _impl->snapshot.session ||
             std::none_of(_impl->snapshot.nodes.begin(),
                          _impl->snapshot.nodes.end(), [&](const auto &node) {
                            return node.id.index == std::get<1>(entry.first) &&
                                   node.id.generation ==
                                       std::get<2>(entry.first) &&
                                   std::get<3>(entry.first) <=
                                       node.state.textRuns.size();
                          });
    });
  }
  _impl->update();
  {
    std::lock_guard lock{_impl->mutex};
    _impl->publicationDirty = _impl->buildFailed;
  }
}

void WindowServices::setWakeCallback(std::function<void()> callback) {
  std::lock_guard lock{_impl->mutex};
  _impl->wake = std::move(callback);
}

void WindowServices::pump() {
  if (_impl->relative &&
      (!_impl->relativeOwner.isActive() ||
       !(SDL_GetWindowFlags(_impl->window) & SDL_WINDOW_INPUT_FOCUS)))
    releaseRelativeMouse();
  std::deque<Impl::Request> requests;
  {
    std::lock_guard lock{_impl->mutex};
    requests.swap(_impl->requests);
  }
  for (auto &request : requests) {
    auto *root = _impl->root;
    if (root && root->workSample().root == request.session) {
      try {
        root->performAction(request.target, request.action,
                            ui::ActionSource::Assistive);
      } catch (const std::exception &error) {
        if (root->services().diagnostic)
          root->services().diagnostic(error.what());
      }
    }
  }
}
} // namespace playground::sdl
