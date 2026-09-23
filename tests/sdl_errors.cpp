#include <support/SDLError.hpp>
#include <support/Test.hpp>

int main() {
  return playground::test::run([] {
    using playground::test::require;
    SDL_SetError("example failure");
    bool caught{};
    try {
      throwSDLError("loading asset");
    } catch (const std::exception &error) {
      caught = true;
      require(std::string{error.what()} == "loading asset:\n  example failure",
              "standard exception retains SDL context and detail");
    }
    require(caught && *SDL_GetError() == '\0', "error consumed and caught");
    playground::test::rejects<std::runtime_error>(
        [] { requireSDL<int>(nullptr, "null resource"); },
        "null resource uses runtime_error even without SDL detail");
    int value{};
    require(requireSDL(&value, "valid") == &value, "valid resource unchanged");
  });
}
