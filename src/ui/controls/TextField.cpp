#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <SDL3_ttf/SDL_ttf.h>

#include <platform/sdl/FontTextSource.hpp>
#include <platform/sdl/SDLGeometry.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <support/SDLError.hpp>
#include <ui/controls/ControlPaint.hpp>
#include <ui/controls/TextField.hpp>

namespace playground::ui {
struct TextField::Layout {
  struct Run {
    FontHandle font;
    std::string text;
    math::Rect bounds;
    rendering::PaintImageHandle image;
    SemanticTextRun semantics;
  };

  struct Caret {
    std::size_t offset;
    math::Point2 point;
  };

  struct Cluster {
    std::size_t begin, end;
    math::Rect bounds;
  };

  std::vector<Run> runs;
  std::vector<Caret> carets;
  std::vector<Cluster> clusters;
  std::optional<math::Point2> compositionCaret;
  std::string text;
  float width{}, height{}, lineHeight{}, padding{};
  float scale{};
  rendering::ResourceDomainId domain{};
};

TextField::TextField(TextFieldProps props, std::string value,
                     layout::BoxProps box)
    : Node{box}, _props{std::move(props)},
      _model{_props.editing, std::move(value)} {
  if (_props.fontSelection)
    _props.fontSelection->validate();
  if (_props.fontFamily && (*_props.fontFamily < FontFamily::Interface ||
                            *_props.fontFamily >= FontFamily::Count))
    throw std::invalid_argument("Invalid font family");
  if (_props.textRole && (*_props.textRole < TextRole::Display ||
                          *_props.textRole >= TextRole::Count))
    throw std::invalid_argument("Invalid text role");
  if (!_props.font && !_props.textRole && !_props.fontFamily)
    throw std::invalid_argument("Text field requires a font");
  _accepted = _model.value();
  setInputProps({HitTestPolicy::Self, true});
  setClip(true);
}

TextField::~TextField() = default;

void TextField::onDetach() noexcept {
  _drag.reset();
  _model.cancelComposition();
  releaseAllPointers();
}

void TextField::setProps(TextFieldProps props) {
  if (props.fontSelection)
    props.fontSelection->validate();
  if (props.fontFamily && (*props.fontFamily < FontFamily::Interface ||
                           *props.fontFamily >= FontFamily::Count))
    throw std::invalid_argument("Invalid font family");
  if (props.textRole && (*props.textRole < TextRole::Display ||
                         *props.textRole >= TextRole::Count))
    throw std::invalid_argument("Invalid text role");
  if (!props.font && !props.textRole && !props.fontFamily)
    throw std::invalid_argument("Text field requires a font");
  textBoundaries(props.placeholder, TextBoundary::Grapheme);
  _model.setProps(props.editing);
  _props = std::move(props);
  _themeFont.reset();
  if (!_props.enabled)
    onDetach();
  _layout.reset();
  invalidate(DirtyFlags::Measure | DirtyFlags::Paint | DirtyFlags::Semantics);
}

void TextField::applyPatch(const TextFieldPatch &p) {
  TextFieldProps d;
  setProps({p.font.appliedTo(_props.font, d.font),
            p.editing.appliedTo(_props.editing, d.editing),
            p.enabled.appliedTo(_props.enabled, true),
            p.required.appliedTo(_props.required, false),
            p.name.appliedTo(_props.name, {}),
            p.placeholder.appliedTo(_props.placeholder, {}),
            p.validationMessage.appliedTo(_props.validationMessage, {}),
            p.foreground.appliedTo(_props.foreground, d.foreground),
            p.background.appliedTo(_props.background, d.background),
            p.selection.appliedTo(_props.selection, d.selection),
            p.textRole.appliedTo(_props.textRole, d.textRole),
            p.fontFamily.appliedTo(_props.fontFamily, d.fontFamily),
            p.fontSelection.appliedTo(_props.fontSelection, d.fontSelection)});
}

FontHandle TextField::resolvedFont() const {
  const auto &typography = resolvedTheme().typography;
  if (!_themeFont || !_fontTypography || *_fontTypography != typography) {
    _themeFont =
        resolveThemeFont(typography, _props.textRole, _props.font, nullptr,
                         _props.fontFamily, _props.fontSelection);
    _fontTypography = typography;
    _directionFonts = {};
  }
  return _themeFont;
}

void TextField::onThemeChanged() noexcept {
  if (!_fontTypography || *_fontTypography != resolvedTheme().typography) {
    _themeFont.reset();
    _layout.reset();
  } else if (_layout)
    _layout->scale = 0;
}

void TextField::setValue(std::string value) {
  _model.setValue(std::move(value));
  _accepted = _model.value();
  _visualCaret.reset();
  changed(false);
}

void TextField::refreshValue(std::string value) {
  _model.validateValue(value);
  if (!draftDirty()) {
    setValue(std::move(value));
    return;
  }
  if (value != _accepted) {
    _accepted = std::move(value);
    auto p = props();
    p.validationMessage =
        "Value changed externally; commit your edit or revert";
    setProps(p);
  }
}

ValidationResult TextField::validateDraft() const {
  if (textInputState().composing)
    return ValidationIssue{"composition",
                           "Finish text composition before applying"};
  if (_props.required && _model.value().empty())
    return ValidationIssue{"required", "A value is required"};
  return _textValidator ? _textValidator(_model.value()) : ValidationResult{};
}

bool TextField::commitDraft(ChangeContext context) {
  if (!isInteractionEnabled() || props().editing.readOnly)
    return !draftDirty();
  if (auto issue = TextField::validateDraft()) {
    if (_textValidationMode != ValidationMode::OnSubmit ||
        context.reason == ChangeReason::Submit ||
        !props().validationMessage.empty())
      showValidation(issue);
    return false;
  }
  auto p = props();
  p.validationMessage.clear();
  setProps(p);
  _accepted = _model.value();
  _committed.emit(_model.value());
  return true;
}

void TextField::revertDraft(ChangeContext) {
  setValue(_accepted);
  auto p = props();
  p.validationMessage.clear();
  setProps(p);
}

void TextField::changed(bool edit) {
  if (edit)
    _visualCaret.reset();
  _layout.reset();
  invalidate(DirtyFlags::Measure | DirtyFlags::Paint | DirtyFlags::Semantics);
  if (edit) {
    if (_textValidationMode == ValidationMode::OnEdit) {
      auto issue = TextField::validateDraft();
      _props.validationMessage = issue ? issue->message : "";
    }
    _changed.emit(_model.value());
  }
}

void TextField::rebuild(float width) {
  width = std::max(1.f, width - 2 * themeMetrics().inputPadding);
  std::string display = _model.value();
  std::vector<std::size_t> source(display.size() + 1);
  for (std::size_t i = 0; i < source.size(); ++i)
    source[i] = i;
  if (!_model.composition().empty()) {
    const auto selection = _model.selection();
    const auto [a, b] = std::minmax(selection.anchor, selection.caret);
    display.replace(a, b - a, _model.composition());
    source.clear();
    for (std::size_t i = 0; i <= display.size(); ++i)
      source.push_back(i <= a ? i
                       : i <= a + _model.composition().size()
                           ? _model.selection().caret
                           : i - _model.composition().size() + (b - a));
  }
  if (_props.editing.password) {
    const auto boundaries = textBoundaries(display, TextBoundary::Grapheme);
    std::string masked;
    std::vector<std::size_t> mapping;
    for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
      masked += '*';
      mapping.push_back(source[boundaries[i]]);
    }
    mapping.push_back(_model.value().size());
    display = std::move(masked);
    source = std::move(mapping);
  }
  const bool placeholder = display.empty() && !_props.placeholder.empty();
  if (placeholder) {
    display = _props.placeholder;
    source.assign(display.size() + 1, 0);
  }
  if (_layout && _layout->text == display && _layout->width == width &&
      _layout->padding == themeMetrics().inputPadding)
    return;
  auto next = std::make_unique<Layout>();
  next->text = display;
  next->width = width;
  next->padding = themeMetrics().inputPadding;
  next->lineHeight = float(std::max(1, resolvedFont()->getLineSkip()));
  using TextResource = SDLResource<TTF_Text, TTF_DestroyText>;
  // Retain directional faces across width-dependent layout passes. Reopening
  // them also discards FreeType/HarfBuzz glyph and script caches.
  const auto font = resolvedFont();
  if (!_directionFonts[0])
    _directionFonts[0] = font->getDirection() == TTF_DIRECTION_LTR
                             ? font
                             : std::make_shared<const Font>(font->cloneWith(
                                   {.direction = TTF_DIRECTION_LTR}));
  if (!_directionFonts[1])
    _directionFonts[1] = font->getDirection() == TTF_DIRECTION_RTL
                             ? font
                             : std::make_shared<const Font>(font->cloneWith(
                                   {.direction = TTF_DIRECTION_RTL}));
  const auto &ltr = _directionFonts[0];
  const auto &rtl = _directionFonts[1];
  const auto measure = [&](std::string_view line) {
    float width{};
    for (auto run : visualTextRuns(line)) {
      const auto font = run.rtl ? rtl : ltr;
      int w{}, h{};
      if (!TTF_GetStringSize(font->get(), line.data() + run.begin,
                             run.end - run.begin, &w, &h))
        throwSDLError("Measure editor run");
      width += w;
    }
    return width;
  };
  std::size_t start{};
  float y = themeMetrics().inputPadding;
  const auto preeditOffset =
      std::min(_model.selection().anchor, _model.selection().caret) +
      _model.compositionSelection().caret;
  while (start <= display.size()) {
    const auto newline = display.find('\n', start);
    const auto paragraphEnd =
        newline == std::string::npos ? display.size() : newline;
    std::size_t end = paragraphEnd;
    if (_props.editing.multiline && end > start) {
      const auto part = std::string_view{display}.substr(start, end - start);
      const auto boundaries = textBoundaries(part, TextBoundary::Grapheme);
      if (measure(part) > width) {
        std::size_t low = 1, high = boundaries.size() - 1;
        while (low < high) {
          const auto mid = (low + high + 1) / 2;
          if (measure(part.substr(0, boundaries[mid])) <= width)
            low = mid;
          else
            high = mid - 1;
        }
        end = start + boundaries[low];
        const auto breaks = textBoundaries(part, TextBoundary::Line);
        auto at = std::upper_bound(breaks.begin(), breaks.end(), end - start);
        if (at != breaks.begin() && *--at > 0)
          end = start + *at;
      }
    }
    const auto line = std::string_view{display}.substr(start, end - start);
    float x = themeMetrics().inputPadding;
    if (line.empty())
      next->carets.push_back({source[start], {x, y}});
    for (const auto run : visualTextRuns(line)) {
      const auto font = run.rtl ? rtl : ltr;
      const auto value = line.substr(run.begin, run.end - run.begin);
      TextResource text{
          TTF_CreateText(nullptr, font->get(), value.data(), value.size())};
      if (!text)
        throwSDLError("Create editor layout");
      // Editable whitespace needs advances for caret, selection and hit
      // testing. SDL_ttf otherwise trims line-ending spaces from substring
      // geometry.
      if (!TTF_SetTextWrapWhitespaceVisible(text.get(), true))
        throwSDLError("Preserve editor whitespace");
      int w{}, h{};
      if (!TTF_GetTextSize(text.get(), &w, &h))
        throwSDLError("Measure editor layout");
      next->runs.push_back({font,
                            std::string{value},
                            math::rect(x, y, float(w), float(h)),
                            {},
                            {}});
      auto &semantic = next->runs.back().semantics;
      semantic.text = value;
      semantic.bounds = next->runs.back().bounds;
      semantic.rtl = run.rtl;
      const auto boundaries = textBoundaries(value, TextBoundary::Grapheme);
      for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
        TTF_SubString sub{};
        if (!TTF_GetTextSubString(text.get(), int(boundaries[i]), &sub))
          throwSDLError("Locate editor cluster");
        // A ligature can cover several graphemes. Divide its advance among
        // legal caret stops rather than shaping isolated prefixes (which
        // changes joins).
        const auto first =
            std::lower_bound(boundaries.begin(), boundaries.end(),
                             std::size_t(std::max(0, sub.offset)));
        const auto last =
            std::lower_bound(boundaries.begin(), boundaries.end(),
                             std::size_t(std::max(0, sub.offset + sub.length)));
        const auto count = std::max<std::ptrdiff_t>(1, last - first);
        const float advance = float(sub.rect.w) / float(count);
        const auto ordinal = std::ptrdiff_t(i) - (first - boundaries.begin());
        const float left =
            x + sub.rect.x +
            (run.rtl ? float(count - ordinal - 1) : float(ordinal)) * advance;
        const auto begin = source[start + run.begin + boundaries[i]],
                   finish = source[start + run.begin + boundaries[i + 1]];
        next->clusters.push_back(
            {begin, finish, math::rect(left, y, advance, next->lineHeight)});
        semantic.byteOffsets.push_back(begin);
        semantic.positions.push_back(left - x);
        semantic.widths.push_back(advance);
        next->carets.push_back({begin, {run.rtl ? left + advance : left, y}});
        next->carets.push_back({finish, {run.rtl ? left : left + advance, y}});
        if (!_model.composition().empty() && !_props.editing.password) {
          const auto displayBegin = start + run.begin + boundaries[i],
                     displayEnd = start + run.begin + boundaries[i + 1];
          if (preeditOffset >= displayBegin && preeditOffset <= displayEnd)
            next->compositionCaret =
                math::Point2{preeditOffset == displayBegin
                                 ? (run.rtl ? left + advance : left)
                                 : (run.rtl ? left : left + advance),
                             y};
        }
      }
      semantic.byteOffsets.push_back(source[start + run.end]);
      x += w;
    }
    y += next->lineHeight;
    if (end == display.size())
      break;
    start = end == paragraphEnd ? end + 1 : end;
  }
  next->height = y + themeMetrics().inputPadding;
  _layout = std::move(next);
  revealCaret();
}

