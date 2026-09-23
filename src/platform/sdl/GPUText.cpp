#include <platform/sdl/GPUText.hpp>
#include <support/SDLError.hpp>
#include <support/TextFlow.hpp>

namespace playground::sdl {

GPUTextEngine::GPUTextEngine(GPUDeviceHandle device)
    : _device{std::move(device)} {
  if (!_device)
    throw std::invalid_argument("GPU text engine requires a device");
  _engine.reset(TTF_CreateGPUTextEngine(_device->get()));
  if (!_engine)
    throwSDLError("Failed to create GPU text engine");
}

GPUText::GPUText(std::shared_ptr<GPUTextEngine> engine, FontHandle font,
                 std::string_view utf8)
    : _engine{std::move(engine)}, _font{std::move(font)} {
  if (!_engine || !_font)
    throw std::invalid_argument("GPU text requires an engine and font");
  (void)ui::graphemeBoundaries(utf8);
  _text.reset(TTF_CreateText(_engine->get(), _font->get(),
                             utf8.empty() ? "" : utf8.data(), utf8.size()));
  if (!_text)
    throwSDLError("Failed to create GPU text");
}

void GPUText::setValue(std::string_view utf8) {
  (void)ui::graphemeBoundaries(utf8);
  if (!TTF_SetTextString(_text.get(), utf8.empty() ? "" : utf8.data(),
                         utf8.size()))
    throwSDLError("Failed to update GPU text");
}

const TTF_GPUAtlasDrawSequence *GPUText::drawData() {
  SDL_ClearError();
  const auto *result = TTF_GetGPUTextDrawData(_text.get());
  if (!result && *SDL_GetError())
    throwSDLError("Failed to prepare GPU glyph geometry");
  return result;
}

} // namespace playground::sdl
