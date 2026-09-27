#include <kiwi/kiwi.h>

#include <ui/containers/ConstraintLayout.hpp>

namespace playground::ui {

void ConstraintLayout::validate(const ConstraintLayoutProps &p) {
  if (!p.maximumPasses || p.maximumPasses > 64 || !std::isfinite(p.tolerance) ||
      p.tolerance <= 0)
    throw std::invalid_argument("Invalid constraint convergence budget");
}

double ConstraintLayout::strength(layout::ConstraintStrength value) {
  switch (value) {
  case layout::ConstraintStrength::Required:
    return kiwi::strength::required;
  case layout::ConstraintStrength::Strong:
    return kiwi::strength::strong;
  case layout::ConstraintStrength::Medium:
    return kiwi::strength::medium;
  case layout::ConstraintStrength::Weak:
    return kiwi::strength::weak;
  }
  throw std::invalid_argument("Invalid constraint strength");
}

float ConstraintLayout::scalar(double value) {
  if (!std::isfinite(value) ||
      std::abs(value) > std::numeric_limits<float>::max())
    throw std::overflow_error("Constraint result exceeds geometry range");
  return static_cast<float>(value);
}

ConstraintLayout::Solution
ConstraintLayout::solve(const std::vector<Entry> &constraints,
                        const layout::SizeConstraints &offered,
                        layout::LayoutDirection direction,
                        bool validating) const {
  struct Variables {
    kiwi::Variable x, y, width, height, baseline;
  };

  kiwi::Solver solver;
  Variables parent;
  std::vector<Variables> vars(_keys.size());
  auto add = [&](kiwi::Constraint c) { solver.addConstraint(c); };
  add(parent.x == 0);
  add(parent.y == 0);
  auto parentAxis = [&](const kiwi::Variable &variable,
                        layout::AxisConstraints axis) {
    add(variable >= axis.minimum);
    if (axis.maximum)
      add(variable <= *axis.maximum);
    add((variable == axis.maximum.value_or(axis.minimum)) |
        (axis.maximum ? kiwi::strength::strong : 0.0001));
  };
  parentAxis(parent.width, offered.width);
  parentAxis(parent.height, offered.height);
  auto expression = [&](const layout::LinearExpression &value) {
    if (!std::isfinite(value.constant))
      throw std::invalid_argument("Nonfinite constraint constant");
    kiwi::Expression result{value.constant};
    for (const auto &term : value.terms) {
      if (!std::isfinite(term.coefficient))
        throw std::invalid_argument("Nonfinite constraint coefficient");
      const Variables *v = &parent;
      std::optional<std::size_t> child;
      if (term.anchor.child) {
        auto it = std::find(_keys.begin(), _keys.end(), *term.anchor.child);
        if (it == _keys.end())
          throw std::invalid_argument(
              "Constraint names a missing direct child");
        child = it - _keys.begin();
        v = &vars[*child];
      }
      const bool rtl = direction == layout::LayoutDirection::RightToLeft;
      kiwi::Expression anchor;
      switch (term.anchor.attribute) {
      case layout::AnchorAttribute::Start:
        anchor = rtl ? v->x + v->width : kiwi::Expression{kiwi::Term{v->x}};
        break;
      case layout::AnchorAttribute::End:
        anchor = rtl ? kiwi::Expression{kiwi::Term{v->x}} : v->x + v->width;
        break;
      case layout::AnchorAttribute::Top:
        anchor = kiwi::Expression{kiwi::Term{v->y}};
        break;
      case layout::AnchorAttribute::Bottom:
        anchor = v->y + v->height;
        break;
      case layout::AnchorAttribute::CenterX:
        anchor = v->x + v->width * 0.5;
        break;
      case layout::AnchorAttribute::CenterY:
        anchor = v->y + v->height * 0.5;
        break;
      case layout::AnchorAttribute::Width:
        anchor = kiwi::Expression{kiwi::Term{v->width}};
        break;
      case layout::AnchorAttribute::Height:
        anchor = kiwi::Expression{kiwi::Term{v->height}};
        break;
      case layout::AnchorAttribute::Baseline:
        anchor = child && validating ? v->y + v->baseline
                 : child && *child < _intrinsic.size() &&
                         _intrinsic[*child].firstBaseline
                     ? v->y + *_intrinsic[*child].firstBaseline
                     : v->y + v->height;
        break;
      default:
        throw std::invalid_argument("Invalid layout anchor");
      }
      result = result + anchor * term.coefficient;
    }
    return result;
  };
  for (std::size_t i = 0; i < vars.size(); ++i) {
    auto &v = vars[i];
    const auto &node = *children()[i];
    const auto intrinsic =
        i < _intrinsic.size() ? _intrinsic[i].size : math::Size2{};
    auto dimension = [&](const kiwi::Variable &variable,
                         const kiwi::Variable &parentVariable,
                         layout::Axis axis, float measured) {
      if (node.isPortal() || node.visibility() == Visibility::Collapsed) {
        add(variable == 0);
        return;
      }
      const auto limits = container_detail::limits(node, axis);
      add(variable >= limits.minimum);
      if (limits.maximum)
        add(variable <= *limits.maximum);
      const auto rule = container_detail::rule(node, axis);
      switch (rule.kind()) {
      case layout::SizeKind::Fixed:
        add(variable == limits.clamp(rule.value()));
        break;
      case layout::SizeKind::Percent:
        add((variable == parentVariable * rule.value()) |
            kiwi::strength::strong);
        break;
      case layout::SizeKind::Fill:
        add((variable == parentVariable) | kiwi::strength::strong);
        break;
      case layout::SizeKind::Content:
        add((variable == measured) | 0.1);
        break;
      }
    };
    dimension(v.width, parent.width, layout::Axis::Horizontal, intrinsic.width);
    dimension(v.height, parent.height, layout::Axis::Vertical,
              intrinsic.height);
    if (validating) {
      add(v.baseline >= 0);
      add(v.baseline <= v.height);
    }
    add((v.x == 0) | 0.001);
    add((v.y == 0) | 0.001);
    add((v.x >= 0) | 0.01);
    add((v.y >= 0) | 0.01);
    add((parent.width >= v.x + v.width) | 0.01);
    add((parent.height >= v.y + v.height) | 0.01);
  }
  for (const auto &entry : constraints) {
    auto relation = kiwi::OP_EQ;
    switch (entry.value.relation) {
    case layout::ConstraintRelation::Equal:
      break;
    case layout::ConstraintRelation::LessEqual:
      relation = kiwi::OP_LE;
      break;
    case layout::ConstraintRelation::GreaterEqual:
      relation = kiwi::OP_GE;
      break;
    default:
      throw std::invalid_argument("Invalid constraint relation");
    }
    try {
      add(kiwi::Constraint{expression(entry.value.left) -
                               expression(entry.value.right),
                           relation, strength(entry.value.strength)});
    } catch (const kiwi::UnsatisfiableConstraint &) {
      throw std::invalid_argument("Unsatisfiable required layout constraint " +
                                  std::to_string(entry.id));
    }
  }
  solver.updateVariables();
  Solution result{{scalar(parent.width.value()), scalar(parent.height.value())},
                  {}};
  for (const auto &v : vars)
    result.children.push_back(
        math::rect(scalar(v.x.value()), scalar(v.y.value()),
                   std::max(0.0f, scalar(v.width.value())),
                   std::max(0.0f, scalar(v.height.value()))));
  return result;
}

void ConstraintLayout::validateConstraints(
    const std::vector<Entry> &candidate) const {
  try {
    (void)solve(candidate, {}, layout::LayoutDirection::LeftToRight, true);
  } catch (const std::invalid_argument &) {
    // Signed physical offsets can intentionally describe an RTL-only set.
    (void)solve(candidate, {}, layout::LayoutDirection::RightToLeft, true);
  }
}

ConstraintLayout::Solution
ConstraintLayout::layout(MeasureContext &context,
                         const layout::SizeConstraints &offered) {
  _intrinsic.clear();
  for (const auto &child : children())
    _intrinsic.push_back(child->isPortal() ? layout::MeasureResult{}
                                           : child->measure(context, {}));
  for (std::size_t pass = 0; pass < _props.maximumPasses; ++pass) {
    Solution result;
    bool provisional{};
    try {
      result = solve(_constraints, offered, context.direction);
    } catch (const std::invalid_argument &) {
      // Width-dependent content may have a different baseline after reflow.
      // Only use the free-baseline solution to obtain remeasurement widths.
      result = solve(_constraints, offered, context.direction, true);
      provisional = true;
    }
    bool stable = true;
    for (std::size_t i = 0; i < children().size(); ++i) {
      if (children()[i]->isPortal())
        continue;
      const auto next =
          children()[i]->measure(context, {{0, result.children[i].w()}, {}});
      stable &= std::abs(next.size.width - _intrinsic[i].size.width) <=
                    _props.tolerance &&
                std::abs(next.size.height - _intrinsic[i].size.height) <=
                    _props.tolerance &&
                next.firstBaseline == _intrinsic[i].firstBaseline;
      _intrinsic[i] = next;
    }
    if (stable)
      return provisional ? solve(_constraints, offered, context.direction)
                         : result;
  }
  throw std::runtime_error(
      "ConstraintLayout intrinsic remeasurement did not converge");
}

void ConstraintLayout::arrangeChildren(ArrangeContext &context,
                                       math::Rect content) {
  const auto solution =
      layout(context, layout::SizeConstraints::tight(content.size));
  // Solve and remeasure completely before committing any child placement.
  for (std::size_t i = 0; i < children().size(); ++i) {
    if (children()[i]->isPortal())
      continue;
    auto bounds = solution.children[i];
    bounds.position.x += content.x();
    bounds.position.y += content.y();
    children()[i]->arrange(context, bounds);
  }
}

void ConstraintLayout::setProps(ConstraintLayoutProps props) {
  validate(props);
  if (_props == props)
    return;
  _props = props;
  invalidateLayout();
}

void ConstraintLayout::applyPatch(const ConstraintLayoutPatch &p) {
  const ConstraintLayoutProps d;
  setProps({p.maximumPasses.appliedTo(_props.maximumPasses, d.maximumPasses),
            p.tolerance.appliedTo(_props.tolerance, d.tolerance)});
}

Node &ConstraintLayout::append(std::string key, std::unique_ptr<Node> child) {
  checkStructuralMutation();
  if (key.empty() || std::find(_keys.begin(), _keys.end(), key) != _keys.end())
    throw std::invalid_argument(
        "Constraint child keys must be nonempty and unique");
  _keys.reserve(_keys.size() + 1);
  auto &result = appendChild(std::move(child));
  _keys.push_back(std::move(key));
  return result;
}

std::unique_ptr<Node> ConstraintLayout::takeChild(const std::string &key) {
  const auto it = std::find(_keys.begin(), _keys.end(), key);
  if (it == _keys.end())
    throw std::out_of_range("Constraint child key");
  for (const auto &entry : _constraints)
    for (const auto *expr : {&entry.value.left, &entry.value.right})
      for (const auto &term : expr->terms)
        if (term.anchor.child == key)
          throw std::logic_error("Remove referencing constraints before child");
  auto result = takeChildAt(it - _keys.begin());
  _keys.erase(it);
  _intrinsic.clear();
  return result;
}

layout::ConstraintId
ConstraintLayout::addConstraint(layout::LayoutConstraint value) {
  auto candidate = _constraints;
  candidate.push_back({_nextId, std::move(value)});
  validateConstraints(candidate);
  _constraints = std::move(candidate);
  invalidateLayout();
  return _nextId++;
}

void ConstraintLayout::removeConstraint(layout::ConstraintId id) {
  auto it = std::find_if(_constraints.begin(), _constraints.end(),
                         [&](const auto &c) { return c.id == id; });
  if (it == _constraints.end())
    throw std::out_of_range("Constraint id");
  _constraints.erase(it);
  invalidateLayout();
}

std::vector<layout::ConstraintId>
ConstraintLayout::setConstraints(std::vector<layout::LayoutConstraint> values) {
  std::vector<Entry> candidate;
  std::vector<layout::ConstraintId> ids;
  auto next = _nextId;
  for (auto &value : values) {
    ids.push_back(next);
    candidate.push_back({next++, std::move(value)});
  }
  validateConstraints(candidate);
  _constraints = std::move(candidate);
  _nextId = next;
  invalidateLayout();
  return ids;
}

} // namespace playground::ui
