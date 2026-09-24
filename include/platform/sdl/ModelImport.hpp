#pragma once

#include <filesystem>

#include <scene/ModelImport.hpp>

namespace playground::sdl {

// Reads glTF/GLB and relative files, decodes PNG/JPEG through SDL_image.
// No network URIs; sibling/descendant assets only. Native image work is
// CPU-only.
scene::ModelHandle loadGLTF(const std::filesystem::path &path,
                            const scene::ModelImportProps &props = {});

} // namespace playground::sdl
