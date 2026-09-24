#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <toml++/toml.hpp>

#include <platform/Settings.hpp>

namespace playground::platform {
namespace {
constexpr std::array modes{"windowed",           "maximized",
                           "desktop-fullscreen", "exclusive-fullscreen",
                           "borderless-display", "borderless-work-area"};
constexpr std::array displays{"primary", "current", "named"};
constexpr std::array sizing{"preferred", "fit-content", "restore-previous"};
constexpr std::array viewports{"reflow", "fixed-canvas"};
constexpr std::array fits{"contain", "cover", "stretch"};
constexpr std::array renderers{"auto", "software", "sdl-gpu"};
constexpr std::array gpuDrivers{"auto", "vulkan"};

template <class T> T value(const toml::node &node) {
  if constexpr (std::is_same_v<T, bool>) {
    if (!node.is_boolean())
      throw std::invalid_argument("Settings boolean must be true or false");
  }
  auto result = node.value<T>();
  if (!result)
    throw std::invalid_argument("Settings field has the wrong type");
  return *result;
}
template <class E, std::size_t N>
E enumeration(const toml::node &node,
              const std::array<const char *, N> &names) {
  const auto name = value<std::string>(node);
  for (std::size_t i = 0; i < N; ++i)
    if (name == names[i])
      return static_cast<E>(i);
  throw std::invalid_argument("Unknown settings enum value: " + name);
}
template <class E, std::size_t N>
const char *enumName(E entry, const std::array<const char *, N> &names) {
  const auto index = static_cast<std::size_t>(entry);
  if (index >= N)
    throw std::invalid_argument("Unknown settings enum");
  return names[index];
}
float number(const toml::node &node) {
  const double result = value<double>(node);
  if (!std::isfinite(result) ||
      std::abs(result) > std::numeric_limits<float>::max())
    throw std::invalid_argument("Nonfinite/out-of-range settings number");
  return static_cast<float>(result);
}
math::Vec2f pair(const toml::node &node) {
  const auto *array = node.as_array();
  if (!array || array->size() != 2)
    throw std::invalid_argument("Expected a two-element settings array");
  return {number(*array->get(0)), number(*array->get(1))};
}
math::Vec2i integers(const toml::node &node) {
  const auto *array = node.as_array();
  if (!array || array->size() != 2)
    throw std::invalid_argument("Expected a two-element integer array");
  const auto axis = [](const toml::node &item) {
    if (!item.is_integer())
      throw std::invalid_argument("Expected integer coordinate");
    const auto result = value<std::int64_t>(item);
    if (result < std::numeric_limits<int>::min() ||
        result > std::numeric_limits<int>::max())
      throw std::invalid_argument("Integer coordinate overflow");
    return static_cast<int>(result);
  };
  return {axis(*array->get(0)), axis(*array->get(1))};
}
const toml::table &table(const toml::node &node) {
  auto *result = node.as_table();
  if (!result)
    throw std::invalid_argument("Expected a settings table");
  return *result;
}
toml::table document(std::string_view text, int maximumVersion = 1) {
  auto result = toml::parse(text);
  const auto *version = result.get("schema_version");
  if (!version || !version->is_integer() || value<std::int64_t>(*version) < 1 ||
      value<std::int64_t>(*version) > maximumVersion)
    throw std::invalid_argument(
        "Unsupported or missing settings schema_version");
  return result;
}
SettingsPatch readPatch(const toml::table &fields) {
  SettingsPatch p;
  for (const auto &[key, node] : fields) {
    const auto name = key.str();
    if (name == "mode")
      p.mode = enumeration<WindowMode>(node, modes);
    else if (name == "display")
      p.display = enumeration<DisplaySelection>(node, displays);
    else if (name == "display_name")
      p.displayName = value<std::string>(node);
    else if (name == "decorated")
      p.decorated = value<bool>(node);
    else if (name == "center")
      p.center = value<bool>(node);
    else if (name == "exclusive_size")
      p.exclusiveSize = integers(node);
    else if (name == "refresh_rate")
      p.refreshRate = number(node);
    else if (name == "initial_sizing")
      p.initialSizing = enumeration<InitialWindowSizing>(node, sizing);
    else if (name == "viewport_mode")
      p.viewportMode = enumeration<ViewportMode>(node, viewports);
    else if (name == "viewport_fit")
      p.viewportFit = enumeration<ViewportFit>(node, fits);
    else if (name == "canvas_size")
      p.canvasSize = math::toSize(pair(node));
    else if (name == "alignment")
      p.alignment = pair(node);
    else if (name == "ui_scale")
      p.uiScale = number(node);
    else if (name == "follow_system_scale")
      p.followSystemScale = value<bool>(node);
    else if (name == "resolution_scale")
      p.resolutionScale = number(node);
    else if (name == "glyph_atlases")
      p.glyphAtlases = value<bool>(node);
    else if (name == "vsync")
      p.vsync = value<bool>(node);
    else if (name == "renderer")
      p.renderer = enumeration<rendering::RendererChoice>(node, renderers);
    else if (name == "gpu_driver")
      p.gpuDriver = enumeration<rendering::GPUDriver>(node, gpuDrivers);
    else if (name == "renderer_fallback")
      p.rendererFallback = value<bool>(node);
    else
      throw std::invalid_argument("Unknown settings field: " +
                                  std::string{name});
  }
  // Validate numeric ranges independently of cross-layer display-name
  // resolution.
  PresentationProps test;
  AppViewPolicy policy;
  p.apply(test, policy);
  if (test.window.display.selection == DisplaySelection::Named &&
      test.window.display.name.empty())
    test.window.display.name = "inherited";
  test.validate();
  policy.validate();
  return p;
}
toml::table writePatch(const SettingsPatch &p) {
  toml::table t;
  if (p.mode)
    t.insert("mode", enumName(*p.mode, modes));
  if (p.display)
    t.insert("display", enumName(*p.display, displays));
  if (p.displayName)
    t.insert("display_name", *p.displayName);
  if (p.decorated)
    t.insert("decorated", *p.decorated);
  if (p.center)
    t.insert("center", *p.center);
  if (p.exclusiveSize)
    t.insert("exclusive_size",
             toml::array{p.exclusiveSize->x, p.exclusiveSize->y});
  if (p.refreshRate)
    t.insert("refresh_rate", *p.refreshRate);
  if (p.initialSizing)
    t.insert("initial_sizing", enumName(*p.initialSizing, sizing));
  if (p.viewportMode)
    t.insert("viewport_mode", enumName(*p.viewportMode, viewports));
  if (p.viewportFit)
    t.insert("viewport_fit", enumName(*p.viewportFit, fits));
  if (p.canvasSize)
    t.insert("canvas_size",
             toml::array{p.canvasSize->width, p.canvasSize->height});
  if (p.alignment)
    t.insert("alignment", toml::array{p.alignment->x, p.alignment->y});
  if (p.uiScale)
    t.insert("ui_scale", *p.uiScale);
  if (p.followSystemScale)
    t.insert("follow_system_scale", *p.followSystemScale);
  if (p.resolutionScale)
    t.insert("resolution_scale", *p.resolutionScale);
  if (p.glyphAtlases)
    t.insert("glyph_atlases", *p.glyphAtlases);
  if (p.vsync)
    t.insert("vsync", *p.vsync);
  if (p.renderer)
    t.insert("renderer", enumName(*p.renderer, renderers));
  if (p.gpuDriver)
    t.insert("gpu_driver", enumName(*p.gpuDriver, gpuDrivers));
  if (p.rendererFallback)
    t.insert("renderer_fallback", *p.rendererFallback);
  return t;
}
std::string format(const toml::table &t) {
  std::ostringstream out;
  out << t;
  return out.str();
}
} // namespace

void SettingsPatch::apply(PresentationProps &p, AppViewPolicy &v) const {
  if (mode)
    p.window.mode = *mode;
  if (display)
    p.window.display.selection = *display;
  if (displayName)
    p.window.display.name = *displayName;
  if (decorated)
    p.window.decorated = *decorated;
  if (center)
    p.window.center = *center;
  if (exclusiveSize)
    p.window.exclusiveSize = *exclusiveSize;
  if (refreshRate)
    p.window.refreshRate = *refreshRate;
  if (initialSizing)
    v.initialSizing = *initialSizing;
  if (viewportMode)
    p.viewport.mode = *viewportMode;
  if (viewportFit)
    p.viewport.fit = *viewportFit;
  if (canvasSize)
    p.viewport.canvasSize = *canvasSize;
  if (alignment)
    p.viewport.alignment = *alignment;
  if (uiScale)
    p.viewport.scale = *uiScale;
  if (followSystemScale)
    p.viewport.followSystemScale = *followSystemScale;
  if (resolutionScale)
    p.render.resolutionScale = *resolutionScale;
  if (glyphAtlases)
    p.render.glyphAtlases = *glyphAtlases;
  if (vsync)
    p.render.vsync = *vsync;
  if (renderer)
    p.renderer.backend = *renderer;
  if (gpuDriver)
    p.renderer.driver = *gpuDriver;
  if (rendererFallback)
    p.renderer.allowFallback = *rendererFallback;
}
void SettingsDocument::apply(std::string_view app, PresentationProps &p,
                             AppViewPolicy &v) const {
  defaults.apply(p, v);
  if (const auto found = apps.find(app); found != apps.end())
    found->second.apply(p, v);
}
SettingsDocument parseSettings(std::string_view text) {
  const auto root = document(text, 2);
  SettingsDocument result;
  for (const auto &[key, node] : root) {
    if (key == "schema_version")
      continue;
    if (key == "defaults")
      result.defaults = readPatch(table(node));
    else if (key == "apps") {
      for (const auto &[app, props] : table(node))
        result.apps.emplace(std::string{app.str()}, readPatch(table(props)));
    } else
      throw std::invalid_argument("Unknown settings document section");
  }
  return result;
}
std::string serializeSettings(const SettingsDocument &document) {
  toml::table apps;
  for (const auto &[key, patch] : document.apps)
    apps.insert(key, writePatch(patch));
  return format(toml::table{{"schema_version", 2},
                            {"defaults", writePatch(document.defaults)},
                            {"apps", std::move(apps)}});
}
SessionState parseSession(std::string_view text) {
  const auto root = document(text);
  for (const auto &[key, node] : root)
    if (key != "schema_version" && key != "apps")
      throw std::invalid_argument("Unknown session section");
  SessionState result;
  if (const auto *apps = root.get("apps")) {
    for (const auto &[app, node] : table(*apps)) {
      const auto &fields = table(node);
      for (const auto &[key, ignored] : fields)
        if (key != "size" && key != "position" && key != "display_name")
          throw std::invalid_argument("Unknown session field");
      if (!fields.get("size") || !fields.get("position") ||
          !fields.get("display_name"))
        throw std::invalid_argument("Incomplete saved window");
      SavedWindow saved{integers(*fields.get("size")),
                        integers(*fields.get("position")),
                        value<std::string>(*fields.get("display_name"))};
      if (!math::hasArea(saved.size))
        throw std::invalid_argument("Invalid saved window size");
      result.emplace(std::string{app.str()}, std::move(saved));
    }
  }
  return result;
}
std::string serializeSession(const SessionState &state) {
  toml::table apps;
  for (const auto &[app, saved] : state)
    apps.insert(app,
                toml::table{{"size", toml::array{saved.size.x, saved.size.y}},
                            {"position",
                             toml::array{saved.position.x, saved.position.y}},
                            {"display_name", saved.displayName}});
  return format(toml::table{{"schema_version", 1}, {"apps", std::move(apps)}});
}
void SettingsStore::reload() { publish(readSnapshot()); }
SettingsSnapshot SettingsStore::readSnapshot() const {
  auto project = _projectFiles.read("project.toml");
  auto user = _userFiles.read("settings.toml");
  auto session = _userFiles.read("session.toml");
  auto nextProject = project ? parseSettings(*project) : SettingsDocument{};
  auto nextUser = user ? parseSettings(*user) : SettingsDocument{};
  auto nextSession = session ? parseSession(*session) : SessionState{};
  return {std::move(nextProject), std::move(nextUser), std::move(nextSession)};
}
void SettingsStore::publish(SettingsSnapshot snapshot) noexcept {
  _project = std::move(snapshot.project);
  _user = std::move(snapshot.user);
  _session = std::move(snapshot.session);
}
void SettingsStore::setUser(SettingsDocument document, bool persist) {
  const auto text = serializeSettings(document);
  auto checked = parseSettings(text);
  if (persist)
    _userFiles.replace("settings.toml", text);
  _user = std::move(checked);
}
void SettingsStore::saveSession(SessionState state) {
  const auto text = serializeSession(state);
  auto checked = parseSession(text);
  _userFiles.replace("session.toml", text);
  _session = std::move(checked);
}
void SettingsStore::resolve(std::string_view app, PresentationProps &p,
                            AppViewPolicy &v) const {
  resolveWithUser(app, _user, p, v);
}
void SettingsStore::resolveWithUser(std::string_view app,
                                    const SettingsDocument &user,
                                    PresentationProps &p,
                                    AppViewPolicy &v) const {
  auto presentation = p;
  auto policy = v;
  _project.apply(app, presentation, policy);
  user.apply(app, presentation, policy);
  presentation.validate();
  policy.validate();
  p = std::move(presentation);
  v = policy;
}
} // namespace playground::platform