layout::MeasureResult
TextField::measureContent(MeasureContext &,
                          const layout::SizeConstraints &offer) {
  const float width = offer.width.maximum.value_or(themeMetrics().inputWidth);
  rebuild(width);
  return {{width,
           _props.editing.multiline
               ? std::min(_layout->height,
                          _layout->lineHeight * themeMetrics().textAreaLines +
                              2 * themeMetrics().inputPadding)
               : _layout->lineHeight + 2 * themeMetrics().inputPadding}};
}

void TextField::arrangeChildren(ArrangeContext &, math::Rect area) {
  rebuild(area.w());
  revealCaret();
}

void TextField::prepareContent(PrepareContext &context) {
  if (!_layout)
    rebuild(bounds().w());
  const float scale = std::max(context.pixelScale.x, context.pixelScale.y);
  const auto domain = context.text     ? context.text->resourceDomain()
                      : context.images ? context.images->resourceDomain()
                                       : rendering::ResourceDomainId::cpu();
  if (_layout->scale == scale && _layout->domain == domain)
    return;
  if (_layout->domain != domain)
    for (auto &run : _layout->runs)
      run.image.reset();
  for (auto &run : _layout->runs) {
    if (run.bounds.w() <= 0 || run.bounds.h() <= 0)
      continue;
    auto font = std::make_shared<const Font>(
        run.font->cloneWith({.size = run.font->getSize() * scale}));
    if (context.text && context.text->isEnabled()) {
      sdl::FontTextSource source{font, run.text, 0, foreground()};
      run.image = context.text->prepareText(source);
    } else {
      auto surface = adoptManagedSurface(
          TTF_RenderText_Blended(font->get(), run.text.data(), run.text.size(),
                                 sdl::toSDL(foreground())),
          font->resources());
      if (!surface)
        throwSDLError("Render editor run");
      run.image = sdl::makeSurfaceImage(std::move(surface));
      if (context.images)
        run.image = context.images->prepare(run.image);
    }
  }
  _layout->scale = scale;
  _layout->domain = domain;
}

