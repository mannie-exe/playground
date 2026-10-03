#pragma once

#include <filesystem>

#include <scene/ModelImport.hpp>

namespace playground::sdl {

// CPU PNG/JPEG decoder with private stream/surface ownership. Resident bytes
// are checked after decoding; this is not a hostile-input allocation sandbox.
rendering::PaintImageHandle
decodeModelImage(std::span<const std::byte>, std::string_view mime,
                 std::size_t maximumBytes,
                 std::shared_ptr<rendering::ResourceLedger> ledger =
                     rendering::defaultResourceLedger());

// Reads glTF/GLB and relative files, decodes PNG/JPEG through SDL_image.
// No network URIs; sibling/descendant assets only. Native image work is
// CPU-only.
scene::ModelHandle loadGLTF(const std::filesystem::path &path,
                            const scene::ModelImportProps &props = {},
                            std::shared_ptr<rendering::ResourceLedger> ledger =
                                rendering::defaultResourceLedger());

} // namespace playground::sdl
