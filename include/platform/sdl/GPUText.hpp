#pragma once

#include <platform/sdl/GPUResources.hpp>
#include <string_view>
#include <support/Font.hpp>

namespace playground::sdl {

// SDL/TTF initialization and the renderer thread must outlive all handles.
class GPUTextEngine {
  GPUDeviceHandle _device;
  SDLResource<TTF_TextEngine, TTF_DestroyGPUTextEngine> _engine;

public:
  explicit GPUTextEngine(GPUDeviceHandle device);
  TTF_TextEngine *get() const noexcept { return _engine.get(); }
  const GPUDeviceHandle &device() const noexcept { return _device; }
  GPUTextEngine(const GPUTextEngine &) = delete;
  GPUTextEngine &operator=(const GPUTextEngine &) = delete;
};

// Retains the atlas engine and font. Returned draw data is borrowed and must
// not be kept past text/engine changes; copy geometry before recording
// commands.
class GPUText {
  std::shared_ptr<GPUTextEngine> _engine;
  FontHandle _font;
  SDLResource<TTF_Text, TTF_DestroyText> _text;

public:
  GPUText(std::shared_ptr<GPUTextEngine> engine, FontHandle font,
          std::string_view utf8);
  void setValue(std::string_view utf8);
  const TTF_GPUAtlasDrawSequence *drawData();
  GPUText(const GPUText &) = delete;
  GPUText &operator=(const GPUText &) = delete;
};

} // namespace playground::sdl