TextInputState TextField::textInputState() const {
  math::Point2 position{themeMetrics().inputPadding,
                        themeMetrics().inputPadding};
  float best = std::numeric_limits<float>::infinity();
  if (_layout)
    for (const auto &caret : _layout->carets)
      if (caret.offset == _model.selection().caret) {
        const float distance =
            _visualCaret ? std::abs(caret.point.x - _visualCaret->x) +
                               std::abs(caret.point.y - _visualCaret->y) * 1000
                         : 0;
        if (distance < best) {
          best = distance;
          position = caret.point;
        }
      }
  if (_layout && _layout->compositionCaret)
    position = *_layout->compositionCaret;
  return {_props.editing.multiline, _props.editing.readOnly,
          _props.editing.password,
          math::rect(position.x - _scroll.x, position.y - _scroll.y,
                     themeMetrics().caretWidth,
                     _layout ? _layout->lineHeight
                             : float(resolvedFont()->getLineSkip())),
          !_model.composition().empty()};
}

void TextField::moveVisually(int direction, bool extend) {
  if (!_layout) {
    _model.move(direction, extend);
    return;
  }
  auto carets = _layout->carets;
  std::sort(carets.begin(), carets.end(), [](const auto &a, const auto &b) {
    return a.point.y == b.point.y ? a.point.x < b.point.x
                                  : a.point.y < b.point.y;
  });
  carets.erase(std::unique(carets.begin(), carets.end(),
                           [](const auto &a, const auto &b) {
                             return a.offset == b.offset && a.point == b.point;
                           }),
               carets.end());
  const auto current = textInputState().caret.position + _scroll;
  auto found = std::find_if(carets.begin(), carets.end(), [&](const auto &c) {
    return c.offset == _model.selection().caret && c.point == current;
  });
  if (found == carets.end())
    return;
  if (direction < 0 && found != carets.begin())
    --found;
  else if (direction > 0 && found + 1 != carets.end())
    ++found;
  _model.setSelection(
      {extend ? _model.selection().anchor : found->offset, found->offset});
  _visualCaret = found->point;
}

