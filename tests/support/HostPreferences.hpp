#pragma once

#include <platform/Settings.hpp>
#include <support/TemporaryDirectory.hpp>

namespace playground::test {
// Owns isolated preferences for a host's entire lifetime. Use the production
// serializer so workloads never embed a second settings schema.
class HostPreferences {
  TemporaryDirectory _directory;

public:
  explicit HostPreferences(std::string name,
                           rendering::GraphicsSettings graphics)
      : _directory{std::move(name)} {
    platform::DirectoryStore files{_directory.path(), true};
    files.replace("settings.toml",
                  platform::serializeSettings({.graphics = graphics}));
  }

  const auto &path() const noexcept { return _directory.path(); }
};
} // namespace playground::test
