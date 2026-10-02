#include <format>
#include <optional>

#include <SDL3/SDL_vulkan.h>

#include "GPUPainter.hpp"
#include "GPUSceneRenderer.hpp"
#include <math/ColorSpace.hpp>
#include <platform/sdl/GPUFrameAccess.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <platform/sdl/RenderError.hpp>
#include <rendering/RenderFailure.hpp>
#include <scene/SceneRenderer.hpp>

namespace playground::sdl {

SDL_GPUShaderFormat packagedShaderFormats() noexcept {
  SDL_GPUShaderFormat formats{};
#ifdef PLAYGROUND_SHADER_SPIRV
  formats |= SDL_GPU_SHADERFORMAT_SPIRV;
#endif
  return formats;
}

std::vector<rendering::RendererCandidate>
availableGPURenderers(std::string &unavailabilityReason) {
  unavailabilityReason.clear();
  if (!packagedShaderFormats()) {
    unavailabilityReason =
        "Vulkan shaders are not packaged; enable PLAYGROUND_GPU and rebuild";
    return {};
  }
  if (SDL_GPUSupportsShaderFormats(packagedShaderFormats(), "vulkan"))
    return {{rendering::RendererKind::SDLGPU,
             rendering::GPUDriver::Vulkan,
             {true, true, rendering::CompositionSpace::Linear, true}}};

  unavailabilityReason =
      std::string{"Vulkan GPU unavailable: "} + SDL_GetError();
  // SDL's GPU probe replaces the loader error with a generic backend error.
  // Check the loader separately only on failure to recover the useful cause.
  if (!SDL_Vulkan_LoadLibrary(nullptr))
    unavailabilityReason =
        std::string{"Vulkan loader unavailable: "} + SDL_GetError();
  else
    SDL_Vulkan_UnloadLibrary();
#ifdef __APPLE__
  unavailabilityReason +=
      ". Check the bundled Frameworks/libMoltenVK.dylib and any "
      "SDL_VULKAN_LIBRARY override on macOS";
#endif
  return {};
}

struct GPURenderBackend::Impl {
  SDL_Window &window;
  rendering::GPUDriver driver;
  GPUDeviceHandle device;
  gpu_detail::PaintDevice paint;

  std::unique_ptr<gpu_detail::GPUSceneRenderer> scenes;
  bool active{};
  bool prepared2D{};
  std::optional<bool> vsync;

  Impl(SDL_Window &window, rendering::GPUDriver driver,
       const rendering::RenderBackendProps &props)
      : window{window}, driver{driver},
        device{std::make_shared<GPUDevice>(
            GPUDeviceProps{packagedShaderFormats(), props.gpuDebug,
                           rendering::toString(driver).data(),
                           props.allocations, props.resources})},
        paint{device} {
    device->setProfilingEnabled(true, false);
    if (!SDL_GPUTextureSupportsFormat(
            device->get(), SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER))
      throw std::runtime_error(
          "GPU lacks linear floating-point composition targets");
    if (!SDL_ClaimWindowForGPUDevice(device->get(), &window))
      throwSDLError("Cannot claim GPU window");
  }

  ~Impl() { SDL_ReleaseWindowFromGPUDevice(device->get(), &window); }
};

namespace {
class GPUFrame final : public rendering::RenderFrame,
                       public GPUFrameAccess,
                       private scene::SceneRenderer {
  SDL_Window &_window;
  gpu_detail::PaintDevice &_device;
  std::unique_ptr<gpu_detail::GPUSceneRenderer> &_scenes;
  bool &_active;
  math::Vec2i _size;
  math::ColorRGBA8 _clear;

  std::shared_ptr<gpu_detail::ColorTarget> _target;
  std::optional<gpu_detail::GPUPainter> _painter;

  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &view,
         std::span<const scene::MeshDraw> draws) override {
    if (!_painter)
      throw std::logic_error("GPU frame already presented");
    if (!_scenes)
      _scenes = std::make_unique<gpu_detail::GPUSceneRenderer>(_device);
    return _scenes->render(view, draws);
  }

public:
  GPUDeviceHandle gpuDevice() const override {
    if (!_painter)
      throw std::logic_error("GPU frame already presented");
    _device.device->checkOwnerThread();
    return _device.device;
  }