void TextField::revealCaret() {
  const auto caret = textInputState().caret;
  if (caret.left() < themeMetrics().inputPadding)
    _scroll.x =
        std::max(0.f, _scroll.x + caret.left() - themeMetrics().inputPadding);
  else if (bounds().w() > 2 * themeMetrics().inputPadding &&
           caret.right() > bounds().w() - themeMetrics().inputPadding)
    _scroll.x += caret.right() - (bounds().w() - themeMetrics().inputPadding);
  if (caret.top() < themeMetrics().inputPadding)
    _scroll.y =
        std::max(0.f, _scroll.y + caret.top() - themeMetrics().inputPadding);
  else if (bounds().h() > 2 * themeMetrics().inputPadding &&
           caret.bottom() > bounds().h() - themeMetrics().inputPadding)
    _scroll.y += caret.bottom() - (bounds().h() - themeMetrics().inputPadding);
}

std::size_t TextField::hit(math::Point2 p) const {
  if (!_layout)
    return 0;
  std::size_t offset{};
  float distance = std::numeric_limits<float>::infinity(),
        lineDistance = distance;
  for (const auto &caret : _layout->carets) {
    const float line =
        std::abs(std::floor((p.y + _scroll.y - themeMetrics().inputPadding) /
                            _layout->lineHeight) -
                 std::floor((caret.point.y - themeMetrics().inputPadding) /
                            _layout->lineHeight));
    const float d = std::abs(p.x + _scroll.x - caret.point.x);
    if (line < lineDistance || (line == lineDistance && d < distance)) {
      lineDistance = line;
      distance = d;
      offset = caret.offset;
    }
  }
  return offset;
}

