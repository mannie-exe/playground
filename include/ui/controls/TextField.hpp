#pragma once

#include <memory>
#include <string>

#include <support/Font.hpp>
#include <ui/Node.hpp>
#include <ui/TextEdit.hpp>

namespace playground::ui {
struct TextFieldProps {
  FontHandle font;
  TextEditProps editing;
  bool enabled{true};
  bool required{};
  std::string name, placeholder, validationMessage;
  math::ColorRGBA8 foreground{240, 240, 240, 255}, background{35, 35, 35, 255},
      selection{50, 90, 140, 255};
  bool useTheme{true};
  bool operator==(const TextFieldProps &) const = default;
};

struct TextFieldPatch {
  Patch<FontHandle> font;
  Patch<TextEditProps> editing;
  Patch<bool> enabled, required;
  Patch<std::string> name, placeholder, validationMessage;
  Patch<math::ColorRGBA8> foreground, background, selection;
  Patch<bool> useTheme;
};

class TextField : public Node, public TextInputClient {
  struct Layout;
  TextFieldProps _props;
  TextEditModel _model;
  Signal<std::string> _changed, _committed;

  std::unique_ptr<Layout> _layout;
  math::Vec2f _scroll{};
  std::optional<math::Point2> _visualCaret;
  std::optional<std::uint64_t> _drag;
  void rebuild(float width);
  void changed(bool edit);
  void revealCaret();
  std::size_t hit(math::Point2) const;
  void moveVisually(int direction, bool extend);

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void arrangeChildren(ArrangeContext &, math::Rect) override;
  void prepareContent(PrepareContext &) override;
  void paint(PaintContext &) const override;
  void onDefaultEvent(UIEvent &) override;
  void onDetach() noexcept override;
  void onThemeChanged() noexcept override;
  virtual bool commit();

public:
  TextField(TextFieldProps props, std::string value = {},
            layout::BoxProps box = {});
  ~TextField() override;

  const TextFieldProps &props() const noexcept { return _props; }

  const TextEditModel &model() const noexcept { return _model; }

  void setProps(TextFieldProps);
  void applyPatch(const TextFieldPatch &);
  void setValue(std::string);

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;
  TextInputState textInputState() const override;

  Connection onValueChanged(std::move_only_function<void(std::string)> f) {
    return _changed.connect(std::move(f));
  }

  Connection onCommit(std::move_only_function<void(std::string)> f) {
    return _committed.connect(std::move(f));
  }
};

class TextArea final : public TextField {
public:
  TextArea(TextFieldProps props, std::string value = {},
           layout::BoxProps box = {})
      : TextField{[&] {
                    props.editing.multiline = true;
                    return props;
                  }(),
                  std::move(value), box} {}
};

struct NumberFieldProps {
  RangeValue range;
  bool integer{};
};

class NumberField final : public TextField {
  NumberFieldProps _number;
  Signal<double> _changed;

protected:
  bool commit() override;

public:
  NumberField(TextFieldProps, NumberFieldProps number = {},
              layout::BoxProps box = {});

  const NumberFieldProps &numberProps() const noexcept { return _number; }

  void setNumberProps(NumberFieldProps);
  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection onNumberChanged(std::move_only_function<void(double)> f) {
    return _changed.connect(std::move(f));
  }
};
} // namespace playground::ui
