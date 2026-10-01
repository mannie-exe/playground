#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

#include "GPUPainter.hpp"

namespace playground::sdl::gpu_detail {
namespace {
std::array<float, 4> color(math::ColorRGBA8 value) {
  const auto linear = math::toLinear(value);
  return {linear.r, linear.g, linear.b, linear.a};
}

std::array<float, 4> rect(math::Rect value) {
  return {value.x(), value.y(), value.w(), value.h()};
}
} // namespace

PaintDevice::PaintDevice(GPUDeviceHandle value)
    : _vertex{loadShader(value, "paint_vertex", SDL_GPU_SHADERSTAGE_VERTEX, 0,
                         1, 1)},
      _fragment{loadShader(value, "paint_fragment",
                           SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 2, 1)},
      _rectFragment{loadShader(value, "paint_rect_fragment",
                               SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 1)},
      _presentFragment{loadShader(value, "present_fragment",
                                  SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 1)},
      _sampler{sampler(value)},
      _linearSampler{sampler(value, SDL_GPU_FILTER_LINEAR)},
      _drawStream{value, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ},
      device{std::move(value)}, images{*this, device}, targets{device},
      white{std::make_shared<GPUImage>(
          device, rendering::RGBA8Image{.size = {1, 1},
                                        .pixels = {255, 255, 255, 255}})} {}

rendering::ResourceDomainId ContentPreparer::resourceDomain() const noexcept {
  return _paint.device->resourceDomain();
}

rendering::PaintImageHandle
ContentPreparer::prepareText(const rendering::TextSource &source) {
  _paint.device->checkOwnerThread();
  const auto *request = dynamic_cast<const FontTextSource *>(&source);
  if (!request)
    throw std::invalid_argument(
        "GPU text preparer requires an SDL font source");
  if (!glyphAtlases)
    throw std::logic_error("GPU atlas preparation is disabled");
  if (!request->font || request->wrapWidth < 0)
    throw std::invalid_argument(
        "GPU text preparation requires a font and nonnegative wrap width");
  if (!_textEngine)
    _textEngine = std::make_shared<GPUTextEngine>(_paint.device);
  GPUText text{_textEngine, request->font, request->value};
  text.setWrapWidth(request->wrapWidth);
  auto size = text.size();
  size.x = std::max(1, size.x);
  size.y = std::max(1, size.y);
  _paint.device->limits().validateTarget(size, 8, "GPU text raster");
  const auto *sequenceHead = text.drawData();
  auto target = _paint.targets.color(size);
  std::vector<Draw> draws;
  for (auto *sequence = sequenceHead; sequence; sequence = sequence->next) {
    if (sequence->num_vertices % 4 != 0)
      throw std::runtime_error("Unexpected SDL_ttf atlas quad geometry");
    if (sequence->image_type == TTF_IMAGE_SDF)
      throw std::invalid_argument(
          "SDF text requires the raster compatibility path");
    for (int i = 0; i < sequence->num_vertices; i += 4) {
      const auto *xy = sequence->xy + i;
      const auto bounds =
          math::rect(xy[0].x, -xy[0].y, xy[2].x - xy[0].x, xy[0].y - xy[2].y);
      if (!bounds.hasArea())
        continue;
      Draw draw;
      draw.bounds = bounds;
      draw.texture = sequence->atlas_texture ? sequence->atlas_texture
                                             : _paint.white->get();
      auto &u = draw.uniforms;
      u.tint = color(request->foreground);
      if (sequence->image_type == TTF_IMAGE_COLOR)
        u.tint = {1, 1, 1, request->foreground.a / 255.f};
      u.outer = {{1, 0, 0, 0}, {0, 1, 0, 0}, rect(bounds), {}, {}};
      u.inner = u.outer;
      u.options = {0, 0, sequence->atlas_texture ? 1.f : 0.f, 0};
      // FreeType's color glyph bytes are premultiplied sRGB; alpha-only
      // glyphs instead carry white RGB and coverage in alpha.
      u.imageOptions = {sequence->image_type == TTF_IMAGE_COLOR ? 1.f : 0.f, 1,
                        1, sequence->image_type == TTF_IMAGE_ALPHA ? 1.f : 0.f};
      if (sequence->atlas_texture) {
        const auto *uv = sequence->uv + i;
        u.source = {uv[0].x, uv[0].y, uv[2].x - uv[0].x, uv[2].y - uv[0].y};
      }
      draws.push_back(std::move(draw));
    }
  }
  Commands commands{_paint.device, "text atlas", {.targetPixels = size}};
  _paint.encode(commands.value, target->get(), size,
                SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, {0, 0, 0, 0}, draws);
  commands.submit();
  _lastTextPreparation = TextPreparation::Atlas;
  return target->publish();
}

void PaintDevice::encode(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *target,
                         math::Vec2i size, SDL_GPUTextureFormat format,
                         math::ColorRGBA8 clear, std::span<const Draw> draws,
                         bool encoded) {
  device->checkOwnerThread();
  device->recordTexture(commands, target);
  for (const auto &draw : draws)
    device->recordTexture(commands, draw.texture);
  const auto pipelineFor = [&](int kind) -> SDL_GPUGraphicsPipeline * {
    auto found = _pipelines.find({format, kind});
    if (found != _pipelines.end())
      return found->second.get();
    SDL_GPUColorTargetDescription targetDescription{format, blend()};
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = _vertex.get();
    info.fragment_shader = kind == 2   ? _presentFragment.get()
                           : kind == 1 ? _rectFragment.get()
                                       : _fragment.get();
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.target_info.color_target_descriptions = &targetDescription;
    info.target_info.num_color_targets = 1;
    auto *nativePipeline = SDL_CreateGPUGraphicsPipeline(device->get(), &info);
    if (!nativePipeline)
      throwSDLError("Cannot create 2D GPU pipeline");
    auto pipeline = Pipeline{device, nativePipeline};
    return _pipelines.emplace(std::pair{format, kind}, std::move(pipeline))
        .first->second.get();
  };
  // Build pipelines before beginning a native pass (creation may throw).
  for (const auto &draw : draws)
    pipelineFor(draw.rectangular && rectangularFastPath ? 1 : 0);
  pipelineFor(2);
  const auto c = math::premultiply(math::toLinear(clear));
  SDL_GPUColorTargetInfo output{};
  output.texture = target;
  output.load_op = SDL_GPU_LOADOP_CLEAR;
  output.store_op = SDL_GPU_STOREOP_STORE;
  output.clear_color = {c.r, c.g, c.b, c.a};

  struct DrawData {
    std::array<float, 4> bounds;
    PaintUniform paint;
  };

  static_assert(sizeof(DrawData) == 2816);
  const auto chunkCapacity = std::min(device->limits().maxStreamBytes,
                                      device->limits().maxUploadBytes) /
                             sizeof(DrawData);
  if (chunkCapacity == 0 && !draws.empty())
    throw std::length_error("Streaming budget cannot hold one paint record");
  std::array<float, 4> vertex{float(size.x), float(size.y), 0, 0};
  const std::array<float, 4> fragment{encoded ? 1.f : 0.f, 0, 0, 0};
  const Draw::PathUniform noPath{};
  std::vector<DrawData> records;
  records.reserve(std::min(chunkCapacity, draws.size()));
  std::size_t offset{};
  do {
    const auto count = std::min(chunkCapacity, draws.size() - offset);
    records.clear();
    for (std::size_t i = 0; i < count; ++i)
      records.push_back(
          {rect(draws[offset + i].bounds), draws[offset + i].uniforms});
    SDL_GPUBuffer *stream{};
    if (!records.empty()) {
      const auto bytes = std::as_bytes(std::span{records});
      stream = _drawStream.write(commands, bytes);
      _stats.streamedBytes += bytes.size();
    }
    auto *pass = SDL_BeginGPURenderPass(commands, &output, 1, nullptr);
    if (!pass)
      throwRenderError("Cannot begin GPU 2D pass",
                       rendering::RenderOperation::Record);
    if (stream) {
      SDL_BindGPUVertexStorageBuffers(pass, 0, &stream, 1);
      SDL_BindGPUFragmentStorageBuffers(pass, 0, &stream, 1);
    }
    SDL_PushGPUFragmentUniformData(commands, 0, fragment.data(),
                                   sizeof(fragment));
    for (std::size_t first = 0; first < count;) {
      const auto &draw = draws[offset + first];
      const auto scissorFor = [&](const Draw &value) {
        const auto bounds =
            value.clipBounds ? math::intersect(*value.clipBounds,
                                               math::rect(0, 0, float(size.x),
                                                          float(size.y)))
                             : math::rect(0, 0, float(size.x), float(size.y));
        if (!bounds.hasArea())
          return math::Rect{};
        const int x = int(std::floor(bounds.x())),
                  y = int(std::floor(bounds.y()));
        return math::rect(float(x), float(y),
                          std::max(0.f, std::ceil(bounds.right()) - x),
                          std::max(0.f, std::ceil(bounds.bottom()) - y));
      };
      const auto scissor = scissorFor(draw);
      if (!scissor.hasArea()) {
        ++_stats.rejected;
        ++first;
        continue;
      }
      std::size_t end = first + 1;
      // Preserve order: batch adjacent quads only. Paths have per-batch
      // uniforms.
      if (!draw.path)
        while (end < count && !draws[offset + end].path &&
               draws[offset + end].rectangular == draw.rectangular &&
               scissorFor(draws[offset + end]) == scissor &&
               draws[offset + end].texture == draw.texture)
          ++end;
      int kind = draw.rectangular && rectangularFastPath ? 1 : 0;
      if (encoded && draws.size() == 1 && kind == 1 &&
          draw.uniforms.inner.bounds ==
              rect(math::rect(0, 0, float(size.x), float(size.y))) &&
          draw.uniforms.options[2] > .5f)
        kind = 2;
      SDL_BindGPUGraphicsPipeline(pass, pipelineFor(kind));
      const SDL_Rect nativeScissor{int(scissor.x()), int(scissor.y()),
                                   int(scissor.w()), int(scissor.h())};
      SDL_SetGPUScissor(pass, &nativeScissor);
      const auto &path = draw.path ? *draw.path : noPath;
      vertex[2] = static_cast<float>(first);
      SDL_PushGPUVertexUniformData(commands, 0, vertex.data(), sizeof(vertex));
      if (!kind)
        SDL_PushGPUFragmentUniformData(commands, 1, &path, sizeof(path));
      const SDL_GPUTextureSamplerBinding binding{
          draw.texture, kind ? _linearSampler.get() : _sampler.get()};
      SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
      SDL_DrawGPUPrimitives(pass, 6, static_cast<Uint32>(end - first), 0, 0);
      ++_stats.drawCalls;
      _stats.quads += end - first;
      if (kind == 2)
        _stats.presentationQuads += end - first;
      else if (kind == 1)
        _stats.rectangularQuads += end - first;
      else
        _stats.generalQuads += end - first;
      first = end;
    }
    SDL_EndGPURenderPass(pass);
    output.load_op = SDL_GPU_LOADOP_LOAD;
    offset += count;
  } while (offset < draws.size());
}

Shape GPUPainter::shape(math::RoundedRect value) const {
  value.validate();
  const auto inverse = _state.transform.inverse();
  if (!inverse)
    throw std::invalid_argument("Singular GPU paint transform");
  const auto radii = value.radii.resolved(value.bounds.size);
  return {{inverse->a, inverse->c, inverse->tx, 0},
          {inverse->b, inverse->d, inverse->ty, 0},
          rect(value.bounds),
          {radii.topLeft.width, radii.topLeft.height, radii.topRight.width,
           radii.topRight.height},
          {radii.bottomRight.width, radii.bottomRight.height,
           radii.bottomLeft.width, radii.bottomLeft.height}};
}

void GPUPainter::restore() noexcept {
  const auto minimum = _layers.empty() ? 0 : _layers.back().saveDepth;
  if (_states.size() > minimum) {
    _state = std::move(_states.back());
    _states.pop_back();
  }
}

void GPUPainter::transform(math::Transform2D value) {
  const auto composed = _state.transform * value;
  for (float component : {composed.a, composed.b, composed.c, composed.d,
                          composed.tx, composed.ty})
    if (!std::isfinite(component))
      throw std::invalid_argument("Nonfinite GPU transform");
  _state.transform = composed;
}

math::Vec2f GPUPainter::pixelScale() const noexcept {
  return {std::hypot(_state.transform.a, _state.transform.b),
          std::hypot(_state.transform.c, _state.transform.d)};
}

void GPUPainter::clipRounded(math::RoundedRect value) {
  if (_state.clips.size() == 32)
    throw std::length_error("GPU clip nesting exceeds 32 masks");
  const auto exact = shape(value);
  const auto bounds = _state.transform.mapBounds(value.bounds);
  if (!math::isFinite(bounds))
    throw std::overflow_error("GPU clip bounds overflow");
  _state.clips.push_back(exact);
  _state.clipBounds =
      _state.clipBounds ? math::intersect(*_state.clipBounds, bounds) : bounds;
  _state.rectangularClips &= _state.transform.b == 0 &&
                             _state.transform.c == 0 &&
                             exact.top == std::array<float, 4>{} &&
                             exact.bottom == std::array<float, 4>{};
}

bool GPUPainter::draw(math::RoundedRect outer,
                      std::optional<math::RoundedRect> inner,
                      math::ColorRGBA8 tint, math::ColorRGBA8 border,
                      rendering::PaintImageHandle image, math::Rect source,
                      rendering::Sampling sampling) {
  outer.validate();
  _device.considered();
  if (!outer.bounds.hasArea() || !_state.transform.inverse())
    return false;
  Draw command;
  command.bounds = _state.transform.mapBounds(outer.bounds);
  if (!math::isFinite(command.bounds))
    throw std::overflow_error("GPU transformed bounds overflow");
  command.clipBounds = _state.clipBounds;
  const auto visible = _state.clipBounds
                           ? math::intersect(command.bounds, *_state.clipBounds)
                           : command.bounds;
  if (!visible.hasArea()) {
    _device.rejected();
    return false;
  }
  // Visit partially covered edge pixels; the shader applies exact shape
  // coverage.
  command.bounds = math::rect(
      std::floor(command.bounds.x()), std::floor(command.bounds.y()),
      std::ceil(command.bounds.right()) - std::floor(command.bounds.x()),
      std::ceil(command.bounds.bottom()) - std::floor(command.bounds.y()));
  auto &u = command.uniforms;
  u.outer = shape(outer);
  u.inner = inner ? shape(*inner) : u.outer;
  command.rectangular = !inner && _state.rectangularClips &&
                        _state.transform.b == 0 && _state.transform.c == 0 &&
                        u.outer.top == std::array<float, 4>{} &&
                        u.outer.bottom == std::array<float, 4>{};
  if (command.rectangular)
    u.inner.bounds = rect(visible);
  u.tint = color(tint);
  u.borderTint = color(border);
  u.options = {float(_state.clips.size()), inner ? 1.f : 0.f, image ? 1.f : 0.f,
               0};
  std::copy(_state.clips.begin(), _state.clips.end(), u.clips.begin());
  if (image) {
    command.owner = _device.images.prepare(std::move(image));
    const auto gpu = std::dynamic_pointer_cast<const GPUImage>(command.owner);
    const auto size = gpu->pixelSize();
    command.texture = gpu->get();
    u.source = {source.x() / size.width, source.y() / size.height,
                source.w() / size.width, source.h() / size.height};
    u.imageOptions = {
        gpu->alphaMode() == rendering::AlphaMode::Premultiplied ? 1.f : 0.f,
        gpu->colorEncoding() == rendering::ColorEncoding::SRGB ? 1.f : 0.f,
        sampling == rendering::Sampling::Linear ? 1.f : 0.f, 0};
  } else {
    command.owner = _device.white;
    command.texture = _device.white->get();
  }
  _draws.push_back(std::move(command));
  return true;
}

void GPUPainter::fillRounded(math::RoundedRect bounds, math::ColorRGBA8 tint) {
  draw(bounds, {}, tint, {});
}

void GPUPainter::drawPath(const math::Path2D &path,
                          const rendering::PathPaint &paint) {
  paint.validate();
  const auto scale = pixelScale();
  const float tolerance = 0.25f / std::max(1.f, std::hypot(scale.x, scale.y));
  const auto record = [&](math::ColorRGBA8 tint, bool stroke) {
    const auto flat = math::flattenPath(path, {.tolerance = tolerance,
                                               .maximumSegments = 255,
                                               .closeOpenContours = !stroke});
    if (flat.segments.empty() || !tint.a || !_state.transform.inverse())
      return;
    const float radius = stroke ? paint.strokeWidth / 2 : 0;
    const auto bounds =
        math::rect(flat.bounds.x() - radius, flat.bounds.y() - radius,
                   flat.bounds.w() + 2 * radius, flat.bounds.h() + 2 * radius);
    if (!bounds.hasArea())
      return;
    auto data = std::make_shared<Draw::PathUniform>();
    data->options = {float(flat.segments.size()),
                     paint.fillRule == math::FillRule::EvenOdd ? 1.f : 0.f,
                     radius, stroke ? 1.f : 0.f};
    for (std::size_t i = 0; i < flat.segments.size(); ++i) {
      const auto &s = flat.segments[i];
      data->segments[i] = {s.from.x, s.from.y, s.to.x, s.to.y};
    }
    if (draw({bounds, {}}, {}, tint, {})) {
      _draws.back().rectangular = false;
      _draws.back().path = std::move(data);
    }
  };
  if (paint.fill)
    record(*paint.fill, false);
  if (paint.stroke && paint.strokeWidth > 0)
    record(*paint.stroke, true);
}

void GPUPainter::strokeRounded(math::RoundedRect bounds, math::Insets widths,
                               math::ColorRGBA8 tint) {
  draw(bounds, bounds.inset(widths), {0, 0, 0, 0}, tint);
}

void GPUPainter::paintRoundedBox(math::RoundedRect bounds, math::Insets widths,
                                 math::ColorRGBA8 tint,
                                 std::optional<math::ColorRGBA8> border) {
  draw(bounds, bounds.inset(widths), tint,
       border.value_or(math::ColorRGBA8{0, 0, 0, 0}));
}

void GPUPainter::drawImage(const rendering::PaintImageHandle &image,
                           math::Rect source, math::Rect destination,
                           rendering::ImagePaint appearance) {
  if (!image || !math::isFinite(source) || !math::isNonNegative(source.size) ||
      source.x() < 0 || source.y() < 0 ||
      source.right() > image->pixelSize().width ||
      source.bottom() > image->pixelSize().height)
    throw std::invalid_argument("Invalid GPU image source rectangle");
  if (source.hasArea())
    draw({destination, {}}, {}, appearance.tint, {}, image, source,
         appearance.sampling);
}

void GPUPainter::beginLayer(math::Rect bounds, float opacity) {
  if (!std::isfinite(opacity) || opacity < 0 || opacity > 1 ||
      !math::isFinite(bounds) || !math::isNonNegative(bounds.size))
    throw std::invalid_argument("Invalid GPU layer");
  // Keep coordinates/clips in parent pixels. Transparent pixels outside bounds
  // are clipped when the finished target is composed back into the parent.
  _layers.push_back({_state, {}, bounds, opacity, _states.size()});
  _layers.back().parent.swap(_draws);
  try {
    clip(bounds);
  } catch (...) {
    cancelLayer();
    throw;
  }
}

void GPUPainter::cancelLayer() noexcept {
  if (_layers.empty())
    return;
  auto layer = std::move(_layers.back());
  _layers.pop_back();
  _state = std::move(layer.state);
  _draws = std::move(layer.parent);
  _states.resize(layer.saveDepth);
}

void GPUPainter::endLayer() {
  if (_layers.empty())
    throw std::logic_error("No GPU layer to finish");
  auto &layer = _layers.back();
  const auto worldExtent = layer.state.transform.mapBounds(layer.bounds);
  const auto extent =
      math::rect(std::floor(worldExtent.x()), std::floor(worldExtent.y()),
                 std::ceil(worldExtent.right()) - std::floor(worldExtent.x()),
                 std::ceil(worldExtent.bottom()) - std::floor(worldExtent.y()));
  if (!extent.hasArea() || layer.opacity == 0) {
    cancelLayer();
    return;
  }
  // Render the layer with a translated pixel origin; clip shapes use that same
  // origin.
  auto draws = std::move(_draws);
  const auto limit = _device.device->limits().maxTextureDimension;
  if (!math::isFinite(extent) || extent.w() > limit || extent.h() > limit ||
      static_cast<double>(extent.w()) > std::numeric_limits<int>::max() ||
      static_cast<double>(extent.h()) > std::numeric_limits<int>::max())
    throw std::length_error(std::format(
        "GPU layer composition dimensions exceed limit: {}x{} pixels at ({}, {}); "
        "maximum {} pixels/axis",
        extent.w(), extent.h(), extent.x(), extent.y(), limit));
  const auto size =
      math::Vec2i{int(std::ceil(extent.w())), int(std::ceil(extent.h()))};
  _device.device->limits().validateTarget(size, 8, "GPU layer composition");
  for (auto &draw : draws) {
    draw.bounds.position.x -= extent.x();
    draw.bounds.position.y -= extent.y();
    if (draw.clipBounds) {
      draw.clipBounds->position.x -= extent.x();
      draw.clipBounds->position.y -= extent.y();
    }
    if (draw.rectangular) {
      draw.uniforms.inner.bounds[0] -= extent.x();
      draw.uniforms.inner.bounds[1] -= extent.y();
    }
    const auto shift = [&](Shape &s) {
      s.rowX[2] += s.rowX[0] * extent.x() + s.rowX[1] * extent.y();
      s.rowY[2] += s.rowY[0] * extent.x() + s.rowY[1] * extent.y();
    };
    shift(draw.uniforms.outer);
    shift(draw.uniforms.inner);
    for (int i = 0; i < int(draw.uniforms.options[0]); ++i)
      shift(draw.uniforms.clips[i]);
  }
  auto target = _device.targets.color(size);
  Commands commands{
      _device.device, "layer composition", {.targetPixels = size}};
  _device.encode(commands.value, target->get(), size,
                 SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, {0, 0, 0, 0}, draws);
  commands.submit();
  GPUPainter composite{_device, {1, 1}};
  composite.drawImage(target->publish(), {{}, target->pixelSize()}, extent, {});
  composite._draws.back().uniforms.tint[3] = layer.opacity;
  // Commit the parent command before popping the layer. If allocation throws,
  // LayerScope still cancels this layer rather than an enclosing one.
  layer.parent.push_back(std::move(composite._draws.back()));
  cancelLayer();
}

rendering::PaintImageHandle GPUPainter::capture(
    math::Rect bounds, math::Vec2f scale,
    const std::function<void(rendering::PaintContext &)> &callback) {
  if (!math::isFinite(bounds) || !bounds.hasArea() || !math::isFinite(scale) ||
      scale.x <= 0 || scale.y <= 0)
    throw std::invalid_argument("Invalid GPU capture bounds");
  const double w = std::ceil(double(bounds.w()) * scale.x),
               h = std::ceil(double(bounds.h()) * scale.y);
  const auto limit = _device.device->limits().maxTextureDimension;
  if (!std::isfinite(w) || !std::isfinite(h) || w > limit || h > limit ||
      w > std::numeric_limits<int>::max() ||
      h > std::numeric_limits<int>::max())
    throw std::length_error(std::format(
        "GPU layer capture dimensions exceed limit: {}x{} logical units at "
        "scale ({}, {}) produces {}x{} pixels; maximum {} pixels/axis",
        bounds.w(), bounds.h(), scale.x, scale.y, w, h, limit));
  const math::Vec2i size{int(w), int(h)};
  _device.device->limits().validateTarget(size, 8, "GPU layer capture");
  auto target = _device.targets.color(size);
  GPUPainter painter{_device, scale};
  painter.translate({-bounds.x(), -bounds.y()});
  callback(painter);
  painter.finish(target->get(), size, {0, 0, 0, 0});
  return target->publish();
}

void GPUPainter::finish(SDL_GPUTexture *target, math::Vec2i size,
                        math::ColorRGBA8 clear) {
  if (!_layers.empty())
    throw std::logic_error("Unclosed GPU layer");
  Commands commands{_device.device, "paint2d", {.targetPixels = size}};
  _device.encode(commands.value, target, size,
                 SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, clear, _draws);
  commands.submit();
}

void GPUPainter::encode(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *target,
                        math::Vec2i size, SDL_GPUTextureFormat format,
                        bool encoded) {
  _device.encode(commands, target, size, format, {0, 0, 0, 0}, _draws, encoded);
}

} // namespace playground::sdl::gpu_detail
