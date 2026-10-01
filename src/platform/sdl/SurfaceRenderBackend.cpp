#include <optional>
#include <stdexcept>

#include <platform/sdl/RenderError.hpp>
#include <platform/sdl/SoftwareSceneRenderer.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <platform/sdl/SurfaceRenderBackend.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {
namespace {

class SurfaceFrame final : public rendering::RenderFrame,
                           private scene::SceneRenderer {
  SDL_Window &_window;
  bool &_frameActive;
  std::uint64_t &_completedWork;
  SDL_Surface *_scaled;

  std::optional<SurfacePainter> _painter;
  SoftwareSceneRenderer _scene;

  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &props,
         std::span<const scene::MeshDraw> draws) override {
    if (!_painter)
      throw std::logic_error("Frame has already been presented");
    return _scene.render(props, draws);
  }

public:
  SurfaceFrame(SDL_Window &window, bool &frameActive,
               std::uint64_t &completedWork, rendering::RenderFrameProps props,
               SDL_Surface *scaled,
               const rendering::RenderBackendProps &backend)
      : _window{window}, _frameActive{frameActive},
        _completedWork{completedWork}, _scaled{scaled},
        _scene{backend.allocations, backend.resources} {
    _admission = std::move(props.admission);
    auto *surface = scaled ? scaled : SDL_GetWindowSurface(&_window);
    if (!surface)
      throwRenderError("Failed to acquire window surface for frame",
                       rendering::RenderOperation::Acquire);
    int width{}, height{};
    if (!SDL_GetWindowSize(&_window, &width, &height))
      throwRenderError("Failed to query frame logical size",
                       rendering::RenderOperation::Query);
    if (width <= 0 || height <= 0)
      throw std::runtime_error("Surface frame requires positive logical size");

    _painter.emplace(
        *surface, SurfacePainterProps{
                      .pixelScale = {static_cast<float>(surface->w) / width,
                                     static_cast<float>(surface->h) / height},
                      .resources = backend.resources});
    // Clearing replaces pixels rather than alpha-blending over the last frame.
    const auto color = props.clearColor;
    SDL_SetSurfaceClipRect(surface, nullptr);
    if (!SDL_FillSurfaceRect(
            surface, nullptr,
            SDL_MapSurfaceRGBA(surface, color.r, color.g, color.b, color.a)))
      throwRenderError("Failed to clear frame surface",
                       rendering::RenderOperation::Record);
    _frameActive = true;
  }

  ~SurfaceFrame() override {
    _painter.reset();
    _frameActive = false;
  }

  rendering::PaintContext &paint2D() override {
    if (!_painter)
      throw std::logic_error("Frame has already been presented");
    return *_painter;
  }

  scene::SceneRenderer *scene3D() noexcept override { return this; }

  rendering::PresentationOutcome present() override {
    if (!_painter)
      throw std::logic_error("Frame has already been presented");
    _painter.reset();
    if (_scaled) {
      auto *target = SDL_GetWindowSurface(&_window);
      if (!target)
        throwRenderError("Failed to acquire scaled presentation target",
                         rendering::RenderOperation::Acquire);
      SDL_Rect clip{};
      SDL_GetSurfaceClipRect(target, &clip);

      struct ClipGuard {
        SDL_Surface *target;
        SDL_Rect clip;

        ~ClipGuard() { SDL_SetSurfaceClipRect(target, &clip); }
      } guard{target, clip};

      SDL_SetSurfaceClipRect(target, nullptr);
      if (!SDL_SetSurfaceBlendMode(_scaled, SDL_BLENDMODE_NONE) ||
          !SDL_BlitSurfaceScaled(_scaled, nullptr, target, nullptr,
                                 SDL_SCALEMODE_LINEAR))
        throwRenderError("Failed to scale frame for presentation",
                         rendering::RenderOperation::Present);
    }
    if (!SDL_UpdateWindowSurface(&_window))
      throwRenderError("Failed to present frame surface",
                       rendering::RenderOperation::Present);
    ++_completedWork;
    return rendering::PresentationOutcome::Submitted;
  }
};

} // namespace

math::Vec2i SurfaceRenderBackend::drawableSize() const {
  math::Vec2i size;
  if (!SDL_GetWindowSizeInPixels(&_window, &size.x, &size.y))
    throwRenderError("Failed to query drawable size",
                     rendering::RenderOperation::Query);
  return size;
}

std::unique_ptr<rendering::RenderFrame>
SurfaceRenderBackend::beginFrame(rendering::RenderFrameProps props) {
  if (_frameActive)
    throw std::logic_error("A rendering frame is already alive");
  props.settings.validate();
  const auto drawable = drawableSize();
  if ((SDL_GetWindowFlags(&_window) & SDL_WINDOW_MINIMIZED) ||
      !math::hasArea(drawable))
    return {};
  const auto targetSize = props.settings.targetSize(drawable);
  _props.allocations.validateTarget(targetSize, 4, "Software frame");
  if (rendering::AllocationLimits::textureBytes(targetSize, 1) >
      _props.allocations.maxSoftwareTargetPixels)
    throw std::length_error("Software frame exceeds pixel policy");
  if (props.settings.resolutionScale == 1) {
    _scaledTarget.reset();
  } else {
    const auto size = targetSize;
    if (!_scaledTarget || _scaledTarget->w != size.x ||
        _scaledTarget->h != size.y) {
      auto replacement = createManagedSurface(size.x, size.y, _props.resources);
      if (!replacement)
        throwSDLError("Failed to allocate scaled frame target");
      _scaledTarget = std::move(replacement);
    }
  }
  return std::make_unique<SurfaceFrame>(_window, _frameActive, _completedWork,
                                        props, _scaledTarget.get(), _props);
}

} // namespace playground::sdl
