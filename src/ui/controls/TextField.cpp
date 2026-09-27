#include <algorithm>
#include <array>
#include <charconv>
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
  float width{}, height{}, lineHeight{};
  float scale{};
  rendering::ResourceDomainId domain{};
};

TextField::TextField(TextFieldProps props, std::string value,
                     layout::BoxProps box)
    : Node{box}, _props{std::move(props)},
      _model{_props.editing, std::move(value)} {
  if (!_props.font)
    throw std::invalid_argument("Text field requires a font");
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
  if (!props.font)
    throw std::invalid_argument("Text field requires a font");
  textBoundaries(props.placeholder, TextBoundary::Grapheme);
  _model.setProps(props.editing);
  _props = std::move(props);
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
            p.useTheme.appliedTo(_props.useTheme, d.useTheme)});
}

void TextField::onThemeChanged() noexcept {
  if (_layout)
    _layout->scale = 0;
}

void TextField::setValue(std::string value) {
  _model.setValue(std::move(value));
  _visualCaret.reset();
  changed(false);
}

void TextField::changed(bool edit) {
  if (edit)
    _visualCaret.reset();
  _layout.reset();
  invalidate(DirtyFlags::Measure | DirtyFlags::Paint | DirtyFlags::Semantics);
  if (edit)
    _changed.emit(_model.value());
}

