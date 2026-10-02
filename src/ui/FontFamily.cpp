#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <ui/FontFamily.hpp>

namespace playground::ui {
void FontSelection::validate() const {
  if (weight < 1 || weight > 1000 || slant < FontSlant::Upright ||
      slant > FontSlant::Italic || opticalSize < FontOpticalSize::Text ||
      opticalSize > FontOpticalSize::Display)
    throw std::invalid_argument("Invalid font selection");
}

FontFamilyDefinition::FontFamilyDefinition(std::string name,
                                           std::vector<FontFace> faces)
    : name{std::move(name)}, faces{std::move(faces)} {
  validate();
}

void FontFamilyDefinition::validate() const {
  if (name.empty() || faces.empty())
    throw std::invalid_argument("Font family requires a name and faces");
  std::set<std::tuple<int, FontSlant, FontOpticalSize>> selections;
  std::set<std::string> identities;
  for (const auto &face : faces) {
    face.selection.validate();
    if (face.id.empty() || face.path.empty() ||
        face.path.find('\0') != std::string::npos ||
        !identities.insert(face.id).second ||
        !selections
             .emplace(face.selection.weight, face.selection.slant,
                      face.selection.opticalSize)
             .second)
      throw std::invalid_argument("Invalid or duplicate font face");
  }
}

const FontFace &selectFontFace(const FontFamilyDefinition &family,
                               FontSelection request) {
  request.validate();
  if (family.faces.empty())
    throw std::invalid_argument("Cannot select from an empty font family");
  const auto rank = [&](const FontFace &face) {
    const auto &s = face.selection;
    return std::tuple{s.slant != request.slant,
                      s.opticalSize == request.opticalSize     ? 0
                      : s.opticalSize == FontOpticalSize::Text ? 1
                                                               : 2,
                      std::abs(s.weight - request.weight),
                      s.weight,
                      s.opticalSize,
                      face.id};
  };
  return *std::min_element(
      family.faces.begin(), family.faces.end(),
      [&](const auto &a, const auto &b) { return rank(a) < rank(b); });
}
} // namespace playground::ui
