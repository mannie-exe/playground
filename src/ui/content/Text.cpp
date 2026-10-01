#include <utility>
#include <vector>

#include <platform/sdl/FontTextSource.hpp>
#include <platform/sdl/SDLGeometry.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <ui/content/Text.hpp>

namespace playground::ui {

void Text::validate(const TextProps &props) {
  (void)graphemeBoundaries(props.value);
  (void)graphemeBoundaries(props.flow.ellipsis);
  if (props.flow.maximumLines && !*props.flow.maximumLines)
    throw std::invalid_argument("Maximum line count must be positive");
  if (props.flow.truncation < TextTruncation::None ||
      props.flow.truncation > TextTruncation::EllipsisEnd)
    throw std::invalid_argument("Unknown truncation mode");
  if (props.flow.writingMode < WritingMode::HorizontalTb ||
      props.flow.writingMode > WritingMode::VerticalLr ||
      props.flow.orientation < TextOrientation::Mixed ||
      props.flow.orientation > TextOrientation::Sideways)
    throw std::invalid_argument("Invalid text writing mode or orientation");
  if (props.flow.writingMode != WritingMode::HorizontalTb &&
      props.method == TextMethod::LCD)
    throw std::invalid_argument("LCD subpixel rendering is horizontal-only");
  if (!props.font)
    throw std::invalid_argument("Text requires a font handle");
  if ((props.method != TextMethod::Blended &&
       props.method != TextMethod::Solid &&
       props.method != TextMethod::Shaded && props.method != TextMethod::LCD) ||
      (props.wrap != TextWrap::None &&
       props.wrap != TextWrap::AvailableInlineSize) ||
      (props.fontFit != FontFit::None && props.fontFit != FontFit::ShrinkToFit))
    throw std::invalid_argument(
        "Invalid text rendering, wrapping, or font fitting mode");
  if (!std::isfinite(props.minFontSize) || !std::isfinite(props.maxFontSize) ||
      props.minFontSize <= 0 || props.maxFontSize < props.minFontSize ||
      !std::isfinite(props.fitStep) || props.fitStep <= 0 ||
      (props.maxFontSize - props.minFontSize) / props.fitStep > 4096)
    throw std::invalid_argument(
        "Invalid bounded font fit interval (at most 4096 steps)");
  if (props.paragraphAlignment != layout::Align::Start &&
      props.paragraphAlignment != layout::Align::Center &&
      props.paragraphAlignment != layout::Align::End)
    throw std::invalid_argument("Invalid paragraph alignment");
  content_detail::validate({.alignment = props.contentAlignment});
}

Text::Measurement Text::atSize(float size,
                               const layout::SizeConstraints &offered,
                               layout::LayoutDirection direction, float scale) {
  auto fontProps = _props.font->props();
  fontProps.style.size = size * scale;
  fontProps.style.outline =
      sdl::checkedPixel(static_cast<double>(fontProps.style.outline) * scale);
  if (fontProps.layout.lineSpace)
    fontProps.layout.lineSpace = std::max(
        1, sdl::checkedPixel(static_cast<double>(*fontProps.layout.lineSpace) *
                             scale));
  fontProps.layout.alignment =
      _props.paragraphAlignment == layout::Align::Center
          ? TTF_HORIZONTAL_ALIGN_CENTER
      : ((_props.paragraphAlignment == layout::Align::End) !=
         (direction == layout::LayoutDirection::RightToLeft))
          ? TTF_HORIZONTAL_ALIGN_RIGHT
          : TTF_HORIZONTAL_ALIGN_LEFT;
  auto font = _assets.getFont(std::move(fontProps));
  // Avoid losing a pixel when an integer extent made a float / scale round
  // trip.
  constexpr double wrapRoundTripTolerance = 0.0001;
  const int wrap =
      _props.wrap == TextWrap::AvailableInlineSize && offered.width.maximum
          ? std::max(1,
                     sdl::checkedPixel(
                         static_cast<double>(*offered.width.maximum) * scale +
                             wrapRoundTripTolerance,
                         sdl::PixelRounding::Floor))
          : 0;
  if (_props.value.empty())
    return {std::move(font), {}, wrap, scale};
  if (_props.flow.writingMode != WritingMode::HorizontalTb) {
    auto columns = sdl::layoutTextColumns(
        _assets, font, _props.value,
        _props.wrap == TextWrap::AvailableInlineSize && offered.height.maximum
            ? std::optional{*offered.height.maximum * scale}
            : std::nullopt,
        _props.flow.writingMode, _props.flow.orientation,
        _props.paragraphAlignment);
    auto extent = columns.size / scale;
    return {std::move(font), extent,       0,
            scale,           _props.value, std::move(columns)};
  }
  int width{}, height{};
  const bool ok =
      (wrap || _props.value.find_first_of("\r\n") != std::string::npos)
          ? TTF_GetStringSizeWrapped(font->get(), _props.value.data(),
                                     _props.value.size(), wrap, &width, &height)
          : TTF_GetStringSize(font->get(), _props.value.data(),
                              _props.value.size(), &width, &height);
  if (!ok)
    throw std::runtime_error(std::string{"Text measurement: "} +
                             SDL_GetError());
  return {std::move(font),
          {width / scale, height / scale},
          wrap,
          scale,
          _props.value};
}

Text::Measurement Text::measured(const layout::SizeConstraints &offered,
                                 layout::LayoutDirection direction,
                                 UIWorkStats *stats, float scale) {
  float size = _props.font->getSize();
  if (_props.fontFit == FontFit::None)
    return atSize(size, offered, direction, scale);
  size = std::min(size, _props.maxFontSize);
  size = std::max(size, _props.minFontSize);
  for (std::size_t step = 0; step <= 4096; ++step) {
    if (stats)
      ++stats->fontFitAttempts;
    const float candidate = std::max(
        _props.minFontSize, size - static_cast<float>(step) * _props.fitStep);
    auto result = atSize(candidate, offered, direction, scale);
    if ((!offered.width.maximum ||
         result.size.width <= *offered.width.maximum) &&
        (!offered.height.maximum ||
         result.size.height <= *offered.height.maximum))
      return result;
    if (candidate == _props.minFontSize)
      return result;
  }
  throw std::logic_error("Font-fit iteration budget exhausted");
}

const Text::Measurement &Text::resolved(const layout::SizeConstraints &offered,
                                        MeasureContext &context) {
  const float scale = std::max(context.pixelScale.x, context.pixelScale.y);
  for (const auto &entry : _layouts)
    if (entry && entry->constraints == offered &&
        entry->direction == context.direction && entry->scale == scale) {
      if (context.stats)
        ++context.stats->textLayoutHits;
      return entry->result;
    }
  if (context.stats)
    ++context.stats->textLayouts;
  auto result = flowed(
      measured(offered, context.direction, context.stats, scale), offered);
  auto &entry = _layouts[_nextLayout];
  entry = LayoutEntry{offered, context.direction, scale, std::move(result)};
  _nextLayout = (_nextLayout + 1) % _layouts.size();
  return entry->result;
}

Text::Measurement Text::flowed(Measurement result,
                               const layout::SizeConstraints &offered) const {
  if (_props.flow.truncation == TextTruncation::None &&
      !_props.flow.maximumLines)
    return result;
  math::Size2 measuredSize;
  auto measure = [&](std::string_view value) {
    if (value.empty()) {
      measuredSize = {};
      return true;
    }
    if (_props.flow.writingMode != WritingMode::HorizontalTb) {
      result.columns = sdl::layoutTextColumns(
          _assets, result.font, value,
          _props.wrap == TextWrap::AvailableInlineSize && offered.height.maximum
              ? std::optional{*offered.height.maximum * result.scale}
              : std::nullopt,
          _props.flow.writingMode, _props.flow.orientation,
          _props.paragraphAlignment);
      measuredSize = result.columns->size / result.scale;
      return (!offered.width.maximum ||
              measuredSize.width <= *offered.width.maximum) &&
             (!offered.height.maximum ||
              measuredSize.height <= *offered.height.maximum) &&
             (!_props.flow.maximumLines ||
              result.columns->count <= *_props.flow.maximumLines);
    }
    using TextResource = SDLResource<TTF_Text, TTF_DestroyText>;
    TextResource text{requireSDL(
        TTF_CreateText(nullptr, result.font->get(), value.data(), value.size()),
        "Create text layout")};
    if (!TTF_SetTextWrapWidth(text.get(), result.wrap))
      throwSDLError("Set text wrap width");
    int width{}, height{};
    if (!TTF_GetTextSize(text.get(), &width, &height))
      throwSDLError("Measure text flow");
    measuredSize = {width / result.scale, height / result.scale};
    return (!offered.width.maximum ||
            measuredSize.width <= *offered.width.maximum) &&
           (!offered.height.maximum ||
            measuredSize.height <= *offered.height.maximum) &&
           (!_props.flow.maximumLines ||
            static_cast<std::size_t>(text->num_lines) <=
                *_props.flow.maximumLines);
  };
  auto flow = _props.flow;
  if (flow.truncation == TextTruncation::None) {
    flow.truncation = TextTruncation::EllipsisEnd;
    flow.ellipsis.clear();
  }
  result.value = truncateText(_props.value, flow, measure);
  measure(result.value);
  result.size = measuredSize;
  return result;
}

SurfaceHandle Text::rasterize(const Measurement &m) const {
  if (m.columns) {
    return sdl::rasterizeTextColumns(
        *m.columns,
        [&](FontHandle font, const std::string &value) {
          return rasterize(Measurement{std::move(font), {}, 0, m.scale, value});
        },
        _props.method == TextMethod::Shaded ? std::optional{_props.background}
                                            : std::nullopt);
  }
  const auto fg = sdl::toSDL(foreground()), bg = sdl::toSDL(_props.background);
  auto *font = m.font->get();
  auto *text = m.value.data();
  const auto length = m.value.size();
  const bool multiline =
      m.wrap || m.value.find_first_of("\r\n") != std::string::npos;
  SDL_Surface *surface{};
  switch (_props.method) {
  case TextMethod::Blended:
    surface = multiline ? TTF_RenderText_Blended_Wrapped(font, text, length, fg,
                                                         m.wrap)
                        : TTF_RenderText_Blended(font, text, length, fg);
    break;
  case TextMethod::Solid:
    surface = multiline
                  ? TTF_RenderText_Solid_Wrapped(font, text, length, fg, m.wrap)
                  : TTF_RenderText_Solid(font, text, length, fg);
    break;
  case TextMethod::Shaded:
    surface = multiline ? TTF_RenderText_Shaded_Wrapped(font, text, length, fg,
                                                        bg, m.wrap)
                        : TTF_RenderText_Shaded(font, text, length, fg, bg);
    break;
  case TextMethod::LCD:
    surface = multiline ? TTF_RenderText_LCD_Wrapped(font, text, length, fg, bg,
                                                     m.wrap)
                        : TTF_RenderText_LCD(font, text, length, fg, bg);
    break;
  }
  if (!surface)
    throw std::runtime_error(std::string{"Text rasterization: "} +
                             SDL_GetError());
  return adoptManagedSurface(surface);
}

layout::MeasureResult
Text::measureContent(MeasureContext &context,
                     const layout::SizeConstraints &offered) {
  const auto &m = resolved(offered, context);
  if (_props.fontFit == FontFit::ShrinkToFit && context.diagnostics &&
      ((offered.width.maximum && m.size.width > *offered.width.maximum) ||
       (offered.height.maximum && m.size.height > *offered.height.maximum)))
    context.diagnostics->report(id(), LayoutPhase::Measure,
                                LayoutIssue::FontDoesNotFit,
                                "Text does not fit at its minimum font size; "
                                "apply clipping or enlarge the box");
  if (!math::hasArea(m.size))
    return {m.size};
  if (_props.flow.writingMode != WritingMode::HorizontalTb)
    return {m.size};
  const float first =
      std::clamp(static_cast<float>(TTF_GetFontAscent(m.font->get())) / m.scale,
                 0.0f, m.size.height);
  const float descent =
      static_cast<float>(TTF_GetFontDescent(m.font->get())) / m.scale;
  return {m.size, first,
          std::clamp(m.size.height + descent, first, m.size.height)};
}

void Text::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  _direction = context.direction;
  const layout::SizeConstraints offered{{0, bounds.w()}, {0, bounds.h()}};
  _arrangedText = resolved(offered, context);
  _pixelText.reset();
  _prepared = false;
}

