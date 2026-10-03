#pragma once

#include <filesystem>

#include <world/Store.hpp>

namespace playground::platform {
// Existing local directory, numeric world filenames. Process locks coordinate
// writers; temp-file flush + replacement + directory flush establish
// durability. Network filesystems and hostile filesystem writers are outside
// this contract.
class DirectoryCheckpointStore final : public world::CheckpointStorage {
  std::filesystem::path _root;
  std::size_t _maxBytes;

public:
  explicit DirectoryCheckpointStore(std::filesystem::path,
                                    std::size_t maxBytes = 128 * 1024 * 1024);
  std::optional<world::StoredCheckpoint> read(world::WorldId,
                                              std::size_t maxBytes) override;
  std::uint64_t publish(world::WorldId, std::uint64_t expectedRevision,
                        std::span<const std::byte>,
                        std::size_t maxBytes) override;
};
} // namespace playground::platform
