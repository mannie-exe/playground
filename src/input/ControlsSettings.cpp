#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <type_traits>

#include <input/ControlsSettings.hpp>

namespace playground::input {
namespace {
using runtime::SettingKind;
constexpr std::array<std::string_view, 2> captures{"toggle", "hold"},
    recipes{"out-of-nowhere", "responsive"};
constexpr std::array<std::string_view, 3> profiles{"steered", "strafe", "tank"},
    perspectives{"automatic", "first-person", "third-person"};

template <auto Member> double get(const ControlsSettings &s) {
  return double(s.*Member);
}

template <auto Member> void set(ControlsSettings &s, double value) {
  s.*Member = static_cast<std::remove_cvref_t<decltype(s.*Member)>>(value);
}

template <auto Member>
ControlsSetting field(std::string_view key, std::string_view label,
                      std::string_view unit, unsigned group, SettingKind kind,
                      double low, double high, double step,
                      std::span<const std::string_view> choices = {}) {
  return {key,  label, unit,  group,   kind,        low,
          high, step,  false, choices, get<Member>, set<Member>};
}
} // namespace

std::span<const ControlsSetting> controlsSettingsSchema() {
  static const std::array fields{
      field<&ControlsSettings::mouseLookSensitivity>(
          "mouse_look_sensitivity", "Look sensitivity", "rad/unit", 0,
          SettingKind::Number, .00001, .1, .0005),
      field<&ControlsSettings::mouseInvertY>("mouse_invert_y", "Invert mouse Y",
                                             "", 0, SettingKind::Boolean, 0, 1,
                                             1),
      field<&ControlsSettings::mouseCapture>("mouse_capture", "Mouse capture",
                                             "", 0, SettingKind::Integer, 0, 1,
                                             1, captures),
      field<&ControlsSettings::zoomSensitivity>(
          "zoom_sensitivity", "Zoom sensitivity", "", 0, SettingKind::Number,
          .01, 20, .1),
      field<&ControlsSettings::stickLookSpeed>(
          "stick_look_speed", "Stick look speed", "rad/s", 1,
          SettingKind::Number, .01, 20, .1),
      field<&ControlsSettings::stickInvertY>("stick_invert_y", "Invert stick Y",
                                             "", 1, SettingKind::Boolean, 0, 1,
                                             1),
      field<&ControlsSettings::stickInnerDeadZone>(
          "stick_inner_dead_zone", "Inner dead zone", "", 1,
          SettingKind::Number, 0, .95, .01),
      field<&ControlsSettings::stickSaturationThreshold>(
          "stick_saturation_threshold", "Saturation threshold", "", 1,
          SettingKind::Number, .01, 1, .01),
      field<&ControlsSettings::stickResponseExponent>(
          "stick_response_exponent", "Stick response exponent", "", 1,
          SettingKind::Number, .1, 8, .1),
      field<&ControlsSettings::locomotion>(
          "locomotion_profile", "Locomotion profile", "", 2,
          SettingKind::Integer, 0, 2, 1, profiles),
      field<&ControlsSettings::steering>("steering_recipe", "Steering recipe",
                                         "", 2, SettingKind::Integer, 0, 1, 1,
                                         recipes),
      field<&ControlsSettings::perspective>("perspective", "Camera perspective",
                                            "", 2, SettingKind::Integer, 0, 2,
                                            1, perspectives)};
  return fields;
}

void setControlsSetting(ControlsSettings &settings,
                        const ControlsSetting &field, double value) {
  if (!std::isfinite(value) || value < field.minimum || value > field.maximum ||
      (field.kind != SettingKind::Number && std::floor(value) != value))
    throw std::invalid_argument("Invalid control setting value");
  field.set(settings, value);
}

void ControlsSettings::validate() const {
  auto copy = *this;
  for (const auto &field : controlsSettingsSchema())
    setControlsSetting(copy, field, field.get(*this));
  if (stickSaturationThreshold <= stickInnerDeadZone)
    throw std::invalid_argument(
        "Stick saturation must exceed its inner dead zone");
}

math::Vec2f stickResponse(math::Vec2f value, const ControlsSettings &settings) {
  settings.validate();
  if (!math::isFinite(value) || std::abs(value.x) > 1 || std::abs(value.y) > 1)
    throw std::invalid_argument("Stick axes must be normalized");
  const double length = std::hypot(value.x, value.y);
  if (length <= settings.stickInnerDeadZone)
    return {};
  const double magnitude =
      std::pow(std::clamp((length - settings.stickInnerDeadZone) /
                              (settings.stickSaturationThreshold -
                               settings.stickInnerDeadZone),
                          0., 1.),
               settings.stickResponseExponent);
  return {float(value.x * magnitude / length),
          float(value.y * magnitude / length)};
}
} // namespace playground::input
