#pragma once

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace playground::test {
class TemporaryDirectory {
  std::filesystem::path _path;

public:
  explicit TemporaryDirectory(std::string name) {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
      auto path =
          std::filesystem::temp_directory_path() /
          (name + "-" + std::to_string(stamp) + "-" + std::to_string(attempt));
      if (std::filesystem::create_directory(path)) {
        _path = std::move(path);
        return;
      }
    }
    throw std::runtime_error("Cannot create temporary test directory");
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(_path, error);
  }

  TemporaryDirectory(const TemporaryDirectory &) = delete;
  TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

  const auto &path() const noexcept { return _path; }
};
} // namespace playground::test
