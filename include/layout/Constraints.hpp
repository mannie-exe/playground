#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace playground::layout {

enum class AnchorAttribute {
  Start,
  End,
  Top,
  Bottom,
  CenterX,
  CenterY,
  Width,
  Height,
  Baseline
};
enum class ConstraintRelation { Equal, LessEqual, GreaterEqual };
enum class ConstraintStrength { Required, Strong, Medium, Weak };
using ConstraintId = std::uint64_t;

struct LayoutAnchor {
  std::optional<std::string> child;
  AnchorAttribute attribute{AnchorAttribute::Start};

  static LayoutAnchor parent(AnchorAttribute attribute) {
    return {{}, attribute};
  }

  bool operator==(const LayoutAnchor &) const = default;
};

struct LinearTerm {
  LayoutAnchor anchor;
  double coefficient{1};
  bool operator==(const LinearTerm &) const = default;
};

struct LinearExpression {
  std::vector<LinearTerm> terms;
  double constant{};
  LinearExpression() = default;

  LinearExpression(double value) : constant{value} {}

  LinearExpression(LayoutAnchor anchor) : terms{{std::move(anchor), 1}} {}

  bool operator==(const LinearExpression &) const = default;
};

inline LinearExpression operator+(LinearExpression a,
                                  const LinearExpression &b) {
  a.terms.insert(a.terms.end(), b.terms.begin(), b.terms.end());
  a.constant += b.constant;
  return a;
}

inline LinearExpression operator*(LinearExpression a, double factor) {
  for (auto &term : a.terms)
    term.coefficient *= factor;
  a.constant *= factor;
  return a;
}

inline LinearExpression operator*(double factor, LinearExpression a) {
  return a * factor;
}

inline LinearExpression operator-(LinearExpression a,
                                  const LinearExpression &b) {
  return a + b * -1;
}

inline LinearExpression operator/(LinearExpression a, double divisor) {
  if (!std::isfinite(divisor) || divisor == 0)
    throw std::invalid_argument("Invalid expression divisor");
  return a * (1 / divisor);
}

struct LayoutConstraint {
  LinearExpression left;
  ConstraintRelation relation{ConstraintRelation::Equal};
  LinearExpression right;
  ConstraintStrength strength{ConstraintStrength::Required};
  bool operator==(const LayoutConstraint &) const = default;
};

} // namespace playground::layout