  rendering::PaintImageHandle renderOffscreen(
      GPUOffscreenProps props,
      const std::function<void(GPURecordingContext)> &record) override {
    auto device = gpuDevice();
    if (!record)
      throw std::invalid_argument(
          "Offscreen rendering requires a recording callback");
    device->limits().validateTarget(props.size, props.depth ? 12 : 8,
                                    "GPU custom offscreen pass");
    auto target = _device.targets.color(props.size);
    std::shared_ptr<gpu_detail::DepthTarget> depth;
    if (props.depth)
      depth = _device.targets.depth(props.size);
    gpu_detail::Commands commands{
        device, "custom offscreen", {.targetPixels = props.size}};
    device->recordTexture(commands.value, target->get());
    if (depth)
      device->recordTexture(commands.value, depth->texture.get());
    const auto clear = math::premultiply(math::toLinear(props.clearColor));
    SDL_GPUColorTargetInfo color{};
    color.texture = target->get();
    color.clear_color = {clear.r, clear.g, clear.b, clear.a};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depthInfo{};
    depthInfo.texture = depth ? depth->texture.get() : nullptr;
    depthInfo.clear_depth = 1;
    depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depthInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    auto *pass = SDL_BeginGPURenderPass(commands.value, &color, 1,
                                        props.depth ? &depthInfo : nullptr);
    if (!pass)
      throwRenderError("Cannot begin custom offscreen pass",
                       rendering::RenderOperation::Record);
    try {
      record({commands.value, pass, *device});
    } catch (...) {
      SDL_EndGPURenderPass(pass);
      throw;
    }
    SDL_EndGPURenderPass(pass);
    commands.submit();
    return target->publish();
  }

  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return _device.device->resourceDomain();
  }

  GPUFrame(SDL_Window &window, gpu_detail::PaintDevice &device,
           std::unique_ptr<gpu_detail::GPUSceneRenderer> &scenes, bool &active,
           std::shared_ptr<gpu_detail::ColorTarget> target, math::Vec2i size,
           math::Vec2f scale, math::ColorRGBA8 clear,
           rendering::FrameLease admission)
      : _window{window}, _device{device}, _scenes{scenes}, _active{active},
        _size{size}, _clear{clear}, _target{std::move(target)},
        _painter{std::in_place, device, scale} {
    _admission = admission;
    _device.device->beginFrame(std::move(admission));
    _active = true;
  }

  ~GPUFrame() override {
    _painter.reset();
    _device.device->endFrame();
    _active = false;
  }

  rendering::PaintContext &paint2D() override {
    if (!_painter)
      throw std::logic_error("GPU frame already presented");
    return *_painter;
  }

  scene::SceneRenderer *scene3D() noexcept override { return this; }

  rendering::PresentationOutcome present() override {
    if (!_painter)
      throw std::logic_error("GPU frame already presented");
    _painter->finish(_target->get(), _size, _clear);
    _painter.reset();
    gpu_detail::GPUPainter compositor{_device, {1, 1}};
    auto *commands =
        _device.device->acquireCommands("presentation composition");
    SDL_GPUTexture *swapchain{};

    struct PresentGuard {
      GPUDevice &device;
      SDL_GPUCommandBuffer *commands;
      SDL_GPUTexture *&swapchain;

      ~PresentGuard() {
        if (commands) {
          if (swapchain)
            device.finishAbandonedPresentation(commands);
          else
            device.cancel(commands);
        }
      }
    } guard{*_device.device, commands, swapchain};

    Uint32 w{}, h{};
    if (!SDL_AcquireGPUSwapchainTexture(commands, &_window, &swapchain, &w, &h))
      throwRenderError("Cannot acquire GPU swapchain",
                       rendering::RenderOperation::Acquire);
    if (!swapchain)
      return rendering::PresentationOutcome::Skipped;
    _device.device->setTimingContext(
        commands,
        {.targetPixels = math::Vec2i{int(w), int(h)}, .sourcePixels = _size});
    // Use the acquired size: a native resize can occur after beginFrame.
    // PresentGuard also releases the acquired image if recording throws.
    compositor.drawImage(_target->publish(), {{}, _target->pixelSize()},
                         math::rect(0, 0, float(w), float(h)), {});
    compositor.encode(
        commands, swapchain, {int(w), int(h)},
        SDL_GetGPUSwapchainTextureFormat(_device.device->get(), &_window),
        true);
    _device.device->submit(commands);
    guard.commands = nullptr;
    return rendering::PresentationOutcome::Submitted;
  }
};
} // namespace

GPURenderBackend::GPURenderBackend(SDL_Window &window,
                                   rendering::GPUDriver driver,
                                   const rendering::RenderBackendProps &props)
    : _impl{[&] {
        if (driver != rendering::GPUDriver::Vulkan)
          throw std::invalid_argument(
              "Only Vulkan hardware rendering is supported");
        props.validate();
        return std::make_unique<Impl>(window, driver, props);
      }()} {}

GPURenderBackend::~GPURenderBackend() = default;

std::size_t GPURenderBackend::pendingWork() const {
  return _impl->device->pendingSubmissions();
}

void GPURenderBackend::trimUnused() {
  _impl->device->pollCompletions();
  _impl->paint.trimUnused();
  if (_impl->scenes)
    _impl->scenes->trimUnused();
}

