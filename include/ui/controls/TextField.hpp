#pragma once

#include <array>
#include <memory>
#include <string>

#include <support/Font.hpp>
#include <ui/Node.hpp>
#include <ui/TextEdit.hpp>
#include <ui/controls/Editing.hpp>

namespace playground::ui {
struct TextFieldProps {
  FontHandle font;
  TextEditProps editing;
  bool enabled{true};
  bool required{};
  std::string name, placeholder, validationMessage;
  std::optional<math::ColorRGBA8> foreground, background, selection;
  std::optional<TextRole> textRole;
  std::optional<FontFamily> fontFamily;
  std::optional<FontSelection> fontSelection;
  bool operator==(const TextFieldProps &) const = default;
};

struct TextFieldPatch {
  Patch<FontHandle> font;
  Patch<TextEditProps> editing;
  Patch<bool> enabled, required;
  Patch<std::string> name, placeholder, validationMessage;
  Patch<std::optional<math::ColorRGBA8>> foreground, background, selection;
  Patch<std::optional<TextRole>> textRole;
  Patch<std::optional<FontFamily>> fontFamily;
  Patch<std::optional<FontSelection>> fontSelection;
};

class TextField : public Node,
                  public TextInputClient,
                  public virtual DraftEditor {
  struct Layout;
  TextFieldProps _props;
  TextEditModel _model;
  std::string _accepted;
  std::function<ValidationResult(std::string_view)> _textValidator;
  ValidationMode _textValidationMode{ValidationMode::OnCommit};
  Signal<std::string> _changed, _committed;

  std::unique_ptr<Layout> _layout;
  mutable FontHandle _themeFont;
  mutable std::array<FontHandle, 2> _directionFonts;
  mutable std::optional<ThemeTypography> _fontTypography;
  FontHandle resolvedFont() const;

  math::ColorRGBA8 foreground() const {
    return isEffectivelyEnabled()
               ? resolveColor(&ThemePalette::text, _props.foreground)
               : theme().mutedText;
  }

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
  void refreshValue(std::string);

  void showValidation(ValidationResult issue) override {
    auto p = props();
    p.validationMessage = issue ? issue->message : "";
    setProps(std::move(p));
  }

  ValidationResult validateDraft() const override;
  bool commitDraft(ChangeContext = {}) override;
  void revertDraft(ChangeContext = {ActionSource::Program,
                                    ChangeReason::Cancel}) override;

  bool draftDirty() const override { return _model.value() != _accepted; }

  void setTextValidator(std::function<ValidationResult(std::string_view)> v,
                        ValidationMode mode = ValidationMode::OnCommit) {
    _textValidator = std::move(v);
    _textValidationMode = mode;
  }

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;
  TextInputState textInputState() const override;

  Connection onValueChanged(support::MoveOnlyFunction<void(std::string)> f) {
    return _changed.connect(std::move(f));
  }

  Connection onCommit(support::MoveOnlyFunction<void(std::string)> f) {
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
  std::optional<double> multiple;
};

class NumberField final : public TextField, public NumericEditor {
  NumberFieldProps _number;
  Signal<double> _changed;
  Signal<double, ChangeContext> _edited;
  Signal<ValidationResult> _validationChanged;
  Signal<ChangeContext> _finished;
  NumberCodec _codec;
  std::function<ValidationResult(double)> _validator;
  ValidationMode _validationMode{ValidationMode::OnCommit};
  std::string _acceptedText;
  bool _conflict{};
  Connection _textChanges;
  Signal<> _draftChanged;
  NumberParse parsed() const;
  std::string formatted(double, const NumberCodec &) const;
  void report(ValidationResult);

protected:
  bool commit() override;
  void onDefaultEvent(UIEvent &) override;

public:
  NumberField(TextFieldProps, NumberFieldProps number = {},
              layout::BoxProps box = {});

  const NumberFieldProps &numberProps() const noexcept { return _number; }

  void setNumberProps(NumberFieldProps);

  double acceptedNumber() const override { return _number.range.value; }

  void setNumericInteraction(bool enabled, bool readOnly) override {
    auto p = props();
    p.enabled = enabled;
    p.editing.readOnly = readOnly;
    setProps(std::move(p));
  }

  void setNumericRange(RangeValue range) override {
    auto p = _number;
    p.range = range;
    setNumberProps(p);
  }

  void showValidation(ValidationResult issue) override {
    report(std::move(issue));
  }

  ValidationResult validateDraft() const override;
  bool commitDraft(ChangeContext = {}) override;
  void revertDraft(ChangeContext = {ActionSource::Program,
                                    ChangeReason::Cancel}) override;

  bool draftDirty() const override { return model().value() != _acceptedText; }

  bool hasConflict() const noexcept { return _conflict; }

  void setCodec(NumberCodec);
  void setValidator(std::function<ValidationResult(double)>,
                    ValidationMode = ValidationMode::OnCommit);
  ActionResult adjustNumber(int, ChangeContext) override;

  Connection onNumberEdited(
      support::MoveOnlyFunction<void(double, ChangeContext)> f) override {
    return _edited.connect(std::move(f));
  }

  Connection onDraftChanged(support::MoveOnlyFunction<void()> f) override {
    return _draftChanged.connect(std::move(f));
  }

  Connection
  onValidationChanged(support::MoveOnlyFunction<void(ValidationResult)> f) {
    return _validationChanged.connect(std::move(f));
  }

  Connection
  onInteractionFinished(support::MoveOnlyFunction<void(ChangeContext)> f) {
    return _finished.connect(std::move(f));
  }

  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection onNumberChanged(support::MoveOnlyFunction<void(double)> f) {
    return _changed.connect(std::move(f));
  }
};
} // namespace playground::ui
