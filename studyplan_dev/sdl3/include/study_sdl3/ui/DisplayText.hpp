#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include <study_sdl3/surface/TextSurface.hpp>
#include <study_sdl3/ui/IDisplayObject.hpp>

struct DisplayTextProps {
  bool selectable{false};
  bool wrapToTransform{true};
  bool fitTextWidthToTransform{false};
  DisplayAlignment alignment{};
};

struct DisplayTextPatch {
  std::optional<bool> selectable;
  std::optional<bool> wrapToTransform;
  std::optional<bool> fitTextWidthToTransform;
  std::optional<DisplayAlignment> alignment;
};

class DisplayText : public IDisplayObject {
  TextSurface _text;
  DisplayTextProps _props;
  RectTransform _textTransform;

  void syncTextToTransform() {
    const int width{std::max(0, static_cast<int>(_transform.size.x))};

    if (_props.wrapToTransform)
      _text.setWrapWidth(width);

    if (_props.fitTextWidthToTransform)
      _text.setFontFitWidth(width);

    const SDL_Rect textSize{_text.getSize()};
    RectTransform textTransform{.position = _transform.position,
                                .size = Vec2f{static_cast<float>(textSize.w),
                                              static_cast<float>(textSize.h)}};

    if (_props.alignment.horizontal == HorizontalAlign::Center)
      textTransform.position.x += (_transform.size.x - textSize.w) / 2.0f;
    else if (_props.alignment.horizontal == HorizontalAlign::Right)
      textTransform.position.x += _transform.size.x - textSize.w;

    if (_props.alignment.vertical == VerticalAlign::Middle)
      textTransform.position.y += (_transform.size.y - textSize.h) / 2.0f;
    else if (_props.alignment.vertical == VerticalAlign::Bottom)
      textTransform.position.y += _transform.size.y - textSize.h;

    _textTransform = textTransform;
  }

public:
  explicit DisplayText(TextProps textProps, Font font,
                       const DisplayTextProps props = {},
                       const SurfaceRenderProps render = {},
                       const RectTransform transform = {})
      : IDisplayObject{transform}, _text{std::move(textProps), std::move(font), render},
        _props{props} {
    syncTextToTransform();
  }

  const std::string &getValue() const { return _text.getValue(); }
  DisplayTextProps getProps() const { return _props; }
  TextStyleProps getStyle() const { return _text.getTextProps().style; }
  TextLayoutProps getLayout() const { return _text.getTextProps().layout; }
  const Font &getFont() const { return _text.getFont(); }

  void setValue(const std::string &value) {
    _text.setValue(value);
    syncTextToTransform();
  }
  void setStyle(const TextStylePatch style) {
    _text.setStyle(style);
    syncTextToTransform();
  }
  void setLayout(const TextLayoutPatch layout) {
    _text.setLayout(layout);
    syncTextToTransform();
  }
  void setSelectable(const bool selectable) { _props.selectable = selectable; }
  void setWrapToTransform(bool wrapToTransform) {
    _props.wrapToTransform = wrapToTransform;
    syncTextToTransform();
  }
  void setFitTextWidthToTransform(bool fitTextWidthToTransform) {
    _props.fitTextWidthToTransform = fitTextWidthToTransform;
    syncTextToTransform();
  }
  void setAlignment(const DisplayAlignment alignment) {
    _props.alignment = alignment;
    syncTextToTransform();
  }
  void setProps(const DisplayTextPatch props) {
    if (props.selectable)
      setSelectable(*props.selectable);
    if (props.wrapToTransform)
      setWrapToTransform(*props.wrapToTransform);
    if (props.fitTextWidthToTransform)
      setFitTextWidthToTransform(*props.fitTextWidthToTransform);
    if (props.alignment)
      setAlignment(*props.alignment);
  }
  void replaceFont(Font font) {
    _text.replaceFont(std::move(font));
    syncTextToTransform();
  }

  void setTransform(const RectTransform &transform) override {
    IDisplayObject::setTransform(transform);
    syncTextToTransform();
  }

  void render(SDL_Surface &targetSurface) override {
    _text.renderInto(targetSurface, _textTransform);
  }
};
