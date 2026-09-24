#pragma once

#include <stdexcept>

#include <rendering/PaintImage.hpp>
#include <rendering/ResourceDomain.hpp>

namespace playground::rendering {

// Provider-specific immutable text description, borrowed only during
// preparation. Implementations retain any font/atlas inputs required by
// submitted work.
class TextSource {
public:
  virtual ~TextSource() = default;
};

class TextImagePreparer {
public:
  virtual ~TextImagePreparer() = default;
  virtual ResourceDomainId resourceDomain() const noexcept = 0;
  virtual bool isEnabled() const noexcept { return true; }
  virtual PaintImageHandle prepareText(const TextSource &source) = 0;
};

inline PaintImageHandle prepareTextImage(const TextSource &source,
                                         TextImagePreparer &preparer) {
  auto result = preparer.prepareText(source);
  if (!result || !math::isFinite(result->pixelSize()) ||
      !math::hasArea(result->pixelSize()) || !isValid(result->alphaMode()) ||
      !isValid(result->colorEncoding()))
    throw std::runtime_error(
        "Text preparer returned invalid pixels or metadata");
  return result;
}

} // namespace playground::rendering
