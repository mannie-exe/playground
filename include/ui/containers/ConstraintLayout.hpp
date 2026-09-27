#pragma once

#include <layout/Constraints.hpp>
#include <ui/containers/Container.hpp>

namespace playground::ui {

struct ConstraintLayoutProps {
  std::size_t maximumPasses{8};
  float tolerance{0.01f};
  bool operator==(const ConstraintLayoutProps &) const = default;
};

struct ConstraintLayoutPatch {
  Patch<std::size_t> maximumPasses;
  Patch<float> tolerance;
};

class ConstraintLayout : public Node {
  struct Entry {
    layout::ConstraintId id;
    layout::LayoutConstraint value;
  };

  struct Solution {
    math::Size2 size;
    std::vector<math::Rect> children;
  };

  ConstraintLayoutProps _props;
  std::vector<std::string> _keys;
  std::vector<Entry> _constraints;
  layout::ConstraintId _nextId{1};

  std::vector<layout::MeasureResult> _intrinsic;

  static void validate(const ConstraintLayoutProps &p);
  static double strength(layout::ConstraintStrength value);
  static float scalar(double value);
  Solution solve(const std::vector<Entry> &constraints,
                 const layout::SizeConstraints &offered,
                 layout::LayoutDirection direction,
                 bool validating = false) const;
  void validateConstraints(const std::vector<Entry> &candidate) const;
  Solution layout(MeasureContext &context,
                  const layout::SizeConstraints &offered);

protected:
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override {
    return {layout(context, offered).size};
  }

  void arrangeChildren(ArrangeContext &context, math::Rect content) override;

public:
  explicit ConstraintLayout(ConstraintLayoutProps props = {},
                            layout::BoxProps box = {})
      : Node{box}, _props{props} {
    validate(props);
  }

  const ConstraintLayoutProps &props() const noexcept { return _props; }

  void setProps(ConstraintLayoutProps props);
  void applyPatch(const ConstraintLayoutPatch &p);
  Node &append(std::string key, std::unique_ptr<Node> child);
  std::unique_ptr<Node> takeChild(const std::string &key);
  layout::ConstraintId addConstraint(layout::LayoutConstraint value);
  void removeConstraint(layout::ConstraintId id);
  std::vector<layout::ConstraintId>
  setConstraints(std::vector<layout::LayoutConstraint> values);
};

} // namespace playground::ui