void TextField::paint(PaintContext &context) const {
  context.fill({{}, bounds().size},
               resolveColor(&ThemePalette::elevated, _props.background));
  if (!isEffectivelyEnabled() && theme().highContrast)
    control_paint::disabledOutline(context, {{}, bounds().size}, theme(),
                                   themeMetrics());
  else
    control_paint::outline(context, {{}, bounds().size},
                           hasFocus() ? theme().focus : theme().border,
                           hasFocus() ? resolvedFocusWidth()
                                      : themeMetrics().borderWidth);
  if (!_layout)
    return;
  const auto selection = _model.selection();
  const auto [a, b] = std::minmax(selection.anchor, selection.caret);
  PaintScope scope{context};
  context.clip({{}, bounds().size});
  context.translate(-_scroll);
  for (const auto &cluster : _layout->clusters)
    if (cluster.begin < b && cluster.end > a) {
      if (theme().highContrast)
        control_paint::outline(context, cluster.bounds, theme().focus,
                               themeMetrics().borderWidth);
      else
        context.fill(cluster.bounds,
                     resolveColor(&ThemePalette::selection, _props.selection));
    }
  for (const auto &run : _layout->runs)
    if (run.image)
      context.drawImage(run.image, {{}, run.image->pixelSize()}, run.bounds,
                        {});
  if (hasFocus()) {
    auto caret = textInputState().caret;
    caret.position = caret.position + _scroll;
    context.fill(caret, foreground());
  }
  if (!_model.composition().empty())
    for (const auto &cluster : _layout->clusters)
      if (cluster.begin == _model.selection().caret &&
          cluster.end == cluster.begin)
        context.fill(
            math::rect(cluster.bounds.left(),
                       cluster.bounds.bottom() - themeMetrics().caretWidth,
                       cluster.bounds.w(), themeMetrics().caretWidth),
            foreground());
}

SemanticState TextField::semanticState() const {
  auto s = Node::semanticState();
  s.description.role = _props.editing.password    ? SemanticRole::Password
                       : _props.editing.multiline ? SemanticRole::TextArea
                                                  : SemanticRole::TextField;
  s.description.name = _props.name;
  s.description.enabled = _props.enabled;
  if (!_props.validationMessage.empty()) {
    if (!s.description.description.empty())
      s.description.description += "; ";
    s.description.description += _props.validationMessage;
  }
  s.description.value =
      _props.editing.password ? std::string{} : _model.value();
  s.readOnly = _props.editing.readOnly;
  s.required = _props.required;
  s.invalid = !_props.validationMessage.empty();
  s.protectedText = _props.editing.password;
  if (!_props.editing.password && _model.composition().empty() && _layout) {
    s.selection = _model.selection();
    s.actions.push_back(SemanticAction::SetSelection);
    if (_layout && !_model.value().empty()) {
      for (const auto &run : _layout->runs) {
        auto semantic = run.semantics;
        semantic.bounds.position = semantic.bounds.position - _scroll;
        s.textRuns.push_back(std::move(semantic));
      }
      std::sort(s.textRuns.begin(), s.textRuns.end(),
                [](const auto &a, const auto &b) {
                  return a.byteOffsets.front() < b.byteOffsets.front();
                });
    }
  }
  if (!_props.editing.readOnly)
    s.actions.push_back(SemanticAction::ReplaceText);
  return s;
}

ActionResult TextField::performAction(const UIAction &action,
                                      ActionSource source) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
  if (std::holds_alternative<CommitEdit>(action))
    return commitDraft({source, ChangeReason::Submit})
               ? ActionResult::Applied
               : ActionResult::Unavailable;
  if (std::holds_alternative<CancelEdit>(action)) {
    revertDraft({source, ChangeReason::Cancel});
    return ActionResult::Applied;
  }
  if (const auto *selection = std::get_if<TextSelection>(&action)) {
    try {
      _model.setSelection(*selection);
    } catch (const std::invalid_argument &) {
      return ActionResult::Unavailable;
    }
    _visualCaret.reset();
    _layout.reset();
    invalidate(DirtyFlags::Measure | DirtyFlags::Paint | DirtyFlags::Semantics);
    return ActionResult::Applied;
  }
  if (const auto *replace = std::get_if<ReplaceSelectedText>(&action)) {
    if (_props.editing.readOnly)
      return ActionResult::Unavailable;
    bool applied{};
    try {
      applied = _model.replace(replace->value);
    } catch (const std::invalid_argument &) {
      return ActionResult::Unavailable;
    } catch (const std::length_error &) {
      return ActionResult::Unavailable;
    }
    changed(applied);
    return applied ? ActionResult::Applied : ActionResult::Unchanged;
  }
  return ActionResult::Unsupported;
}

