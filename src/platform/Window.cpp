#include <format>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>

#include <platform/Window.hpp>
#include <support/SDLError.hpp>

Window::Window(const WindowConfig &config)
    : _window{createWindow(this, config)},
      _state{.title = config.title,
             .windowedSize = config.windowedSize,
             .windowedPosition = config.windowedPosition,
             .resizable = config.resizable,
             .fullscreen = config.fullscreen,
             .borderless = config.borderless,
             .alwaysOnTop = config.alwaysOnTop,
             .focusable = config.focusable,
             .highPixelDensity = config.highPixelDensity,
             .hidden = config.hidden,
             .maximized = config.maximized,
             .minimized = config.minimized,
             .transparent = config.transparent,
             .mouseGrabbed = config.mouseGrabbed,
             .display = SDL_GetDisplayForWindow(_window.get())} {
  if (!_window) {
    throwSDLError(
        std::format("Window@{} Failed to create SDL_Window", (void *)this));
  }
  // A maximized or minimized window reports its current display state here,
  // not necessarily the size and position it will restore to later.
  if (!config.fullscreen && !config.maximized && !config.minimized) {
    int x{};
    int y{};
    if (!SDL_GetWindowPosition(_window.get(), &x, &y))
      throwSDLError(std::format("Window@{} Failed to query window position",
                                (void *)this));
    _state.windowedPosition = playground::math::Vec2i{x, y};

    int width{};
    int height{};
    if (!SDL_GetWindowSize(_window.get(), &width, &height))
      throwSDLError(
          std::format("Window@{} Failed to query window size", (void *)this));
    _state.windowedSize = playground::math::Vec2i{width, height};
  }
  refreshState();
}

playground::math::Vec2i Window::getWindowSize() const {
  playground::math::Vec2i size;
  if (!SDL_GetWindowSize(_window.get(), &size.x, &size.y))
    throwSDLError("Failed to query window coordinate size");
  return size;
}

void Window::setTitle(const std::string &title) {
  if (title == _state.title)
    return;

  if (!SDL_SetWindowTitle(_window.get(), title.c_str()))
    throwSDLError(
        std::format("Window@{} Failed to set window title", (void *)this));

  _state.title = title;
}

void Window::setWindowedPosition(playground::math::Vec2i position) {
  if (!SDL_SetWindowPosition(_window.get(), position.x, position.y))
    throwSDLError(
        std::format("Window@{} Failed to set window position", (void *)this));

  refreshState();
}

void Window::setWindowedSize(playground::math::Vec2i size) {
  if (!hasArea(size))
    return;

  if (size == getWindowSize())
    return;

  if (!SDL_SetWindowSize(_window.get(), size.x, size.y))
    throwSDLError(
        std::format("Window@{} Failed to set window size", (void *)this));

  refreshState();
}

void Window::setResizable(bool resizable) {
  if (resizable == _state.resizable)
    return;

  if (!SDL_SetWindowResizable(_window.get(), resizable))
    throwSDLError(
        std::format("Window@{} Failed to set window resizable", (void *)this));

  refreshState();
}

void Window::setBorderless(bool borderless) {
  if (borderless == _state.borderless)
    return;
  if (!SDL_SetWindowBordered(_window.get(), !borderless))
    throwSDLError(
        std::format("Window@{} Failed to set window borderless", (void *)this));
  refreshState();
}

void Window::setAlwaysOnTop(bool alwaysOnTop) {
  if (alwaysOnTop == _state.alwaysOnTop)
    return;
  if (!SDL_SetWindowAlwaysOnTop(_window.get(), alwaysOnTop))
    throwSDLError(
        std::format("Window@{} Failed to set always-on-top", (void *)this));
  refreshState();
}

void Window::setFocusable(bool focusable) {
  if (focusable == _state.focusable)
    return;
  if (!SDL_SetWindowFocusable(_window.get(), focusable))
    throwSDLError(
        std::format("Window@{} Failed to set focusable", (void *)this));
  refreshState();
}

void Window::setMouseGrabbed(bool grabbed) {
  if (grabbed == _state.mouseGrabbed)
    return;
  if (!SDL_SetWindowMouseGrab(_window.get(), grabbed))
    throwSDLError(
        std::format("Window@{} Failed to set mouse grab", (void *)this));
  refreshState();
}

void Window::setHidden(bool hidden) {
  if (hidden == _state.hidden)
    return;
  const bool success =
      hidden ? SDL_HideWindow(_window.get()) : SDL_ShowWindow(_window.get());
  if (!success)
    throwSDLError(
        std::format("Window@{} Failed to set hidden state", (void *)this));
  refreshState();
}

void Window::setMaximized(bool maximized) {
  const bool success = maximized ? SDL_MaximizeWindow(_window.get())
                                 : SDL_RestoreWindow(_window.get());
  if (!success)
    throwSDLError(
        std::format("Window@{} Failed to set maximized state", (void *)this));

  refreshState();
}

