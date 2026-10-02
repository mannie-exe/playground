#include <utility>

#include <app/AppContext.hpp>
#include <app/Assets.hpp>
#include <platform/sdl/SystemAppearance.hpp>
#include <platform/sdl/UISession.hpp>
#include <platform/sdl/WindowServices.hpp>

namespace playground::sdl {
// AppHost-specific convenience stays out of the reusable SDL module.
void UISession::synchronize(AppContext &ctx) {
  _graphics = ctx.graphicsState();
  auto &services = ctx.windowServices();
  _root.setInteractionProps(ctx.viewPolicy().interaction);
  const auto appearance = systemAppearance();
  _root.motion().setPreference(_graphics.requested.motion,
                               appearance.reducedMotion.value_or(false));
  _root.setAppearance(ctx.viewPolicy().colorScheme,
                      ctx.viewPolicy().userContrast, appearance);
  if (_windowServices != &services) {
    auto definition = _root.themeDefinition();
    app::configureThemeFonts(definition.typography, ctx.resources());
    definition.typography.textScale = _graphics.requested.textScale;
    _root.setThemeDefinition(std::move(definition));
    _root.setWakeCallback(ctx.wakeCallback());
    services.setWakeCallback(ctx.wakeCallback());
    auto attachment = services.attach(_root);
    _attachment.disconnect();
    _attachment = std::move(attachment);
    _windowServices = &services;
  }
  if (_root.themeDefinition().typography.textScale !=
      _graphics.requested.textScale) {
    auto definition = _root.themeDefinition();
    definition.typography.textScale = _graphics.requested.textScale;
    _root.setThemeDefinition(std::move(definition));
  }
  synchronize(ctx.windowMetrics(), ctx.presentation().viewport,
              &ctx.performance());
  if (auto *content = _root.content(); content && content->id() != _revealed) {
    if (_timing == UISessionTiming::Monotonic)
      update(0);
    _revealed = content->id();
    _reveal = _root.motion().play(
        ui::motion::opacity(content->handle()),
        ui::Keyframes<float>{{{0, 0.f},
                              {1, std::get<float>(content->motionValue(
                                      ui::MotionProperty::Opacity))}}},
        _root.resolvedTheme().motion.reveal);
  }
  services.setMode(_root.interactionProps().accessibility);
  services.pump();
  services.publish(_root, _mapping);
}
} // namespace playground::sdl
