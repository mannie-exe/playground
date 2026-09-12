#pragma once

#include <string>
#include <string_view>

#include <SDL3/SDL_filesystem.h>

namespace playground::assets {
inline std::string basePath() {
  const char *path = SDL_GetBasePath();
  return path ? std::string{path} : std::string{};
}

inline std::string path(std::string_view relativePath) {
  return basePath() + std::string{relativePath};
}
} // namespace playground::assets
