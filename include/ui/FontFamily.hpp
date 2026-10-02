#pragma once

#include <memory>
#include <string>
#include <vector>

namespace playground::ui {
enum class FontSlant { Upright, Italic };
enum class FontOpticalSize { Text, Caption, SmallText, Subhead, Display };

struct FontSelection {
  int weight{400};
  FontSlant slant{FontSlant::Upright};
  FontOpticalSize opticalSize{FontOpticalSize::Text};
  void validate() const;
  bool operator==(const FontSelection &) const = default;
};

// Resolved asset sources, not open font objects. Definitions are immutable once
// published and do not depend on the platform text renderer.
struct FontFace {
  std::string id, path, cacheIdentity;
  FontSelection selection;
  bool operator==(const FontFace &) const = default;
};

struct FontFamilyDefinition {
  const std::string name;
  const std::vector<FontFace> faces;
  FontFamilyDefinition(std::string name, std::vector<FontFace> faces);

private:
  void validate() const;
};

using FontFamilyHandle = std::shared_ptr<const FontFamilyDefinition>;

const FontFace &selectFontFace(const FontFamilyDefinition &, FontSelection);
} // namespace playground::ui
