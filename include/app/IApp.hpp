#pragma once

#include <app/AppContext.hpp>
#include <app/AppTypes.hpp>
#include <interfaces/IRuntimeObject.hpp>

class IApp : public IRuntimeObject {
public:
  virtual ~IApp() = default;

  virtual AppInfo info() const = 0;
  // Invalidate native resources after a published backend/domain change.
  // CPU/model state stays intact. No host commands, exceptions or onEnter
  // replay.
  virtual void
  onRendererChanged(AppContext &, playground::rendering::ResourceDomainId,
                    playground::rendering::ResourceDomainId) noexcept {}
  // Query only after onEnter has constructed content. No window mutations.
  // Null means this app has no preferred-content measurement implementation.
  virtual std::optional<playground::math::Size2>
  preferredContentSize(playground::math::Size2 maximum,
                       playground::math::Vec2f density) {
    return {};
  }

  IApp(IApp &&) noexcept = default;
  IApp &operator=(IApp &&) noexcept = default;

  IApp(const IApp &) = delete;
  IApp &operator=(const IApp &) = delete;

protected:
  IApp() = default;
};
