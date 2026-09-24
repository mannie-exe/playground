#include <algorithm>
#include <limits>
#include <stdexcept>

#include <math/Geometry3D.hpp>

namespace playground::math {
namespace {
float finiteFloat(double value) {
  if (!std::isfinite(value) ||
      std::abs(value) > std::numeric_limits<float>::max())
    throw std::overflow_error("Transform exceeds finite float range");
  return static_cast<float>(value);
}
void affine(const Matrix4 &matrix) {
  if (!isFinite(matrix) || matrix.at(3, 0) != 0 || matrix.at(3, 1) != 0 ||
      matrix.at(3, 2) != 0 || matrix.at(3, 3) != 1)
    throw std::invalid_argument(
        "Direction/normal transformation requires an affine matrix");
}
} // namespace

Quaternion normalizedRotation(Quaternion q) {
  const double length = std::hypot(std::hypot(double(q.x), double(q.y)),
                                   std::hypot(double(q.z), double(q.w)));
  if (!std::isfinite(length) || length == 0)
    throw std::invalid_argument(
        "Cannot normalize zero or nonfinite quaternion");
  return {finiteFloat(q.x / length), finiteFloat(q.y / length),
          finiteFloat(q.z / length), finiteFloat(q.w / length)};
}
Quaternion axisAngle(Vec3f axis, float radians) {
  if (!std::isfinite(radians))
    throw std::invalid_argument("Rotation angle must be finite radians");
  axis = normalized(axis);
  const double sine = std::sin(double(radians) / 2);
  return normalizedRotation(Quaternion{
      finiteFloat(axis.x * sine), finiteFloat(axis.y * sine),
      finiteFloat(axis.z * sine), finiteFloat(std::cos(double(radians) / 2))});
}
Quaternion operator*(Quaternion a, Quaternion b) {
  a = normalizedRotation(a);
  b = normalizedRotation(b);
  return normalizedRotation(
      Quaternion{finiteFloat(double(a.w) * b.x + double(a.x) * b.w +
                             double(a.y) * b.z - double(a.z) * b.y),
                 finiteFloat(double(a.w) * b.y - double(a.x) * b.z +
                             double(a.y) * b.w + double(a.z) * b.x),
                 finiteFloat(double(a.w) * b.z + double(a.x) * b.y -
                             double(a.y) * b.x + double(a.z) * b.w),
                 finiteFloat(double(a.w) * b.w - double(a.x) * b.x -
                             double(a.y) * b.y - double(a.z) * b.z)});
}
Matrix4 rotation(Quaternion value) {
  const auto q = normalizedRotation(value);
  const double x = q.x, y = q.y, z = q.z, w = q.w;
  Matrix4 result;
  result.at(0, 0) = finiteFloat(1 - 2 * (y * y + z * z));
  result.at(0, 1) = finiteFloat(2 * (x * y - z * w));
  result.at(0, 2) = finiteFloat(2 * (x * z + y * w));
  result.at(1, 0) = finiteFloat(2 * (x * y + z * w));
  result.at(1, 1) = finiteFloat(1 - 2 * (x * x + z * z));
  result.at(1, 2) = finiteFloat(2 * (y * z - x * w));
  result.at(2, 0) = finiteFloat(2 * (x * z - y * w));
  result.at(2, 1) = finiteFloat(2 * (y * z + x * w));
  result.at(2, 2) = finiteFloat(1 - 2 * (x * x + y * y));
  return result;
}
Matrix4 inverse(const Matrix4 &matrix) {
  if (!isFinite(matrix))
    throw std::invalid_argument("Cannot invert nonfinite matrix");
  double rows[4][8]{};
  for (std::size_t r = 0; r < 4; ++r) {
    for (std::size_t c = 0; c < 4; ++c)
      rows[r][c] = matrix.at(r, c);
    rows[r][r + 4] = 1;
  }
  for (std::size_t c = 0; c < 4; ++c) {
    std::size_t pivot = c;
    for (std::size_t r = c + 1; r < 4; ++r)
      if (std::abs(rows[r][c]) > std::abs(rows[pivot][c]))
        pivot = r;
    if (rows[pivot][c] == 0)
      throw std::invalid_argument("Cannot invert singular matrix");
    for (std::size_t k = 0; k < 8; ++k)
      std::swap(rows[pivot][k], rows[c][k]);
    const double divisor = rows[c][c];
    for (double &value : rows[c])
      value /= divisor;
    for (std::size_t r = 0; r < 4; ++r) {
      if (r == c)
        continue;
      const double factor = rows[r][c];
      for (std::size_t k = 0; k < 8; ++k)
        rows[r][k] -= factor * rows[c][k];
    }
  }
  Matrix4 result;
  for (std::size_t r = 0; r < 4; ++r)
    for (std::size_t c = 0; c < 4; ++c)
      result.at(r, c) = finiteFloat(rows[r][c + 4]);
  return result;
}
Vec3f transformPoint(const Matrix4 &matrix, Vec3f point) {
  const auto p = matrix * Vec4f{point.x, point.y, point.z, 1};
  if (p.w == 0)
    throw std::invalid_argument("Projected point has zero homogeneous W");
  return {finiteFloat(double(p.x) / p.w), finiteFloat(double(p.y) / p.w),
          finiteFloat(double(p.z) / p.w)};
}
Vec3f transformDirection(const Matrix4 &matrix, Vec3f direction) {
  affine(matrix);
  const auto v = matrix * Vec4f{direction.x, direction.y, direction.z, 0};
  return {v.x, v.y, v.z};
}
Vec3f transformNormal(const Matrix4 &model, Vec3f normal) {
  affine(model);
  const auto inverted = inverse(model);
  return normalized(Vec3f{finiteFloat(double(inverted.at(0, 0)) * normal.x +
                                      double(inverted.at(1, 0)) * normal.y +
                                      double(inverted.at(2, 0)) * normal.z),
                          finiteFloat(double(inverted.at(0, 1)) * normal.x +
                                      double(inverted.at(1, 1)) * normal.y +
                                      double(inverted.at(2, 1)) * normal.z),
                          finiteFloat(double(inverted.at(0, 2)) * normal.x +
                                      double(inverted.at(1, 2)) * normal.y +
                                      double(inverted.at(2, 2)) * normal.z)});
}
Matrix4 Transform3D::matrix() const {
  return translation(position) * rotation(orientation) * scaling(scale);
}

} // namespace playground::math
