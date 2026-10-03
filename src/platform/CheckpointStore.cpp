#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>

#include <sys/file.h>
#include <unistd.h>
#endif

#include <platform/CheckpointStore.hpp>

namespace playground::platform {
namespace {
#ifdef _WIN32
struct File {
  HANDLE value{INVALID_HANDLE_VALUE};

  explicit File(HANDLE handle) : value{handle} {
    if (value == INVALID_HANDLE_VALUE)
      throw std::system_error(GetLastError(), std::system_category(),
                              "Checkpoint file");
  }

  File(const File &) = delete;

  ~File() { CloseHandle(value); }
};

struct Lock {
  File file;
  OVERLAPPED overlap{};

  explicit Lock(const std::filesystem::path &path)
      : file{CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)} {
    if (!LockFileEx(file.value, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlap))
      throw std::system_error(GetLastError(), std::system_category(),
                              "Checkpoint lock");
  }

  ~Lock() { UnlockFileEx(file.value, 0, 1, 0, &overlap); }
};
#else
struct File {
  int value{-1};

  explicit File(int descriptor) : value{descriptor} {
    if (value < 0)
      throw std::system_error(errno, std::generic_category(),
                              "Checkpoint file");
  }

  File(const File &) = delete;

  ~File() { ::close(value); }
};

struct Lock {
  File file;

  explicit Lock(const std::filesystem::path &path)
      : file{::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW,
                    0600)} {
    while (::flock(file.value, LOCK_EX) != 0)
      if (errno != EINTR)
        throw std::system_error(errno, std::generic_category(),
                                "Checkpoint lock");
  }

  ~Lock() { ::flock(file.value, LOCK_UN); }
};

void flush(int file) {
  while (::fsync(file) != 0)
    if (errno != EINTR)
      throw std::system_error(errno, std::generic_category(),
                              "Checkpoint fsync");
#ifdef __APPLE__
  while (::fcntl(file, F_FULLFSYNC) != 0)
    if (errno != EINTR)
      throw std::system_error(errno, std::generic_category(),
                              "Checkpoint full synchronization");
#endif
}
#endif

std::filesystem::path filename(const std::filesystem::path &root,
                               world::WorldId id, std::string_view suffix) {
  if (!id.value)
    throw std::invalid_argument("Checkpoint world identity required");
  return root / (std::to_string(id.value) + std::string{suffix});
}

void put(std::span<std::byte> bytes, std::uint64_t value) {
  for (unsigned i = 0; i < 8; ++i)
    bytes[i] = std::byte((value >> (i * 8)) & 255);
}

std::uint64_t get(std::span<const std::byte> bytes) {
  std::uint64_t result{};
  for (unsigned i = 0; i < 8; ++i)
    result |= std::uint64_t(std::to_integer<unsigned>(bytes[i])) << (i * 8);
  return result;
}

// Accidental-corruption detection, not authentication of creator content.
std::uint64_t checksum(std::span<const std::byte> header,
                       std::span<const std::byte> payload) {
  std::uint64_t result = 14695981039346656037ULL;
  for (auto bytes : {header, payload})
    for (auto byte : bytes) {
      result ^= std::to_integer<unsigned>(byte);
      result *= 1099511628211ULL;
    }
  return result;
}

std::optional<world::StoredCheckpoint>
readFile(const std::filesystem::path &path, std::size_t maximum) {
  if (!std::filesystem::exists(path))
    return {};
  const auto size = std::filesystem::file_size(path);
  if (size < 24 || size - 24 > maximum)
    throw std::length_error("Invalid checkpoint file size");
  std::ifstream input{path, std::ios::binary};
  std::array<std::byte, 24> header;
  input.read(reinterpret_cast<char *>(header.data()), header.size());
  if (!input)
    throw std::runtime_error("Cannot read checkpoint envelope");
  world::StoredCheckpoint result;
  result.revision = get(header);
  if (!result.revision || get(std::span{header}.subspan(8)) != size - 24)
    throw std::invalid_argument("Invalid checkpoint envelope");
  result.bytes.resize(static_cast<std::size_t>(size - 24));
  input.read(reinterpret_cast<char *>(result.bytes.data()),
             result.bytes.size());
  if (!input || input.peek() != std::char_traits<char>::eof() ||
      checksum(std::span{header}.first(16), result.bytes) !=
          get(std::span{header}.subspan(16)))
    throw std::runtime_error("Checkpoint truncated, changed or corrupt");
  return result;
}

