#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <scene/Scene3D.hpp>

namespace playground::scene {

enum class PlaybackMode { Loop, Clamp };

struct PlaybackProps {
  PlaybackMode mode{PlaybackMode::Loop};
  double rate{1};
};

class Playback {
  PlaybackProps _props;
  double _time{};
  bool _paused{};

public:
  explicit Playback(PlaybackProps props = {});

  const PlaybackProps &props() const noexcept { return _props; }

  void setProps(PlaybackProps props);
  void seek(double seconds);
  void advance(double seconds);

  void setPaused(bool paused) noexcept { _paused = paused; }

  bool isPaused() const noexcept { return _paused; }

  double time() const noexcept { return _time; }

  double sampleTime(double duration) const;
};

enum class TrackPath { Translation, Rotation, Scale };
enum class TrackInterpolation { Step, Linear, CubicSpline };

struct TransformTrack {
  std::size_t node{};
  TrackPath path{};
  TrackInterpolation interpolation{TrackInterpolation::Linear};
  std::vector<float> times;
  // Cubic keys contain incoming tangent, value, outgoing tangent, in that
  // order.
  std::vector<math::Vec4f> values;
  void validate(std::size_t nodeCount) const;
  math::Vec4f sample(double seconds) const;
};

struct AnimationClip {
  std::string name;
  double duration{};
  std::vector<TransformTrack> tracks;
  void validate(std::size_t nodeCount) const;
};

struct FlipbookProps {
  math::Vec2i grid{1, 1};
  unsigned frames{1};
  double framesPerSecond{30};
  // Inset in normalized atlas UV units (e.g. half a source texel).
  math::Vec2f inset{};

  struct PixelGrid {
    math::Vec2i atlas, frame;
  };

  // Optional pixel cells; permits a cropped trailing row/column in source art.
  std::optional<PixelGrid> pixels;
  void validate() const;
};

rendering::UVTransform flipbookFrame(const FlipbookProps &, const Playback &);
// Spherical camera-facing quad. Local +Y remains the camera's up direction.
math::Transform3D billboard(math::Vec3f position, const CameraProps &camera,
                            math::Vec3f scale = {1, 1, 1});

} // namespace playground::scene