void Window::setMinimized(bool minimized) {
  const bool success = minimized ? SDL_MinimizeWindow(_window.get())
                                 : SDL_RestoreWindow(_window.get());
  if (!success)
    throwSDLError(
        std::format("Window@{} Failed to set minimized state", (void *)this));

  refreshState();
}

void Window::setFullscreen(bool fullscreen) {
  if (!SDL_SetWindowFullscreen(_window.get(), fullscreen))
    throwSDLError(
        std::format("Window@{} Failed to set window fullscreen", (void *)this));

  refreshState();
}

void Window::handleEvent(const SDL_Event &event) {
  if (event.type >= SDL_EVENT_WINDOW_FIRST &&
      event.type <= SDL_EVENT_WINDOW_LAST &&
      event.window.windowID == SDL_GetWindowID(_window.get()))
    refreshState();
  if (event.type == SDL_EVENT_DISPLAY_ADDED ||
      event.type == SDL_EVENT_DISPLAY_REMOVED ||
      event.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED)
    refreshState();
}

void Window::refreshState() {
  const auto flags = SDL_GetWindowFlags(_window.get());
  _state.title = SDL_GetWindowTitle(_window.get());
  _state.resizable = (flags & SDL_WINDOW_RESIZABLE) != 0;
  _state.fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
  _state.borderless = (flags & SDL_WINDOW_BORDERLESS) != 0;
  _state.alwaysOnTop = (flags & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
  _state.focusable = (flags & SDL_WINDOW_NOT_FOCUSABLE) == 0;
  _state.highPixelDensity = (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY) != 0;
  _state.hidden = (flags & SDL_WINDOW_HIDDEN) != 0;
  _state.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
  _state.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
  _state.transparent = (flags & SDL_WINDOW_TRANSPARENT) != 0;
  _state.mouseGrabbed = (flags & SDL_WINDOW_MOUSE_GRABBED) != 0;
  if (!SDL_GetWindowSize(_window.get(), &_state.actualSize.x,
                         &_state.actualSize.y) ||
      !SDL_GetWindowSizeInPixels(_window.get(), &_state.drawableSize.x,
                                 &_state.drawableSize.y) ||
      !SDL_GetWindowPosition(_window.get(), &_state.actualPosition.x,
                             &_state.actualPosition.y))
    throwSDLError("Failed to query window geometry");
  _state.display = SDL_GetDisplayForWindow(_window.get());
  _state.displayScale = SDL_GetWindowDisplayScale(_window.get());
  if (_state.displayScale <= 0)
    throwSDLError("Failed to query window display scale");
  if (!_applyingPreferences &&
      _requestedMode == playground::platform::WindowMode::Windowed &&
      !_state.fullscreen && !_state.maximized && !_state.minimized) {
    _state.windowedSize = _state.actualSize;
    _state.windowedPosition = _state.actualPosition;
  }
}

playground::platform::WindowMetrics Window::metrics() const {
  playground::platform::WindowMetrics result{
      getWindowSize(), {}, SDL_GetWindowDisplayScale(_window.get())};
  if (!SDL_GetWindowSizeInPixels(_window.get(), &result.drawableSize.x,
                                 &result.drawableSize.y) ||
      result.displayScale <= 0)
    throwSDLError("Failed to query window metrics");
  return result;
}

SDL_DisplayID Window::resolveDisplay(
    const playground::platform::DisplayPreference &preference) const {
  using playground::platform::DisplaySelection;
  if (preference.selection == DisplaySelection::Current) {
    if (auto current = SDL_GetDisplayForWindow(_window.get()))
      return current;
  }
  if (preference.selection == DisplaySelection::Named) {
    int count{};
    auto *displays = SDL_GetDisplays(&count);
    if (!displays)
      throwSDLError("Failed to enumerate displays");
    struct Guard {
      SDL_DisplayID *values;
      ~Guard() { SDL_free(values); }
    } guard{displays};
    for (int i = 0; i < count; ++i)
      if (const char *name = SDL_GetDisplayName(displays[i]);
          name && preference.name == name)
        return displays[i];
  }
  const auto primary = SDL_GetPrimaryDisplay();
  if (!primary)
    throwSDLError("No display is available");
  return primary;
}

playground::math::Rect Window::usableBounds(
    const playground::platform::DisplayPreference &preference) const {
  SDL_Rect bounds{};
  const auto display = resolveDisplay(preference);
  if (!SDL_GetDisplayUsableBounds(display, &bounds) &&
      !SDL_GetDisplayBounds(display, &bounds))
    throwSDLError("Failed to query display bounds");
  return {{static_cast<float>(bounds.x), static_cast<float>(bounds.y)},
          {static_cast<float>(bounds.w), static_cast<float>(bounds.h)}};
}

void Window::setMinimumSize(playground::math::Vec2i size) {
  if (!SDL_SetWindowMinimumSize(_window.get(), size.x, size.y))
    throwSDLError("Failed to set window minimum size");
}

void Window::applyPreferences(
    const playground::platform::WindowPreferences &preferences) {
  using playground::platform::WindowMode;
  preferences.validate();
  const auto display = resolveDisplay(preferences.display);
  refreshState();
  const auto previousMode = _requestedMode;
  const auto normalSize = _state.windowedSize;
  const auto normalPosition = _state.windowedPosition;
  _applyingPreferences = true;
  struct TransitionGuard {
    bool &active;
    ~TransitionGuard() { active = false; }
  } transition{_applyingPreferences};
  _requestedMode = preferences.mode;
  if (_state.fullscreen || previousMode == WindowMode::DesktopFullscreen ||
      previousMode == WindowMode::ExclusiveFullscreen)
    setFullscreen(false);
  if ((_state.maximized || _state.minimized ||
       previousMode == WindowMode::Maximized) &&
      !SDL_RestoreWindow(_window.get()))
    throwSDLError("Failed to restore window before presentation transition");
  if (!SDL_SetWindowFullscreenMode(_window.get(), nullptr))
    throwSDLError("Failed to clear exclusive fullscreen mode");
  setBorderless(!preferences.decorated);
  if (previousMode != WindowMode::Windowed) {
    setWindowedSize(normalSize);
    setWindowedPosition(normalPosition);
  }
  if (preferences.center || preferences.display.selection !=
                                playground::platform::DisplaySelection::Current)
    setWindowedPosition(
        {static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display)),
         static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display))});
  switch (preferences.mode) {
  case WindowMode::Windowed:
    break;
  case WindowMode::Maximized:
    setMaximized(true);
    break;
  case WindowMode::DesktopFullscreen:
    setFullscreen(true);
    break;
  case WindowMode::ExclusiveFullscreen: {
    SDL_DisplayMode mode{};
    if (!SDL_GetClosestFullscreenDisplayMode(
            display, preferences.exclusiveSize.x, preferences.exclusiveSize.y,
            preferences.refreshRate, true, &mode))
      throwSDLError("No compatible exclusive fullscreen display mode");
    if (!SDL_SetWindowFullscreenMode(_window.get(), &mode))
      throwSDLError("Failed to select exclusive fullscreen display mode");
    setFullscreen(true);
    break;
  }
  case WindowMode::BorderlessDisplay:
  case WindowMode::BorderlessWorkArea: {
    SDL_Rect bounds{};
    const bool queried = preferences.mode == WindowMode::BorderlessWorkArea
                             ? SDL_GetDisplayUsableBounds(display, &bounds)
                             : SDL_GetDisplayBounds(display, &bounds);
    if (!queried)
      throwSDLError("Failed to query borderless window bounds");
    setBorderless(true);
    setWindowedSize({bounds.w, bounds.h});
    setWindowedPosition({bounds.x, bounds.y});
    break;
  }
  }
  _applyingPreferences = false;
  refreshState();
}