void Text::prepareContent(PrepareContext &context) {
  _prepared = false;
  const auto bounds =
      content_detail::contentBounds(this->bounds(), contentInsets());
  if (!_arrangedText)
    throw std::logic_error("Text must be arranged before preparation");
  if (!math::hasArea(_arrangedText->size) || !bounds.hasArea()) {
    _raster.reset();
    _prepared = true;
    return;
  }
  const float scale = std::max(context.pixelScale.x, context.pixelScale.y);
  if (!_pixelText || _pixelDensity != scale ||
      _pixelTextBounds != bounds.size) {
    if (context.stats)
      ++context.stats->textLayouts;
    auto candidate =
        scale == _arrangedText->scale
            ? *_arrangedText
            : atSize(_arrangedText->font->getSize() / _arrangedText->scale,
                     {{0, bounds.w()}, {0, bounds.h()}}, _direction, scale);
    candidate =
        flowed(std::move(candidate), {{0, bounds.w()}, {0, bounds.h()}});
    if (scale != _arrangedText->scale && !candidate.columns) {
      // A render-only scale must not change authored line breaks or truncation.
      const auto lineRanges = [](const Measurement &m) {
        using TextResource = SDLResource<TTF_Text, TTF_DestroyText>;
        TextResource text{
            requireSDL(TTF_CreateText(nullptr, m.font->get(), m.value.data(),
                                      m.value.size()),
                       "Create raster comparison")};
        if (!TTF_SetTextWrapWidth(text.get(), m.wrap) ||
            !TTF_UpdateText(text.get()))
          throwSDLError("Resolve raster lines");
        std::vector<std::pair<int, int>> ranges;
        for (int line = 0; line < text->num_lines; ++line) {
          TTF_SubString span;
          if (!TTF_GetTextSubStringForLine(text.get(), line, &span))
            throwSDLError("Read raster lines");
          ranges.emplace_back(span.offset, span.length);
        }
        return ranges;
      };
      if (candidate.value != _arrangedText->value ||
          lineRanges(candidate) != lineRanges(*_arrangedText))
        candidate = *_arrangedText;
    }
    _pixelText = std::move(candidate);
    _pixelDensity = scale;
    _pixelTextBounds = bounds.size;
  } else if (context.stats)
    ++context.stats->textLayoutHits;
  const auto &m = *_pixelText;
  if (m.value.empty()) {
    _raster.reset();
    _prepared = true;
    return;
  }
  auto key = AssetRegistry::fontKey(m.font->props()) + ":" +
             std::to_string(m.wrap) + ":" +
             std::to_string(static_cast<int>(_props.method));
  for (auto color : {foreground(), _props.background})
    for (auto value : {color.r, color.g, color.b, color.a})
      key += ":" + std::to_string(value);
  key += ":" + std::to_string(static_cast<int>(_props.flow.writingMode)) + ":" +
         std::to_string(static_cast<int>(_props.flow.orientation));
  if (m.columns)
    for (const auto &run : m.columns->runs)
      key += ":" + std::to_string(run.value.size()) + ":" + run.value + ":" +
             std::to_string(run.bounds.x()) + ":" +
             std::to_string(run.bounds.y());
  key += ":" + std::to_string(m.value.size()) + ":" + m.value;
  auto *textPreparer = context.text;
  if (textPreparer && textPreparer->isEnabled() &&
      _props.method == TextMethod::Blended && !m.columns && !m.font->isSDF()) {
    const auto imageDomain = context.images
                                 ? context.images->resourceDomain()
                                 : rendering::ResourceDomainId::cpu();
    if (!_raster || !_atlasRaster || key != _rasterKey ||
        _rasterDomain != textPreparer->resourceDomain() ||
        _rasterImageDomain != imageDomain) {
      if (_rasterDomain != textPreparer->resourceDomain() || _rasterImageDomain != imageDomain)
        _raster.reset();
      auto raster = rendering::prepareTextImage(
          sdl::FontTextSource{m.font, m.value, m.wrap, foreground()},
          *textPreparer);
      _raster = rendering::prepareImage(std::move(raster), context.images);
      _rasterKey = key;
      _rasterDomain = textPreparer->resourceDomain();
      _rasterImageDomain = imageDomain;
      _atlasRaster = true;
      _source.reset();
    }
    _destination = layout::alignBounds(bounds, _arrangedText->size,
                                       _props.contentAlignment, {}, _direction);
    _prepared = true;
    return;
  }
  _atlasRaster = false;
  if (!_source || key != _rasterKey) {
    auto raster = sdl::makeSurfaceImage(
        _assets.getText(key, [&] { return rasterize(m); }));
    _source = std::move(raster);
    _rasterKey = std::move(key);
  }
  _raster = rendering::prepareImage(_source, context.images);
  _destination = layout::alignBounds(bounds, _arrangedText->size,
                                     _props.contentAlignment, {}, _direction);
  _prepared = true;
}