void TextField::rebuild(float width) {
  width = std::max(1.f, width - 8);
  std::string display = _model.value();
  std::vector<std::size_t> source(display.size() + 1);
  for (std::size_t i = 0; i < source.size(); ++i)
    source[i] = i;
  if (!_model.composition().empty()) {
    const auto [a, b] =
        std::minmax(_model.selection().anchor, _model.selection().caret);
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
  if (_layout && _layout->text == display && _layout->width == width)
    return;
  auto next = std::make_unique<Layout>();
  next->text = display;
  next->width = width;
  next->lineHeight = float(std::max(1, _props.font->getLineSkip()));
  using TextResource = SDLResource<TTF_Text, TTF_DestroyText>;
  auto ltr = std::make_shared<const Font>(
      _props.font->cloneWith({.direction = TTF_DIRECTION_LTR}));
  auto rtl = std::make_shared<const Font>(
      _props.font->cloneWith({.direction = TTF_DIRECTION_RTL}));
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
  float y = 4;
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
    float x = 4;
    if (line.empty())
      next->carets.push_back({source[start], {x, y}});
    for (const auto run : visualTextRuns(line)) {
      const auto font = run.rtl ? rtl : ltr;
      const auto value = line.substr(run.begin, run.end - run.begin);
      TextResource text{
          TTF_CreateText(nullptr, font->get(), value.data(), value.size())};
      if (!text)
        throwSDLError("Create editor layout");
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
  next->height = y + 4;
  _layout = std::move(next);
  revealCaret();
}

layout::MeasureResult
TextField::measureContent(MeasureContext &,
                          const layout::SizeConstraints &offer) {
  const float width = offer.width.maximum.value_or(240);
  rebuild(width);
  return {{width, _props.editing.multiline
                      ? std::min(_layout->height, _layout->lineHeight * 5 + 8)
                      : _layout->lineHeight + 8}};
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
  for (auto &run : _layout->runs) {
    if (run.bounds.w() <= 0 || run.bounds.h() <= 0)
      continue;
    auto font = std::make_shared<const Font>(
        run.font->cloneWith({.size = run.font->getSize() * scale}));
    if (context.text && context.text->isEnabled()) {
      sdl::FontTextSource source{font, run.text, 0,
                                 _props.useTheme ? theme().text
                                                 : _props.foreground};
      run.image = context.text->prepareText(source);
    } else {
      SurfaceHandle surface{
          TTF_RenderText_Blended(
              font->get(), run.text.data(), run.text.size(),
              sdl::toSDL(_props.useTheme ? theme().text : _props.foreground)),
          SurfaceHandleDeleter{}};
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
  math::Point2 position{4, 4};
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
          math::rect(position.x - _scroll.x, position.y - _scroll.y, 1,
                     _layout ? _layout->lineHeight
                             : float(_props.font->getLineSkip())),
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
  if (caret.left() < 4)
    _scroll.x = std::max(0.f, _scroll.x + caret.left() - 4);
  else if (bounds().w() > 8 && caret.right() > bounds().w() - 4)
    _scroll.x += caret.right() - (bounds().w() - 4);
  if (caret.top() < 4)
    _scroll.y = std::max(0.f, _scroll.y + caret.top() - 4);
  else if (bounds().h() > 8 && caret.bottom() > bounds().h() - 4)
    _scroll.y += caret.bottom() - (bounds().h() - 4);
}

std::size_t TextField::hit(math::Point2 p) const {
  if (!_layout)
    return 0;
  std::size_t offset{};
  float distance = std::numeric_limits<float>::infinity(),
        lineDistance = distance;
  for (const auto &caret : _layout->carets) {
    const float line =
        std::abs(std::floor((p.y + _scroll.y - 4) / _layout->lineHeight) -
                 std::floor((caret.point.y - 4) / _layout->lineHeight));
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
               _props.useTheme ? theme().elevated : _props.background);
  control_paint::outline(context, {{}, bounds().size},
                         hasFocus() ? theme().focus : theme().border,
                         hasFocus() ? 2.f : 1.f);
  if (!_layout)
    return;
  const auto [a, b] =
      std::minmax(_model.selection().anchor, _model.selection().caret);
  PaintScope scope{context};
  context.clip({{}, bounds().size});
  context.translate(-_scroll);
  for (const auto &cluster : _layout->clusters)
    if (cluster.begin < b && cluster.end > a) {
      if (_props.useTheme && theme().highContrast)
        control_paint::outline(context, cluster.bounds, theme().focus, 1);
      else
        context.fill(cluster.bounds,
                     _props.useTheme ? theme().selection : _props.selection);
    }
  for (const auto &run : _layout->runs)
    if (run.image)
      context.drawImage(run.image, {{}, run.image->pixelSize()}, run.bounds,
                        {});
  if (hasFocus()) {
    auto caret = textInputState().caret;
    caret.position = caret.position + _scroll;
    context.fill(caret, _props.useTheme ? theme().text : _props.foreground);
  }
  if (!_model.composition().empty())
    for (const auto &cluster : _layout->clusters)
      if (cluster.begin == _model.selection().caret &&
          cluster.end == cluster.begin)
        context.fill(math::rect(cluster.bounds.left(),
                                cluster.bounds.bottom() - 1, cluster.bounds.w(),
                                1),
                     _props.useTheme ? theme().text : _props.foreground);
}

SemanticState TextField::semanticState() const {
  auto s = Node::semanticState();
  s.description.role = _props.editing.password    ? SemanticRole::Password
                       : _props.editing.multiline ? SemanticRole::TextArea
                                                  : SemanticRole::TextField;
  s.description.name = _props.name;
  s.description.enabled = _props.enabled;
  s.description.description = _props.validationMessage;
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

ActionResult TextField::performAction(const UIAction &action, ActionSource) {
  if (!_props.enabled)
    return ActionResult::Unavailable;
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
  if (_props.required && _model.value().empty())
    return false;
  _committed.emit(_model.value());
  return true;
}

void TextField::onDefaultEvent(UIEvent &e) {
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
#if defined(__APPLE__)
    const bool shortcut = e.command;
#else
    const bool shortcut = e.control;
#endif
    bool edit{};
    bool handled = true;
    if (!_model.composition().empty() && e.logicalKey != Key::Escape) {
      e.handled = true;
      return;
    }
    switch (e.logicalKey) {
    case Key::Left:
      if (shortcut) {
        _model.move(-1, e.shift, true);
        _visualCaret.reset();
      } else
        moveVisually(-1, e.shift);
      break;
    case Key::Right:
      if (shortcut) {
        _model.move(1, e.shift, true);
        _visualCaret.reset();
      } else
        moveVisually(1, e.shift);
      break;
    case Key::Up:
    case Key::Down: {
      auto caret = textInputState().caret;
      auto p = hit(
          {caret.left(), caret.top() + (e.logicalKey == Key::Up ? -1 : 1) *
                                           float(_props.font->getLineSkip())});
      _model.setSelection({e.shift ? _model.selection().anchor : p, p});
      break;
    }
    case Key::Home:
    case Key::End: {
      auto p =
          e.logicalKey == Key::Home ? std::size_t{0} : _model.value().size();
      if (!shortcut && _layout) {
        const auto current = textInputState().caret.position + _scroll;
        math::Point2 selected = current;
        for (const auto &c : _layout->carets)
          if (c.point.y == current.y &&
              ((e.logicalKey == Key::Home && c.point.x <= selected.x) ||
               (e.logicalKey == Key::End && c.point.x >= selected.x))) {
            selected = c.point;
            p = c.offset;
          }
        _visualCaret = selected;
      } else
        _visualCaret.reset();
      _model.setSelection({e.shift ? _model.selection().anchor : p, p});
      break;
    }
    case Key::Backspace:
      edit = _model.erase(true, shortcut);
      break;
    case Key::Delete:
      edit = _model.erase(false, shortcut);
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
}

void NumberField::setNumberProps(NumberFieldProps props) {
  props.range.validate();
  if (props.integer)
    for (const auto v : {props.range.value, props.range.minimum,
                         props.range.maximum, props.range.step})
      if (std::trunc(v) != v)
        throw std::invalid_argument(
            "Integer field requires integral range values");
  std::array<char, 128> buffer;
  const auto result = std::to_chars(
      buffer.data(), buffer.data() + buffer.size(), props.range.value);
  if (result.ec != std::errc{})
    throw std::runtime_error("Could not format numeric value");
  setValue(std::string{buffer.data(), result.ptr});
  _number = props;
  if (!this->props().validationMessage.empty()) {
    auto p = this->props();
    p.validationMessage.clear();
    setProps(std::move(p));
  }
}

bool NumberField::commit() {
  double value{};
  const auto &text = model().value();
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
      !std::isfinite(value) || value < _number.range.minimum ||
      value > _number.range.maximum ||
      (_number.integer && std::trunc(value) != value)) {
    auto p = props();
    p.validationMessage = "Enter a value within the allowed range";
    setProps(std::move(p));
    return false;
  }
  auto p = props();
  p.validationMessage.clear();
  setProps(std::move(p));
  const bool changed = value != _number.range.value;
  _number.range.value = value;
  if (changed)
    _changed.emit(value);
  return TextField::commit();
}

SemanticState NumberField::semanticState() const {
  auto s = TextField::semanticState();
  s.description.role = SemanticRole::SpinButton;
  s.range = _number.range;
  if (!props().editing.readOnly)
    s.actions.insert(s.actions.end(),
                     {SemanticAction::SetValue, SemanticAction::Increment,
                      SemanticAction::Decrement});
  return s;
}

ActionResult NumberField::performAction(const UIAction &a,
                                        ActionSource source) {
  if (auto *set = std::get_if<SetValue>(&a)) {
    if (!isInteractionEnabled() || props().editing.readOnly ||
        !std::isfinite(set->value) || set->value < _number.range.minimum ||
        set->value > _number.range.maximum ||
        (_number.integer && std::trunc(set->value) != set->value))
      return ActionResult::Unavailable;
    auto p = _number;
    p.range.value = set->value;
    if (p.range.value == _number.range.value)
      return ActionResult::Unchanged;
    setNumberProps(p);
    _changed.emit(p.range.value);
    return ActionResult::Applied;
  }
  if (auto *step = std::get_if<Increment>(&a))
    return performAction(SetValue{_number.range.adjusted(step->direction)},
                         source);
  return TextField::performAction(a, source);
}
} // namespace playground::ui
