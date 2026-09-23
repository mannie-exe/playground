#pragma once

#include <algorithm>
#include <cmath>
#include <layout/LayoutAlgorithms.hpp>
#include <optional>
#include <platform/sdl/TextColumns.hpp>
#include <string>
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
  math::ColorRGBA8 foreground{255, 255, 255, 255};
  math::ColorRGBA8 background{0, 0, 0, 255};
  TextMethod method{TextMethod::Blended};
  TextWrap wrap{TextWrap::None};
  layout::Align paragraphAlignment{layout::Align::Start};
  layout::Alignment contentAlignment;
  FontFit fontFit{FontFit::None};
  float minFontSize{6};
  float maxFontSize{256};
  float fitStep{0.5f};
  TextFlowProps flow;
  bool operator==(const TextProps &) const = default;
};
struct TextPatch {
  Patch<std::string> value;
  Patch<FontHandle> font;
  Patch<math::ColorRGBA8> foreground, background;
  Patch<TextMethod> method;
  Patch<TextWrap> wrap;
  Patch<layout::Align> paragraphAlignment;
  Patch<layout::Alignment> contentAlignment;
  Patch<FontFit> fontFit;
  Patch<float> minFontSize, maxFontSize, fitStep;
  Patch<TextFlowProps> flow;
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

  layout::LayoutDirection _direction{layout::LayoutDirection::LeftToRight};
  std::optional<Measurement> _arrangedText;
  PaintImageHandle _source;
  PaintImageHandle _raster;
  math::Rect _destination;
  std::string _rasterKey;
  bool _prepared{};

  static void validate(const TextProps &props);
  Measurement atSize(float size, const layout::SizeConstraints &offered,
                     layout::LayoutDirection direction, float scale = 1);
  Measurement measured(const layout::SizeConstraints &offered,
                       layout::LayoutDirection direction);
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

public:
  Text(AssetRegistry &assets, TextProps props, layout::BoxProps box = {});
  const TextProps &props() const noexcept { return _props; }
  std::string_view displayedValue() const noexcept {
    return _arrangedText ? std::string_view{_arrangedText->value}
                         : std::string_view{_props.value};
  }
  bool isTruncated() const noexcept { return displayedValue() != _props.value; }
  FontHandle effectiveFont() const {
    return _arrangedText ? _arrangedText->font : _props.font;
  }
  void setProps(TextProps value);
  void setValue(std::string value);
  void setFont(FontHandle font);
  void applyPatch(const TextPatch &p);
};

} // namespace playground::ui
