#pragma once

#include <app/AppContext.hpp>
#include <app/AppTypes.hpp>
#include <interfaces/IRuntimeObject.hpp>

class IApp : public IRuntimeObject {
public:
  virtual ~IApp() = default;

  virtual AppInfo info() const = 0;

  IApp(IApp &&) noexcept = default;
  IApp &operator=(IApp &&) noexcept = default;

  IApp(const IApp &) = delete;
  IApp &operator=(const IApp &) = delete;

protected:
  IApp() = default;
};
