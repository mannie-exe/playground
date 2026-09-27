#pragma once

class AppContext;

class IActivatable {
protected:
  IActivatable() = default;

public:
  virtual ~IActivatable() = default;

  // Build owned state; commands are deferred until activation succeeds.
  // External side effects are the application's responsibility and cannot be
  // rolled back.
  virtual void onEnter(AppContext &) {}

  // Cleanup-only: do not request new host operations or throw. The host
  // isolates throwing implementations, but cannot repair their external
  // effects.
  virtual void onExit(AppContext &) {}

  IActivatable(IActivatable &&) noexcept = default;
  IActivatable &operator=(IActivatable &&) noexcept = default;

  IActivatable(const IActivatable &) = delete;
  IActivatable &operator=(const IActivatable &) = delete;
};
