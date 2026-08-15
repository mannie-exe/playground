#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <random.h>
#include <readchar.h>

int main() {
  int sdl_version = SDL_GetVersion();
  int image_version = IMG_Version();
  int ttf_version = TTF_Version();

  return sdl_version == 0 || image_version == 0 || ttf_version == 0;
}
