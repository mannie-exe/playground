#include <memory>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/AdaptiveStack.hpp>
#include <ui/containers/AnchorLayout.hpp>
#include <ui/containers/Flow.hpp>
#include <ui/containers/Grid.hpp>
#include <ui/containers/ZStack.hpp>

using namespace playground;

template <class Container, class Patch> void checkPlacementPatch() {
  std::unique_ptr<Container> parent = std::make_unique<Container>();
  Container *parentPtr = parent.get();
  ui::Node &child = parent->append(std::make_unique<ui::Box>());
  ui::UIRoot root;
  root.setContent(std::move(parent));
  const ui::NodeId id = child.id();

  Patch patch;
  patch.margin = ui::Patch<math::Insets>::set(math::Insets::all(7));
  parentPtr->applyPlacementPatch(id, patch);
  test::require(parentPtr->placementOf(id).margin == math::Insets::all(7),
                "NodeId placement patch targets attached child");

  patch.margin = ui::Patch<math::Insets>::reset();
  parentPtr->applyPlacementPatch(id, patch);
  test::require(parentPtr->placementOf(id).margin == math::Insets{},
                "placement Reset uses baseline");
}

int main() {
  return test::run([] {
    checkPlacementPatch<ui::HStack, ui::StackPlacementPatch>();
    checkPlacementPatch<ui::Flow, ui::StackPlacementPatch>();
    checkPlacementPatch<ui::AdaptiveStack, ui::StackPlacementPatch>();
    checkPlacementPatch<ui::Grid, ui::GridPlacementPatch>();
    checkPlacementPatch<ui::ZStack, ui::BoxPlacementPatch>();
    checkPlacementPatch<ui::AnchorLayout, ui::AnchorPlacementPatch>();
  });
}