bool TextField::commit() {
  return TextField::commitDraft({ActionSource::Keyboard, ChangeReason::Enter});
}

void TextField::onDefaultEvent(UIEvent &e) {
  if (e.type == EventType::FocusLost && !_props.editing.multiline &&
      draftDirty() && !textInputState().composing)
    commitDraft({e.source, ChangeReason::Blur});
  if (e.type == EventType::FocusLost || e.type == EventType::PointerCancel ||
      e.type == EventType::InputCancel) {
    onDetach();
    changed(false);
    return;
  }
  if (e.handled || !_props.enabled)
    return;
  if (e.type == EventType::TextInput) {
    performAction(ReplaceSelectedText{e.text}, e.source);
    e.handled = true;
    return;
  }
  if (e.type == EventType::TextEditing) {
    try {
      _model.setComposition(e.text, e.compositionStart, e.compositionLength);
      changed(false);
    } catch (const std::invalid_argument &) {
    } catch (const std::length_error &) {
    }
    e.handled = true;
    return;
  }
  if (e.type == EventType::PointerDown && e.button == 1) {
    requestFocus();
    _drag = e.pointer;
    capturePointer(e.pointer);
    auto p = hit(e.localPosition);
    _model.setSelection({e.shift ? _model.selection().anchor : p, p});
    _visualCaret = e.localPosition + _scroll;
    e.handled = true;
  }
  if (_drag == e.pointer &&
      (e.type == EventType::PointerMove ||
       (e.type == EventType::PointerUp && e.button == 1))) {
    _model.setSelection({_model.selection().anchor, hit(e.localPosition)});
    _visualCaret = e.localPosition + _scroll;
    e.handled = true;
    if (e.type == EventType::PointerUp) {
      _drag.reset();
      releaseAllPointers();
    }
  }
  if (e.type == EventType::KeyDown) {
    auto key = e.logicalKey;
#if defined(__APPLE__)
    const bool shortcut = e.command;
    const bool word = e.alt;
    const bool document = e.command && key != Key::Left && key != Key::Right;
    if (e.command) {
      if (key == Key::Left || key == Key::Up)
        key = Key::Home;
      else if (key == Key::Right || key == Key::Down)
        key = Key::End;
    }
#else
    const bool shortcut = e.control;
    const bool word = e.control;
    const bool document = e.control;
#endif
    bool edit{};
    bool handled = true;
    if (!_model.composition().empty() && e.logicalKey != Key::Escape) {
      e.handled = true;
      return;
    }
    const auto edge = [&](bool start, bool wholeDocument) {
      auto p = start ? std::size_t{0} : _model.value().size();
      if (!wholeDocument && _layout) {
        const auto current = textInputState().caret.position + _scroll;
        math::Point2 selected = current;
        for (const auto &c : _layout->carets)
          if (c.point.y == current.y && ((start && c.point.x <= selected.x) ||
                                         (!start && c.point.x >= selected.x))) {
            selected = c.point;
            p = c.offset;
          }
        _visualCaret = selected;
      } else
        _visualCaret.reset();
      return p;
    };
    switch (key) {
    case Key::Left:
      if (word) {
        _model.move(-1, e.shift, true);
        _visualCaret.reset();
      } else
        moveVisually(-1, e.shift);
      break;
    case Key::Right:
      if (word) {
        _model.move(1, e.shift, true);
        _visualCaret.reset();
      } else
        moveVisually(1, e.shift);
      break;
    case Key::Up:
    case Key::Down: {
      auto caret = textInputState().caret;
      auto p = hit({caret.left(),
                    caret.top() + (e.logicalKey == Key::Up ? -1 : 1) *
                                      float(resolvedFont()->getLineSkip())});
      _model.setSelection({e.shift ? _model.selection().anchor : p, p});
      break;
    }
    case Key::Home:
    case Key::End: {
      const auto p = edge(key == Key::Home, document);
      _model.setSelection({e.shift ? _model.selection().anchor : p, p});
      break;
    }
    case Key::Backspace:
#if defined(__APPLE__)
      if (e.command && !_props.editing.readOnly &&
          _model.selection().anchor == _model.selection().caret) {
        edit = _model.eraseTo(edge(true, false));
        break;
      }
#endif
      edit = _model.erase(true, word);
      break;
    case Key::Delete:
      edit = _model.erase(false, word);
      break;
    case Key::Enter:
      if (_props.editing.multiline && !shortcut) {
        performAction(ReplaceSelectedText{"\n"}, e.source);
      } else
        commit();
      break;
    case Key::Escape:
      if (!_model.composition().empty())
        _model.cancelComposition();
      else if (draftDirty())
        revertDraft({e.source, ChangeReason::Cancel});
      else
        handled = false;
      break;
    case Key::A:
      if (shortcut)
        _model.selectAll();
      else
        handled = false;
      break;
    case Key::C:
    case Key::X:
      if (shortcut) {
        if (auto *s = services();
            s && s->writeClipboard && !_props.editing.password) {
          s->writeClipboard(_model.selectedText());
          if (e.logicalKey == Key::X)
            edit = _model.replace({});
        }
      } else
        handled = false;
      break;
    case Key::V:
      if (shortcut) {
        if (auto *s = services(); s && s->readClipboard)
          performAction(ReplaceSelectedText{s->readClipboard()}, e.source);
      } else
        handled = false;
      break;
    case Key::Z:
      if (shortcut)
        edit = e.shift ? _model.redo() : _model.undo();
      else
        handled = false;
      break;
    case Key::Y:
      if (shortcut)
        edit = _model.redo();
      else
        handled = false;
      break;
    default:
      handled = false;
      break;
    }
    if (handled) {
      e.handled = true;
      if (edit || e.logicalKey == Key::Escape)
        changed(edit);
    }
  }
  if (e.handled) {
    revealCaret();
    invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
  }
}

