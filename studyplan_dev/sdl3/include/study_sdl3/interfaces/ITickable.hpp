#pragma once

class ITickable {
protected:
  ITickable() = default;

public:
  virtual ~ITickable() = default;

  virtual void update(float) {}

  ITickable(ITickable &&) noexcept = default;
  ITickable &operator=(ITickable &&) noexcept = default;

  ITickable(const ITickable &) = delete;
  ITickable &operator=(const ITickable &) = delete;
};