void GPURenderBackend::invalidate() noexcept {
  _impl->device->invalidate();
  (void)_impl->device->retireInvalidatedSubmissions();
}

void GPURenderBackend::setProfilingEnabled(bool enabled) {
  _impl->device->setProfilingEnabled(enabled);
}

std::optional<rendering::PaintWork> GPURenderBackend::takePaintWork() {
  auto result = _impl->paint.takeStats();
  if (_impl->scenes)
    result.scene = _impl->scenes->takeWork();
  return result;
}

bool GPURenderBackend::supportsGPUTiming() const noexcept {
  return _impl->device->supportsTimestamps();
}

std::vector<rendering::GPUTimingSample> GPURenderBackend::takeGPUTimings() {
  return _impl->device->takeGPUTimings();
}

rendering::GPUTimingCollection GPURenderBackend::gpuTimingCollection() const {
  return _impl->device->gpuTimingCollection();
}

rendering::ResourceDomainId GPURenderBackend::resourceDomain() const noexcept {
  return _impl->device->resourceDomain();
}

std::uint64_t GPURenderBackend::completedWork() {
  _impl->device->pollCompletions();
  return _impl->device->completedSubmission();
}

rendering::RendererCandidate GPURenderBackend::description() const {
  return {rendering::RendererKind::SDLGPU,
          _impl->driver,
          {true, true, rendering::CompositionSpace::Linear, true}};
}

void GPURenderBackend::prepare(rendering::RendererRequirements requirements) {
  rendering::RenderBackend::prepare(requirements);
  if (_impl->active)
    throw std::logic_error("Cannot prepare capabilities during a live frame");
  if ((requirements.paint2D || requirements.linearComposition) &&
      !_impl->prepared2D) {
    auto target = _impl->paint.targets.color({1, 1});
    gpu_detail::GPUPainter painter{_impl->paint, {1, 1}};
    painter.finish(target->get(), {1, 1}, {});
    _impl->prepared2D = true;
  }
  if ((requirements.scene3D || requirements.metallicRoughness) &&
      !_impl->scenes) {
    if (!SDL_GPUTextureSupportsFormat(
            _impl->device->get(), SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
      throw std::runtime_error("GPU lacks scene depth targets");
    auto candidate =
        std::make_unique<gpu_detail::GPUSceneRenderer>(_impl->paint);
    scene::SceneRenderProps probe{};
    probe.pixelSize = {1, 1};
    (void)candidate->render(probe, {});
    _impl->scenes = std::move(candidate);
  }
}

math::Vec2i GPURenderBackend::drawableSize() const {
  math::Vec2i size;
  if (!SDL_GetWindowSizeInPixels(&_impl->window, &size.x, &size.y))
    throwRenderError("Cannot query GPU drawable size",
                     rendering::RenderOperation::Query);
  return size;
}

std::unique_ptr<rendering::RenderFrame>
GPURenderBackend::beginFrame(rendering::RenderFrameProps props) {
  if (_impl->active)
    throw std::logic_error("GPU frame already active");
  props.settings.validate();
  _impl->paint.images.glyphAtlases = props.settings.glyphAtlases;
  if (_impl->vsync != props.settings.vsync) {
    auto mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!props.settings.vsync &&
        SDL_WindowSupportsGPUPresentMode(_impl->device->get(), &_impl->window,
                                         SDL_GPU_PRESENTMODE_IMMEDIATE))
      mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    if (!SDL_SetGPUSwapchainParameters(_impl->device->get(), &_impl->window,
                                       SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
      throwRenderError("Cannot configure GPU presentation",
                       rendering::RenderOperation::Present);
    _impl->vsync = props.settings.vsync;
  }
  const auto drawable = drawableSize();
  if ((SDL_GetWindowFlags(&_impl->window) & SDL_WINDOW_MINIMIZED) ||
      !math::hasArea(drawable))
    return {};
  const auto size = props.settings.targetSize(drawable);
  std::shared_ptr<gpu_detail::ColorTarget> target;
  try {
    target = _impl->paint.targets.color(size, "GPU frame");
  } catch (const rendering::ResourcePressure &) {
    throw;
  } catch (const std::length_error &error) {
    throw std::length_error(
        std::format("{}; drawable {}x{}, resolution scale {}", error.what(),
                    drawable.x, drawable.y, props.settings.resolutionScale));
  }
  int width{}, height{};
  if (!SDL_GetWindowSize(&_impl->window, &width, &height) || width <= 0 ||
      height <= 0)
    throwRenderError("Cannot query GPU logical size",
                     rendering::RenderOperation::Query);
  return std::make_unique<GPUFrame>(
      _impl->window, _impl->paint, _impl->scenes, _impl->active,
      std::move(target), size,
      math::Vec2f{float(size.x) / width, float(size.y) / height},
      props.clearColor, std::move(props.admission));
}

} // namespace playground::sdl