NumberField::NumberField(TextFieldProps props, NumberFieldProps number,
                         layout::BoxProps box)
    : TextField{[&] {
                  props.editing.multiline = false;
                  return props;
                }(),
                {},
                box} {
  setNumberProps(number);
  _textChanges = onValueChanged([this](const std::string &) {
    _draftChanged.emit();
    if (_validationMode == ValidationMode::OnEdit ||
        !this->props().validationMessage.empty())
      report(validateDraft());
  });
}

NumberParse NumberField::parsed() const {
  return _codec.parse ? _codec.parse(model().value())
                      : parseNumber(model().value());
}

std::string NumberField::formatted(double value,
                                   const NumberCodec &codec) const {
  auto text = codec.format ? codec.format(value) : formatNumber(value);
  model().validateValue(text);
  const auto parsed = codec.parse ? codec.parse(text) : parseNumber(text);
  if (parsed.state != ParseState::Valid || parsed.value != value)
    throw std::invalid_argument(
        "Number codec must round-trip the accepted value");
  return text;
}

void NumberField::report(ValidationResult issue) {
  auto p = props();
  auto message = issue ? issue->message : std::string{};
  if (p.validationMessage == message)
    return;
  p.validationMessage = std::move(message);
  setProps(std::move(p));
  _validationChanged.emit(std::move(issue));
}

void NumberField::setNumberProps(NumberFieldProps props) {
  props.range.validate();
  if (props.integer)
    for (auto v : {props.range.value, props.range.minimum, props.range.maximum,
                   props.range.step})
      if (std::trunc(v) != v)
        throw std::invalid_argument(
            "Integer field requires integral range values");
  if (auto issue = validateNumber(props.range.value, props.range, props.integer,
                                  props.multiple))
    throw std::invalid_argument(issue->message);
  const bool preserve = draftDirty();
  const bool changed = _number.range.value != props.range.value;
  auto text = formatted(props.range.value, _codec);
  if (!preserve)
    setValue(text);
  _number = props;
  _acceptedText = std::move(text);
  if (preserve) {
    _conflict |= changed;
    if (_conflict)
      report(ValidationIssue{
          "conflict", "Value changed externally; commit your edit or revert"});
  } else {
    report({});
  }
  invalidate(DirtyFlags::Semantics);
}

void NumberField::setCodec(NumberCodec codec) {
  if (bool(codec.parse) != bool(codec.format))
    throw std::invalid_argument("Number codec requires parse and format");
  auto text = formatted(acceptedNumber(), codec);
  const bool preserve = draftDirty();
  if (!preserve)
    setValue(text);
  _codec = std::move(codec);
  _acceptedText = std::move(text);
  if (!preserve)
    report({});
}

void NumberField::setValidator(
    std::function<ValidationResult(double)> validator, ValidationMode mode) {
  _validator = std::move(validator);
  _validationMode = mode;
  setTextValidator({}, mode);
}

ValidationResult NumberField::validateDraft() const {
  if (auto issue = TextField::validateDraft())
    return issue;
  if (textInputState().composing)
    return ValidationIssue{"composition",
                           "Finish text composition before applying"};
  const auto p = parsed();
  if (p.state != ParseState::Valid)
    return ValidationIssue{p.state == ParseState::Empty ? "required" : "number",
                           "Enter a complete number"};
  if (auto issue = validateNumber(p.value, _number.range, _number.integer,
                                  _number.multiple))
    return issue;
  return _validator ? _validator(p.value) : ValidationResult{};
}