WindowResource Window::createWindow(void *owner, const WindowConfig &config) {
  if (!SDL_WasInit(SDL_INIT_VIDEO))
    throwSDLError(std::format(
        "Window@{} Failed to create window: SDL video not initialized", owner));

  SDL_PropertiesID properties{SDL_CreateProperties()};
  if (!properties)
    throwSDLError(
        std::format("Window@{} Failed to create SDL properties", owner));

  struct PropertiesGuard {
    SDL_PropertiesID value;
    ~PropertiesGuard() { SDL_DestroyProperties(value); }
  } propertiesGuard{properties};

  auto setBoolean = [&](const char *name, bool value) {
    if (!SDL_SetBooleanProperty(properties, name, value))
      throwSDLError(std::format("Window@{} Failed to set window property {}",
                                owner, name));
  };
  auto setNumber = [&](const char *name, Sint64 value) {
    if (!SDL_SetNumberProperty(properties, name, value))
      throwSDLError(std::format("Window@{} Failed to set window property {}",
                                owner, name));
  };

  if (!SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                             config.title.c_str()))
    throwSDLError(
        std::format("Window@{} Failed to set window title property", owner));

  setNumber(SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, config.windowedSize.x);
  setNumber(SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, config.windowedSize.y);
  setNumber(SDL_PROP_WINDOW_CREATE_X_NUMBER, config.windowedPosition.x);
  setNumber(SDL_PROP_WINDOW_CREATE_Y_NUMBER, config.windowedPosition.y);
  setBoolean(SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, config.resizable);
  setBoolean(SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, config.fullscreen);
  setBoolean(SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, config.borderless);
  setBoolean(SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, config.alwaysOnTop);
  setBoolean(SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN, config.focusable);
  setBoolean(SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN,
             config.highPixelDensity);
  setBoolean(SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, config.hidden);
  setBoolean(SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, config.maximized);
  setBoolean(SDL_PROP_WINDOW_CREATE_MINIMIZED_BOOLEAN, config.minimized);
  setBoolean(SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, config.transparent);
  setBoolean(SDL_PROP_WINDOW_CREATE_MOUSE_GRABBED_BOOLEAN, config.mouseGrabbed);

  WindowResource window{SDL_CreateWindowWithProperties(properties)};
  if (!window)
    throwSDLError(std::format("Window@{} Failed to create window", owner));
  return window;
}
