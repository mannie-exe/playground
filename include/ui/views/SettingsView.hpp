#pragma once
#include <functional>

#include <rendering/GraphicsSettings.hpp>
#include <ui/containers/Box.hpp>
#include <ui/content/Text.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/Form.hpp>
#include <ui/controls/Meter.hpp>
#include <ui/controls/TextField.hpp>

namespace playground::ui {
struct SettingsViewActions {
  std::function<void(rendering::GraphicsSettings, bool persist)> apply;
  std::function<void()> close, returnToMenu;
};

// Replaceable host seam. A fork can supply any retained content and own draft
// controls; only publication and runtime readouts are required by the host.
class SettingsPanel : public Box {
public:
  using Box::Box;
  virtual void setRuntime(const rendering::ResolvedGraphicsState &,
                          const rendering::RenderRuntimeSnapshot &) = 0;
  virtual void setResult(rendering::GraphicsSettings, std::string message) = 0;
};

// Reusable retained UI, with no filesystem or AppHost dependency. Draft edits
// have no renderer side effects. Uses inherited ThemePalette and caller fonts.
class SettingsView : public SettingsPanel {
  AssetRegistry &_assets;
  FontHandle _font;
  SettingsViewActions _actions;
  rendering::GraphicsSettings _draft, _applied;
  std::optional<rendering::ResourceSnapshot> _usage;
  std::vector<Connection> _connections;
  Text *_status{}, *_meters{};
  Form _form;

  struct EditorEntry {
    std::string key;
    unsigned group;
    NumberField *editor;
    Text *error;
  };

  std::vector<EditorEntry> _editors;
  std::vector<Connection> _registrations;
  Tabs *_tabs{};
  Meter *_cpuMeter{}, *_gpuMeter{};
  std::vector<std::function<void()>> _refresh;
  std::unique_ptr<Text> text(std::string value);
  void build();
  void edited();

protected:
  void arrangeChildren(ArrangeContext &, math::Rect) override;

  void onDetach() noexcept override { _registrations.clear(); }

  void submit(bool persist);

public:
  SettingsView(AssetRegistry &, FontHandle, rendering::GraphicsSettings,
               SettingsViewActions);
  void setRuntime(const rendering::ResolvedGraphicsState &,
                  const rendering::RenderRuntimeSnapshot &) override;
  void setResult(rendering::GraphicsSettings applied,
                 std::string message) override;

  const auto &draft() const noexcept { return _draft; }

  bool dirty() const { return _draft != _applied || _form.dirty(); }
};

std::unique_ptr<SettingsView> makeSettingsView(AssetRegistry &, FontHandle,
                                               rendering::GraphicsSettings,
                                               SettingsViewActions);
using SettingsViewFactory = std::function<std::unique_ptr<SettingsPanel>(
    AssetRegistry &, FontHandle, rendering::GraphicsSettings,
    SettingsViewActions)>;
} // namespace playground::ui
