#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace playground::platform {

// UTF-8 document names/content. Missing is distinct from unreadable. Names are
// relative to one root; this is a storage boundary, not a security sandbox.
class FileStore {
public:
  virtual ~FileStore() = default;
  virtual std::optional<std::string> read(std::string_view name) const = 0;
  virtual void replace(std::string_view name, std::string_view content) = 0;
};

class DirectoryStore final : public FileStore {
  std::filesystem::path _root;
  bool _writable;
  std::filesystem::path resolve(std::string_view name) const;

public:
  explicit DirectoryStore(std::filesystem::path root, bool writable);
  std::optional<std::string> read(std::string_view name) const override;
  void replace(std::string_view name, std::string_view content) override;
};

std::filesystem::path preferenceDirectory(std::string_view organization,
                                          std::string_view application);
std::filesystem::path executableDirectory();

} // namespace playground::platform
