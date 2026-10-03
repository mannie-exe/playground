#include <algorithm>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <math/Geometry3D.hpp>

namespace playground::math {
namespace {
float checked(double value) {
  if (!std::isfinite(value) ||
      std::abs(value) > std::numeric_limits<float>::max())
    throw std::overflow_error("3D arithmetic exceeds finite float range");
  return static_cast<float>(value);
}

void planes(float nearPlane, float farPlane) {
  if (!std::isfinite(nearPlane) || !std::isfinite(farPlane) || nearPlane < 0 ||
      farPlane <= nearPlane)
    throw std::invalid_argument("Invalid camera depth interval");
}
} // namespace

Vec3f normalized(Vec3f value) {
  const double length =
      std::hypot(static_cast<double>(value.x), value.y, value.z);
  if (!isFinite(value) || length == 0)
    throw std::invalid_argument("Cannot normalize zero or nonfinite vector");
  return {checked(value.x / length), checked(value.y / length),
          checked(value.z / length)};
}

bool isFinite(const Matrix4 &m) {
  return std::all_of(m.elements.begin(), m.elements.end(),
                     [](float v) { return std::isfinite(v); });
}

Matrix4 operator*(const Matrix4 &a, const Matrix4 &b) {
  Matrix4 result{{}};
  for (std::size_t r = 0; r < 4; ++r)
    for (std::size_t c = 0; c < 4; ++c) {
      double value{};
      for (std::size_t k = 0; k < 4; ++k)
        value += static_cast<double>(a.at(r, k)) * b.at(k, c);
      result.at(r, c) = checked(value);
    }
  return result;
}

Vec4f operator*(const Matrix4 &m, Vec4f v) {
  const std::array<float, 4> input{v.x, v.y, v.z, v.w};
  std::array<float, 4> result{};
  for (std::size_t r = 0; r < 4; ++r) {
    double value{};
    for (std::size_t c = 0; c < 4; ++c)
      value += static_cast<double>(m.at(r, c)) * input[c];
    result[r] = checked(value);
  }
  return {result[0], result[1], result[2], result[3]};
}

Matrix4 translation(Vec3f offset) {
  if (!isFinite(offset))
    throw std::invalid_argument("Invalid 3D translation");
  Matrix4 m;
  m.at(0, 3) = offset.x;
  m.at(1, 3) = offset.y;
  m.at(2, 3) = offset.z;
  return m;
}

Matrix4 scaling(Vec3f scale) {
  if (!isFinite(scale))
    throw std::invalid_argument("Invalid 3D scale");
  Matrix4 m;
  m.at(0, 0) = scale.x;
  m.at(1, 1) = scale.y;
  m.at(2, 2) = scale.z;
  return m;
}

Matrix4 perspectiveLH(float fov, float aspect, float nearPlane,
                      float farPlane) {
  planes(nearPlane, farPlane);
  if (!std::isfinite(fov) || fov <= 0 || fov >= std::numbers::pi_v<float> ||
      !std::isfinite(aspect) || aspect <= 0 || nearPlane == 0)
    throw std::invalid_argument("Invalid perspective camera");
  const double y = 1.0 / std::tan(static_cast<double>(fov) / 2);
  const double depth = static_cast<double>(farPlane) - nearPlane;
  Matrix4 m{{}};
  m.at(0, 0) = checked(y / aspect);
  m.at(1, 1) = checked(y);
  m.at(2, 2) = checked(farPlane / depth);
  m.at(2, 3) = checked(-static_cast<double>(nearPlane) * farPlane / depth);
  m.at(3, 2) = 1;
  return m;
}

Matrix4 orthographicLH(float width, float height, float nearPlane,
                       float farPlane) {
  planes(nearPlane, farPlane);
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 ||
      height <= 0)
    throw std::invalid_argument("Invalid orthographic camera");
  const double depth = static_cast<double>(farPlane) - nearPlane;
  Matrix4 m;
  m.at(0, 0) = checked(2.0 / width);
  m.at(1, 1) = checked(2.0 / height);
  m.at(2, 2) = checked(1.0 / depth);
  m.at(2, 3) = checked(-nearPlane / depth);
  return m;
}

Matrix4 lookAtLH(Vec3f eye, Vec3f target, Vec3f up) {
  if (!isFinite(eye) || !isFinite(target) || !isFinite(up))
    throw std::invalid_argument("Invalid camera basis");
  const auto forward = normalized(target - eye);
  const auto right = normalized(cross(normalized(up), forward));
  const auto correctedUp = cross(forward, right);
  Matrix4 m;
  const std::array<Vec3f, 3> basis{right, correctedUp, forward};
  for (std::size_t r = 0; r < 3; ++r) {
    const auto v = basis[r];
    m.at(r, 0) = v.x;
    m.at(r, 1) = v.y;
    m.at(r, 2) = v.z;
    m.at(r, 3) = checked(-(static_cast<double>(v.x) * eye.x +
                           static_cast<double>(v.y) * eye.y +
                           static_cast<double>(v.z) * eye.z));
  }
  return m;
}

Quaternion lookRotation(Vec3f forward, Vec3f up) {
  const auto z = normalized(forward);
  const auto x = normalized(cross(normalized(up), z));
  const auto y = cross(z, x);
  // Basis columns form the camera's world rotation, inverse of its view basis.
  const float m[3][3]{{x.x, y.x, z.x}, {x.y, y.y, z.y}, {x.z, y.z, z.z}};
  const float trace = m[0][0] + m[1][1] + m[2][2];
  Quaternion q;
  if (trace > 0) {
    const float scale = 2 * std::sqrt(trace + 1);
    q = {(m[2][1] - m[1][2]) / scale, (m[0][2] - m[2][0]) / scale,
         (m[1][0] - m[0][1]) / scale, scale / 4};
  } else {
    int i = m[1][1] > m[0][0] ? 1 : 0;
    if (m[2][2] > m[i][i])
      i = 2;
    const int j = (i + 1) % 3, k = (j + 1) % 3;
    const float scale = 2 * std::sqrt(1 + m[i][i] - m[j][j] - m[k][k]);
    float xyz[3]{};
    xyz[i] = scale / 4;
    xyz[j] = (m[j][i] + m[i][j]) / scale;
    xyz[k] = (m[k][i] + m[i][k]) / scale;
    q = {xyz[0], xyz[1], xyz[2], (m[k][j] - m[j][k]) / scale};
  }
  return normalizedRotation(q);
}

Quaternion slerp(Quaternion a, Quaternion b, double t) {
  if (!std::isfinite(t) || t < 0 || t > 1)
    throw std::invalid_argument("Invalid rotation interpolation weight");
  a = normalizedRotation(a);
  b = normalizedRotation(b);
  double cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (cosine < 0) {
    b = {-b.x, -b.y, -b.z, -b.w};
    cosine = -cosine;
  }
  double left = 1 - t, right = t;
  if (cosine < 0.9995) {
    const double angle = std::acos(std::clamp(cosine, 0.0, 1.0));
    left = std::sin((1 - t) * angle) / std::sin(angle);
    right = std::sin(t * angle) / std::sin(angle);
  }
  return normalizedRotation({static_cast<float>(a.x * left + b.x * right),
                             static_cast<float>(a.y * left + b.y * right),
                             static_cast<float>(a.z * left + b.z * right),
                             static_cast<float>(a.w * left + b.w * right)});
}

} // namespace playground::math