void replaceFile(const std::filesystem::path &root,
                 const std::filesystem::path &path,
                 const std::filesystem::path &temporary,
                 std::span<const std::byte> bytes) {
  // One cooperating writer holds the world lock. An interrupted candidate is
  // never a committed checkpoint and can be replaced by the next attempt.
  std::filesystem::remove(temporary);

  struct Cleanup {
    std::filesystem::path path;

    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  } cleanup{temporary};
#ifdef _WIN32
  {
    File file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    std::size_t offset{};
    while (offset < bytes.size()) {
      const auto count = static_cast<DWORD>(
          std::min<std::size_t>(bytes.size() - offset, 1024 * 1024));
      DWORD written{};
      if (!WriteFile(file.value, bytes.data() + offset, count, &written,
                     nullptr) ||
          !written)
        throw std::system_error(GetLastError(), std::system_category(),
                                "Checkpoint write");
      offset += written;
    }
    if (!FlushFileBuffers(file.value))
      throw std::system_error(GetLastError(), std::system_category(),
                              "Checkpoint flush");
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw world::CheckpointUncertain(
        "Checkpoint publication failed: " +
        std::system_category().message(GetLastError()));
#else
  File file{::open(temporary.c_str(),
                   O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW, 0600)};
  std::size_t offset{};
  while (offset < bytes.size()) {
    const auto written =
        ::write(file.value, bytes.data() + offset, bytes.size() - offset);
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0)
      throw std::system_error(written ? errno : EIO, std::generic_category(),
                              "Checkpoint write");
    offset += static_cast<std::size_t>(written);
  }
  flush(file.value);
  if (::rename(temporary.c_str(), path.c_str()) != 0)
    throw std::system_error(errno, std::generic_category(),
                            "Checkpoint publication");
  // A failure after rename has an uncertain acknowledgement, not permission to
  // retry with a stale expected revision. Re-read and reconcile the checkpoint.
  try {
    File directory{::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
    flush(directory.value);
  } catch (const std::exception &error) {
    throw world::CheckpointUncertain(error.what());
  }
#endif
}
} // namespace

DirectoryCheckpointStore::DirectoryCheckpointStore(std::filesystem::path root,
                                                   std::size_t maximum)
    : _root{std::filesystem::canonical(root)}, _maxBytes{maximum} {
  if (!std::filesystem::is_directory(_root) || !maximum ||
      maximum > 1024ULL * 1024 * 1024)
    throw std::invalid_argument(
        "Checkpoint store needs an existing directory and byte limit");
}

std::optional<world::StoredCheckpoint>
DirectoryCheckpointStore::read(world::WorldId id, std::size_t maximum) {
  Lock lock{filename(_root, id, ".lock")};
  return readFile(filename(_root, id, ".world"), std::min(maximum, _maxBytes));
}

std::uint64_t
DirectoryCheckpointStore::publish(world::WorldId id, std::uint64_t expected,
                                  std::span<const std::byte> bytes,
                                  std::size_t maxBytes) {
  const auto maximum = std::min(maxBytes, _maxBytes);
  if (bytes.size() > maximum ||
      expected == std::numeric_limits<std::uint64_t>::max())
    throw std::length_error("Checkpoint byte/revision limit exceeded");
  Lock lock{filename(_root, id, ".lock")};
  const auto path = filename(_root, id, ".world");
  const auto old = readFile(path, maximum);
  if ((old ? old->revision : 0) != expected)
    throw std::invalid_argument("Checkpoint store revision conflict");
  std::vector<std::byte> encoded(24 + bytes.size());
  put(encoded, expected + 1);
  put(std::span{encoded}.subspan(8), bytes.size());
  put(std::span{encoded}.subspan(16),
      checksum(std::span{encoded}.first(16), bytes));
  std::copy(bytes.begin(), bytes.end(), encoded.begin() + 24);
  replaceFile(_root, path, filename(_root, id, ".candidate"), encoded);
  return expected + 1;
}
} // namespace playground::platform