bool NumberField::commitDraft(ChangeContext context) {
  if (!isInteractionEnabled() || props().editing.readOnly)
    return !draftDirty();
  if (auto issue = validateDraft()) {
    if (_validationMode != ValidationMode::OnSubmit ||
        context.reason == ChangeReason::Submit ||
        !props().validationMessage.empty())
      report(issue);
    return false;
  }
  const double value = parsed().value;
  const bool changed = value != _number.range.value;
  auto text = formatted(value, _codec);
  setValue(text);
  _number.range.value = value;
  _acceptedText = std::move(text);
  _conflict = false;
  report({});
  _draftChanged.emit();
  if (changed) {
    _edited.emit(value, context);
    _changed.emit(value);
  }
  TextField::commitDraft(context);
  _finished.emit(context);
  return true;
}

bool NumberField::commit() {
  return commitDraft({ActionSource::Keyboard, ChangeReason::Enter});
}

void NumberField::revertDraft(ChangeContext context) {
  setValue(_acceptedText);
  _conflict = false;
  report({});
  _draftChanged.emit();
  _finished.emit({context.source, ChangeReason::Cancel});
}

ActionResult NumberField::adjustNumber(int direction, ChangeContext context) {
  if (!isInteractionEnabled() || props().editing.readOnly)
    return ActionResult::Unavailable;
  if (draftDirty() && !commitDraft(context))
    return ActionResult::Unavailable;
  const auto next = _number.range.adjusted(direction);
  return performAction(SetValue{next}, context.source);
}

void NumberField::onDefaultEvent(UIEvent &e) {
  if (!e.handled && e.type == EventType::KeyDown &&
      !textInputState().composing) {
    if (e.logicalKey == Key::Escape && draftDirty()) {
      revertDraft({e.source, ChangeReason::Cancel});
      e.handled = true;
      return;
    }
    if (e.logicalKey == Key::Enter) {
      commitDraft({e.source, ChangeReason::Enter});
      e.handled = true;
      return;
    }
    if (e.logicalKey == Key::Up || e.logicalKey == Key::Down ||
        e.logicalKey == Key::PageUp || e.logicalKey == Key::PageDown) {
      const bool large =
          e.logicalKey == Key::PageUp || e.logicalKey == Key::PageDown;
      const int direction =
          (e.logicalKey == Key::Up || e.logicalKey == Key::PageUp) ? 1 : -1;
      if (!large)
        adjustNumber(direction, {e.source, ChangeReason::Step});
      else if ((!draftDirty() || commitDraft({e.source, ChangeReason::Step})) &&
               isInteractionEnabled() && !props().editing.readOnly) {
        auto value = std::clamp(_number.range.value +
                                    direction * _number.range.step * 10,
                                _number.range.minimum, _number.range.maximum);
        performAction(SetValue{value}, e.source);
      }
      e.handled = true;
      return;
    }
  }
  TextField::onDefaultEvent(e);
}

SemanticState NumberField::semanticState() const {
  auto s = TextField::semanticState();
  s.description.role = SemanticRole::SpinButton;
  s.range = _number.range;
  if (isInteractionEnabled() && !props().editing.readOnly)
    s.actions.insert(s.actions.end(),
                     {SemanticAction::SetValue, SemanticAction::Increment,
                      SemanticAction::Decrement});
  return s;
}

ActionResult NumberField::performAction(const UIAction &a,
                                        ActionSource source) {
  if (!isInteractionEnabled())
    return ActionResult::Unavailable;
  if (std::holds_alternative<CommitEdit>(a))
    return commitDraft({source, ChangeReason::Submit})
               ? ActionResult::Applied
               : ActionResult::Unavailable;
  if (std::holds_alternative<CancelEdit>(a)) {
    revertDraft({source, ChangeReason::Cancel});
    return ActionResult::Applied;
  }
  if (auto *step = std::get_if<Increment>(&a))
    return adjustNumber(step->direction, {source, ChangeReason::Step});
  if (auto *set = std::get_if<SetValue>(&a)) {
    if (!isInteractionEnabled() || props().editing.readOnly ||
        validateNumber(set->value, _number.range, _number.integer,
                       _number.multiple))
      return ActionResult::Unavailable;
    if (_validator)
      if (auto issue = _validator(set->value)) {
        report(issue);
        return ActionResult::Unavailable;
      }
    const auto old = _number.range.value;
    auto text = formatted(set->value, _codec);
    setValue(text);
    _number.range.value = set->value;
    _acceptedText = std::move(text);
    _conflict = false;
    report({});
    _draftChanged.emit();
    if (old == set->value)
      return ActionResult::Unchanged;
    ChangeContext context{source, ChangeReason::Step};
    _edited.emit(set->value, context);
    _changed.emit(set->value);
    _finished.emit(context);
    return ActionResult::Applied;
  }
  return TextField::performAction(a, source);
}
} // namespace playground::ui
