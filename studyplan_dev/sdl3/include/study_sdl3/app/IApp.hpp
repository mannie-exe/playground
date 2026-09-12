#pragma once

#include <study_sdl3/app/AppContext.hpp>
#include <study_sdl3/app/AppTypes.hpp>
#include <study_sdl3/interfaces/IRuntimeObject.hpp>

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