void Text::paint(PaintContext &context) const {
  if (!_prepared)
    throw std::logic_error(
        "Text must be prepared after layout or props changes");
  if (_raster)
    context.drawImage(_raster, {{}, _raster->pixelSize()}, _destination, {});
}

Text::Text(AssetRegistry &assets, TextProps props, layout::BoxProps box)
    : Node{box}, _assets{assets}, _props{std::move(props)} {
  validate(_props);
  setHitTestPolicy(HitTestPolicy::None);
  setSemanticProps({.role = SemanticRole::Text, .name = _props.value});
}

void Text::setProps(TextProps value) {
  validate(value);
  if (value == _props)
    return;
  const bool geometry =
      value.value != _props.value || value.font != _props.font ||
      value.wrap != _props.wrap ||
      value.paragraphAlignment != _props.paragraphAlignment ||
      value.fontFit != _props.fontFit ||
      value.minFontSize != _props.minFontSize ||
      value.maxFontSize != _props.maxFontSize ||
      value.fitStep != _props.fitStep || value.flow != _props.flow;
  _props = std::move(value);
  _prepared = false;
  if (geometry) {
    _layouts = {};
    _arrangedText.reset();
    _pixelText.reset();
    invalidateLayout();
  } else
    invalidatePaint();
  auto s = semanticProps();
  s.name = _props.value;
  setSemanticProps(std::move(s));
}

