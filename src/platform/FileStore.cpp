#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <platform/FileStore.hpp>
#include <support/SDLError.hpp>

namespace playground::platform {
namespace {
std::filesystem::path fromUTF8(std::string_view text) {
  return std::filesystem::path{std::u8string{
      reinterpret_cast<const char8_t *>(text.data()), text.size()}};
}

std::string utf8(const std::filesystem::path &path) {
  const auto value = path.u8string();
  return {reinterpret_cast<const char *>(value.data()), value.size()};
}
} // namespace

DirectoryStore::DirectoryStore(std::filesystem::path root, bool writable)
    : _root{std::filesystem::absolute(std::move(root))}, _writable{writable} {}

std::filesystem::path DirectoryStore::resolve(std::string_view name) const {
  // Documents live directly in their store. Reject separators/Windows streams
  // on every platform so the serialized names have identical meaning.
  if (name.empty() || name == "." || name == ".." ||
      name.find_first_of("/\\:") != std::string_view::npos ||
      name.find('\0') != std::string_view::npos)
    throw std::invalid_argument("Expected a relative document filename");
  return _root / fromUTF8(name);
}

std::optional<std::string> DirectoryStore::read(std::string_view name) const {
  const auto path = resolve(name);
  if (!std::filesystem::exists(path))
    return {};
  const auto size = std::filesystem::file_size(path);
  if (size > 1024 * 1024)
    throw std::length_error("Settings document exceeds 1 MiB limit");
  std::ifstream stream{path, std::ios::binary};
  if (!stream)
    throw std::runtime_error("Cannot open settings document: " + utf8(path));
  std::string result(static_cast<std::size_t>(size), '\0');
  stream.read(result.data(), static_cast<std::streamsize>(size));
  if (!stream || stream.peek() != std::char_traits<char>::eof())
    throw std::runtime_error("Settings document changed or failed during read");
  return result;
}

void DirectoryStore::replace(std::string_view name, std::string_view content) {
  if (!_writable)
    throw std::logic_error("This document store is read-only");
  if (content.size() > 1024 * 1024)
    throw std::length_error("Settings document exceeds 1 MiB limit");
  const auto destination = resolve(name);
  std::filesystem::create_directories(_root);
  static std::atomic<unsigned long long> sequence{};
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto temporary =
      resolve(std::string{name} + ".tmp-" + std::to_string(nonce) + "-" +
              std::to_string(++sequence));
  std::ofstream stream{temporary,
                       std::ios::binary | std::ios::out | std::ios::noreplace};
  if (!stream)
    throw std::runtime_error("Cannot create settings temporary file");

  struct Cleanup {
    std::filesystem::path path;
    std::ofstream &stream;

    ~Cleanup() {
      stream.close();
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  } cleanup{temporary, stream};

  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
  stream.flush();
  if (!stream)
    throw std::runtime_error("Cannot write settings temporary file");
  stream.close();
  if (!stream)
    throw std::runtime_error("Cannot close settings temporary file");
  if (!SDL_RenamePath(utf8(temporary).c_str(), utf8(destination).c_str()))
    throwSDLError("Cannot replace settings document");
}

std::filesystem::path preferenceDirectory(std::string_view organization,
                                          std::string_view application) {
  auto *value = SDL_GetPrefPath(std::string{organization}.c_str(),
                                std::string{application}.c_str());
  if (!value)
    throwSDLError("Cannot obtain preferences directory");

  struct Guard {
    char *value;

    ~Guard() { SDL_free(value); }
  } guard{value};

  return fromUTF8(value);
}

std::filesystem::path executableDirectory() {
  const char *value = SDL_GetBasePath();
  if (!value)
    throwSDLError("Cannot obtain executable directory");
  return fromUTF8(value);
}
} // namespace playground::platform
