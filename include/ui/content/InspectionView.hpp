#pragma once

#include <scene/Controllers.hpp>
#include <ui/content/SceneView.hpp>

namespace playground::ui {
struct InspectionProps {
  double orbitSensitivity{.003}, zoomSensitivity{1};
  bool invertY{}, altPrimary{true};
  void validate() const;
  bool operator==(const InspectionProps &) const = default;
};

// App owns scene publication. Changes emit a resolved camera; no subject or
// native relative-mouse lease is required by this view.
class InspectionView final : public SceneView {
  scene::OrbitController _orbit;
  world::WorldPosition _initialPivot;
  InspectionProps _navigation;
  enum class Gesture { Orbit, Pan, Zoom };
  Gesture _gesture{};
  std::optional<std::uint64_t> _pointer;
  int _button{};
  math::Point2 _previous;
  Signal<scene::WorldCamera> _changed;
  void publish();

protected:
  void onDefaultEvent(UIEvent &) override;

  void onDetach() noexcept override { cancelNavigation(); }

public:
  InspectionView(SceneViewProps, scene::OrbitProps, InspectionProps = {},
                 layout::BoxProps = {});

  scene::WorldCamera camera() const { return _orbit.camera(); }

  const scene::OrbitProps &orbitProps() const noexcept {
    return _orbit.props();
  }

  bool isNavigating() const noexcept { return _pointer.has_value(); }

  void setNavigationProps(InspectionProps);
  void cancelNavigation() noexcept;
  void resetPan();

  Connection onCameraChanged(
      support::MoveOnlyFunction<void(scene::WorldCamera)> callback) {
    return _changed.connect(std::move(callback));
  }
};
} // namespace playground::ui