void Text::setValue(std::string value) {
  auto p = _props;
  p.value = std::move(value);
  setProps(std::move(p));
}

void Text::setFont(FontHandle font) {
  auto p = _props;
  p.font = std::move(font);
  setProps(std::move(p));
}

void Text::applyPatch(const TextPatch &p) {
  const TextProps d;
  setProps({p.value.appliedTo(_props.value, d.value),
            p.font.appliedTo(_props.font),
            p.foreground.appliedTo(_props.foreground, d.foreground),
            p.background.appliedTo(_props.background, d.background),
            p.method.appliedTo(_props.method, d.method),
            p.wrap.appliedTo(_props.wrap, d.wrap),
            p.paragraphAlignment.appliedTo(_props.paragraphAlignment,
                                           d.paragraphAlignment),
            p.contentAlignment.appliedTo(_props.contentAlignment,
                                         d.contentAlignment),
            p.fontFit.appliedTo(_props.fontFit, d.fontFit),
            p.minFontSize.appliedTo(_props.minFontSize, d.minFontSize),
            p.maxFontSize.appliedTo(_props.maxFontSize, d.maxFontSize),
            p.fitStep.appliedTo(_props.fitStep, d.fitStep),
            p.flow.appliedTo(_props.flow, d.flow),
            p.useTheme.appliedTo(_props.useTheme, d.useTheme)});
}

} // namespace playground::ui
