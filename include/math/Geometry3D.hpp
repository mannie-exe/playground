#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace playground::math {

struct Vec3f {
  float x{}, y{}, z{};
  bool operator==(const Vec3f &) const = default;
};
struct Vec4f {
  float x{}, y{}, z{}, w{};
  bool operator==(const Vec4f &) const = default;
};
constexpr Vec3f operator+(Vec3f a, Vec3f b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
constexpr Vec3f operator-(Vec3f a, Vec3f b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
constexpr Vec3f operator*(Vec3f v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}
constexpr float dot(Vec3f a, Vec3f b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
constexpr Vec3f cross(Vec3f a, Vec3f b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline bool isFinite(Vec3f v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline bool isFinite(Vec4f v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
         std::isfinite(v.w);
}
Vec3f normalized(Vec3f value);

// Column-major storage; column vectors; clip = projection * view * model * p.
struct Matrix4 {
  std::array<float, 16> elements{1, 0, 0, 0, 0, 1, 0, 0,
                                 0, 0, 1, 0, 0, 0, 0, 1};
  constexpr float &at(std::size_t row, std::size_t column) {
    if (row >= 4 || column >= 4)
      throw std::out_of_range("Matrix index");
    return elements.at(column * 4 + row);
  }
  constexpr float at(std::size_t row, std::size_t column) const {
    if (row >= 4 || column >= 4)
      throw std::out_of_range("Matrix index");
    return elements.at(column * 4 + row);
  }
  bool operator==(const Matrix4 &) const = default;
};
Matrix4 operator*(const Matrix4 &a, const Matrix4 &b);
Vec4f operator*(const Matrix4 &matrix, Vec4f vector);
bool isFinite(const Matrix4 &matrix);
Matrix4 translation(Vec3f offset);
Matrix4 scaling(Vec3f scale);

// Left-handed, +Y up, camera looking +Z. NDC z is [0,1]. UI remains +Y down;
// the viewport/backend handles the mapping without changing UI box geometry.
Matrix4 perspectiveLH(float verticalFovRadians, float aspect, float nearPlane,
                      float farPlane);
Matrix4 orthographicLH(float width, float height, float nearPlane,
                       float farPlane);
Matrix4 lookAtLH(Vec3f eye, Vec3f target, Vec3f up = {0, 1, 0});

} // namespace playground::math
