#include <utility>

#include <app/AppContext.hpp>
#include <platform/sdl/SystemAppearance.hpp>
#include <platform/sdl/UISession.hpp>
#include <platform/sdl/WindowServices.hpp>

namespace playground::sdl {
// AppHost-specific convenience stays out of the reusable SDL module.
void UISession::synchronize(AppContext &ctx) {
  _graphics = ctx.graphicsState();
  auto &services = ctx.windowServices();
  _root.setInteractionProps(ctx.viewPolicy().interaction);
  _root.setAppearance(ctx.viewPolicy().colorScheme,
                      ctx.viewPolicy().userContrast, systemAppearance());
  if (_windowServices != &services) {
    _root.setWakeCallback(ctx.wakeCallback());
    services.setWakeCallback(ctx.wakeCallback());
    auto attachment = services.attach(_root);
    _attachment.disconnect();
    _attachment = std::move(attachment);
    _windowServices = &services;
  }
  synchronize(ctx.windowMetrics(), ctx.presentation().viewport,
              &ctx.performance());
  services.setMode(_root.interactionProps().accessibility);
  services.pump();
  services.publish(_root, _mapping);
}
} // namespace playground::sdl
