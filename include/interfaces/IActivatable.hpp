#pragma once

class AppContext;

class IActivatable {
protected:
  IActivatable() = default;

public:
  virtual ~IActivatable() = default;

  virtual void onEnter(AppContext &) {}
  virtual void onExit(AppContext &) {}

  IActivatable(IActivatable &&) noexcept = default;
  IActivatable &operator=(IActivatable &&) noexcept = default;

  IActivatable(const IActivatable &) = delete;
  IActivatable &operator=(const IActivatable &) = delete;
};
