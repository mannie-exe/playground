#pragma once
#include <span>
#include <string_view>

#include <math/Geometry2D.hpp>
#include <runtime/Setting.hpp>

namespace playground::input {
enum class MouseCapture { Toggle, Hold };
enum class LocomotionPreference { Steered, Strafe, Tank };
enum class SteeringRecipe { OutOfNowhere, Responsive };
enum class PerspectivePreference { Automatic, FirstPerson, ThirdPerson };

struct ControlsSettings {
  double mouseLookSensitivity{.003}, stickLookSpeed{2.5},
      stickInnerDeadZone{.15}, stickSaturationThreshold{1},
      stickResponseExponent{1}, zoomSensitivity{1};
  bool mouseInvertY{}, stickInvertY{};
  MouseCapture mouseCapture{MouseCapture::Toggle};
  LocomotionPreference locomotion{LocomotionPreference::Steered};
  SteeringRecipe steering{SteeringRecipe::OutOfNowhere};
  PerspectivePreference perspective{PerspectivePreference::Automatic};
  void validate() const;
  bool operator==(const ControlsSettings &) const = default;
};

struct ControlCapabilities {
  bool mouse{}, gamepad{}, locomotion{}, follow{};
};

struct ControlsState {
  ControlsSettings requested;
  ControlCapabilities capabilities;
  std::string_view restriction;
};

struct ControlsSetting {
  std::string_view key, label, unit;
  unsigned group; // Mouse, Gamepad, Locomotion/Camera.
  runtime::SettingKind kind;
  double minimum, maximum, step;
  bool inactive{};
  std::span<const std::string_view> choices;
  double (*get)(const ControlsSettings &);
  void (*set)(ControlsSettings &, double);
};

std::span<const ControlsSetting> controlsSettingsSchema();
void setControlsSetting(ControlsSettings &, const ControlsSetting &, double);
math::Vec2f stickResponse(math::Vec2f, const ControlsSettings &);
} // namespace playground::input
