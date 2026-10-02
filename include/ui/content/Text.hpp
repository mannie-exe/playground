#pragma once

#include <array>
#include <optional>
#include <string>

#include <layout/LayoutAlgorithms.hpp>
#include <platform/sdl/TextColumns.hpp>
#include <support/AssetRegistry.hpp>
#include <support/TextFlow.hpp>
#include <ui/Node.hpp>
#include <ui/content/ContentTypes.hpp>

namespace playground::ui {

enum class TextWrap { None, AvailableInlineSize };
enum class FontFit { None, ShrinkToFit };
enum class TextMethod { Blended, Solid, Shaded, LCD };

struct TextProps {
  std::string value;
  FontHandle font;
  std::optional<math::ColorRGBA8> foreground, background;
  TextMethod method{TextMethod::Blended};
  TextWrap wrap{TextWrap::None};
  layout::Align paragraphAlignment{layout::Align::Start};
  layout::Alignment contentAlignment;
  FontFit fontFit{FontFit::None};
  float minFontSize{6};
  float maxFontSize{256};
  float fitStep{0.5f};
  TextFlowProps flow;
  ColorTreatment colorTreatment{ColorTreatment::Adaptive};
  std::optional<TextRole> textRole;
  TextInk ink{TextInk::Primary};
  std::optional<FontFamily> fontFamily;
  std::optional<FontSelection> fontSelection;
  bool operator==(const TextProps &) const = default;
};

struct TextPatch {
  Patch<std::string> value;
  Patch<FontHandle> font;
  Patch<std::optional<math::ColorRGBA8>> foreground, background;
  Patch<TextMethod> method;
  Patch<TextWrap> wrap;
  Patch<layout::Align> paragraphAlignment;
  Patch<layout::Alignment> contentAlignment;
  Patch<FontFit> fontFit;
  Patch<float> minFontSize, maxFontSize, fitStep;
  Patch<TextFlowProps> flow;
  Patch<ColorTreatment> colorTreatment;
  Patch<std::optional<TextRole>> textRole;
  Patch<TextInk> ink;
  Patch<std::optional<FontFamily>> fontFamily;
  Patch<std::optional<FontSelection>> fontSelection;
};

class Text final : public Node {
  struct Measurement {
    FontHandle font;
    math::Size2 size;
    int wrap{};
    float scale{1};
    std::string value;
    std::optional<sdl::TextColumns> columns;
  };

  AssetRegistry &_assets;
  TextProps _props;
  mutable FontHandle _themeFont;
  mutable std::optional<ThemeTypography> _fontTypography;
  FontHandle resolvedFont() const;

  struct LayoutEntry {
    layout::SizeConstraints constraints;
    layout::LayoutDirection direction;
    float scale;
    Measurement result;
  };

  std::array<std::optional<LayoutEntry>, 2> _layouts;
  std::size_t _nextLayout{};

  layout::LayoutDirection _direction{layout::LayoutDirection::LeftToRight};
  std::optional<Measurement> _arrangedText;
  std::optional<Measurement> _pixelText;
  float _pixelDensity{};
  math::Size2 _pixelTextBounds;
  rendering::PaintImageHandle _source;
  rendering::PaintImageHandle _raster;
  math::Rect _destination;
  std::string _rasterKey;
  rendering::ResourceDomainId _rasterDomain{};
  rendering::ResourceDomainId _rasterImageDomain{};
  bool _atlasRaster{};
  bool _prepared{};

  static void validate(const TextProps &props);
  Measurement atSize(float size, const layout::SizeConstraints &offered,
                     layout::LayoutDirection direction, float scale = 1);
  Measurement measured(const layout::SizeConstraints &offered,
                       layout::LayoutDirection direction, UIWorkStats *stats,
                       float scale);
  const Measurement &resolved(const layout::SizeConstraints &offered,
                              MeasureContext &context);
  Measurement flowed(Measurement result,
                     const layout::SizeConstraints &offered) const;

  SurfaceHandle rasterize(const Measurement &m) const;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;
  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;
  void prepareContent(PrepareContext &context) override;
  void paint(PaintContext &context) const override;

  void onThemeChanged() noexcept override;

  math::ColorRGBA8 foreground() const {
    if (_props.colorTreatment == ColorTreatment::PreserveArtwork)
      return _props.foreground.value_or(theme().text);
    if (!isEffectivelyEnabled())
      return theme().mutedText;
    const auto role =
        _props.ink == TextInk::Secondary  ? &ThemePalette::mutedText
        : _props.ink == TextInk::Error    ? &ThemePalette::error
        : _props.ink == TextInk::Warning  ? &ThemePalette::warning
        : _props.ink == TextInk::Success  ? &ThemePalette::success
        : _props.ink == TextInk::OnAccent ? &ThemePalette::onAccent
                                          : &ThemePalette::text;
    return resolveColor(role, _props.foreground);
  }

  math::ColorRGBA8 background() const {
    return _props.colorTreatment == ColorTreatment::PreserveArtwork
               ? _props.background.value_or(theme().surface)
               : resolveColor(&ThemePalette::surface, _props.background);
  }

public:
  Text(AssetRegistry &assets, TextProps props, layout::BoxProps box = {});

  const TextProps &props() const noexcept { return _props; }

  std::string_view displayedValue() const noexcept {
    return _arrangedText ? std::string_view{_arrangedText->value}
                         : std::string_view{_props.value};
  }

  bool isTruncated() const noexcept { return displayedValue() != _props.value; }

  FontHandle effectiveFont() const {
    return _arrangedText ? _arrangedText->font : resolvedFont();
  }

  void setProps(TextProps value);
  void setValue(std::string value);
  void setFont(FontHandle font);
  void applyPatch(const TextPatch &p);
};

} // namespace playground::ui
