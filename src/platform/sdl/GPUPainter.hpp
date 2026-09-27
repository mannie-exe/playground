#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "GPUInternal.hpp"
#include <math/ColorSpace.hpp>
#include <platform/sdl/FontTextSource.hpp>
#include <platform/sdl/GPUText.hpp>
#include <rendering/PaintContext.hpp>
#include <rendering/PaintWork.hpp>
#include <rendering/TextImagePreparer.hpp>

namespace playground::sdl::gpu_detail {

struct Shape {
  std::array<float, 4> rowX{}, rowY{}, bounds{}, top{}, bottom{};
};

struct PaintUniform {
  std::array<float, 4> tint{}, borderTint{}, source{}, options{},
      imageOptions{};
  Shape outer, inner;
  std::array<Shape, 32> clips;
};

struct Draw {
  math::Rect bounds;
  PaintUniform uniforms;
  rendering::PaintImageHandle owner;
  SDL_GPUTexture *texture{};
  std::optional<math::Rect> clipBounds;
  bool rectangular{};

  struct PathUniform {
    std::array<float, 4> options{};
    std::array<std::array<float, 4>, 255> segments{};
  };

  static_assert(sizeof(PathUniform) == 4096);
  std::shared_ptr<const PathUniform> path;
};

using PaintStats = rendering::PaintWork;

class PaintDevice;
enum class TextPreparation { Atlas, RasterCompatibility };

class ContentPreparer final : public GPUImagePreparer,
                              public rendering::TextImagePreparer {
  PaintDevice &_paint;
  std::shared_ptr<GPUTextEngine> _textEngine;
  TextPreparation _lastTextPreparation{TextPreparation::Atlas};

public:
  bool glyphAtlases{true};

  ContentPreparer(PaintDevice &paint, GPUDeviceHandle device)
      : GPUImagePreparer{std::move(device)}, _paint{paint} {}

  rendering::ResourceDomainId resourceDomain() const noexcept override;

  bool isEnabled() const noexcept override { return glyphAtlases; }

  TextPreparation lastTextPreparation() const noexcept {
    return _lastTextPreparation;
  }

  rendering::PaintImageHandle
  prepareText(const rendering::TextSource &source) override;
};

class PaintDevice {
  Shader _vertex, _fragment, _rectFragment, _presentFragment;
  Sampler _sampler, _linearSampler;
  std::map<std::pair<SDL_GPUTextureFormat, int>, Pipeline> _pipelines;
  StreamBuffer _drawStream;
  PaintStats _stats;

public:
  GPUDeviceHandle device;
  ContentPreparer images;
  TargetPool targets;
  std::shared_ptr<GPUImage> white;

  explicit PaintDevice(GPUDeviceHandle device);

  const PaintStats &stats() const noexcept { return _stats; }

  PaintStats takeStats() noexcept { return std::exchange(_stats, {}); }

  void considered() noexcept { ++_stats.considered; }

  void rejected() noexcept { ++_stats.rejected; }

  // Used by parity tests; complex geometry always takes the general path.
  bool rectangularFastPath{true};

  void encode(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *target,
              math::Vec2i size, SDL_GPUTextureFormat format,
              math::ColorRGBA8 clear, std::span<const Draw> draws,
              bool encoded = false);
};

class GPUPainter final : public rendering::PaintContext {
  struct State {
    math::Transform2D transform;
    std::vector<Shape> clips;
    std::optional<math::Rect> clipBounds;
    bool rectangularClips{true};
  };

  struct Layer {
    State state;
    std::vector<Draw> parent;
    math::Rect bounds;
    float opacity;
    std::size_t saveDepth;
  };

  PaintDevice &_device;
  State _state;
  std::vector<State> _states;
  std::vector<Layer> _layers;
  std::vector<Draw> _draws;

  Shape shape(math::RoundedRect bounds) const;
  bool draw(math::RoundedRect outer, std::optional<math::RoundedRect> inner,
            math::ColorRGBA8 color, math::ColorRGBA8 border,
            rendering::PaintImageHandle image = {}, math::Rect source = {},
            rendering::Sampling sampling = rendering::Sampling::Linear);

public:
  GPUPainter(PaintDevice &device, math::Vec2f scale) : _device{device} {
    _state.transform = math::Transform2D::scaling(scale);
  }

  rendering::ImagePreparer *imagePreparer() noexcept override {
    return &_device.images;
  }

  rendering::TextImagePreparer *textPreparer() noexcept override {
    return &_device.images;
  }

  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return _device.device->resourceDomain();
  }

  std::size_t captureBytesPerPixel() const noexcept override { return 8; }

  void save() override { _states.push_back(_state); }

  void restore() noexcept override;

  void translate(math::Vec2f offset) override {
    transform(math::Transform2D::translation(offset));
  }

  void transform(math::Transform2D value) override;
  math::Vec2f pixelScale() const noexcept override;

  void clip(math::Rect rectangle) override { clipRounded({rectangle, {}}); }

  void clipRounded(math::RoundedRect shape) override;

  void fill(math::Rect bounds, math::ColorRGBA8 color) override {
    fillRounded({bounds, {}}, color);
  }

  void fillRounded(math::RoundedRect bounds, math::ColorRGBA8 color) override;
  void drawPath(const math::Path2D &, const rendering::PathPaint &) override;
  void strokeRounded(math::RoundedRect shape, math::Insets widths,
                     math::ColorRGBA8 color) override;
  void paintRoundedBox(math::RoundedRect shape, math::Insets widths,
                       math::ColorRGBA8 color,
                       std::optional<math::ColorRGBA8> border) override;
  void drawImage(const rendering::PaintImageHandle &, math::Rect, math::Rect,
                 rendering::ImagePaint) override;
  void beginLayer(math::Rect bounds, float opacity) override;
  void endLayer() override;
  void cancelLayer() noexcept override;
  rendering::PaintImageHandle
  capture(math::Rect bounds, math::Vec2f scale,
          const std::function<void(rendering::PaintContext &)> &draw) override;
  void finish(SDL_GPUTexture *target, math::Vec2i size, math::ColorRGBA8 clear);
  void encode(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *target,
              math::Vec2i size, SDL_GPUTextureFormat format, bool encoded);
};

} // namespace playground::sdl::gpu_detail
