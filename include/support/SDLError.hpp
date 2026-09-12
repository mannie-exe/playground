#pragma once

#include <format>
#include <string>
#include <string_view>

#include <SDL3/SDL_error.h>

inline std::string consumeSDLError() {
  const char *rawError = SDL_GetError();
  std::string error{rawError ? rawError : ""};

  if (!error.empty()) {
    SDL_ClearError();
  }

  return error;
}

inline std::string buildSDLErrorMessage(std::string_view context) {
  std::string error = consumeSDLError();

  if (error.empty()) {
    return std::string{context};
  }

  return std::format("{}:\n  {}", context, error);
}

[[noreturn]] inline void throwSDLError(std::string_view context) {
  throw buildSDLErrorMessage(context);
}

template <typename T> T *requireSDL(T *resource, std::string_view context) {
  if (!resource) {
    throwSDLError(context);
  }

  return resource;
}
