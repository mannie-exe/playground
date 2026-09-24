#pragma once

#include <stdexcept>

#include <rendering/PaintImage.hpp>
#include <rendering/ResourceDomain.hpp>

namespace playground::rendering {

// Source pixels are immutable for the lifetime of a handle. Replace the source
// handle after edits. Implementations own device-local realization/caching;
// preparation may enqueue uploads but preserves dimensions, alpha and encoding.
class ImagePreparer {
public:
  virtual ~ImagePreparer() = default;
  virtual ResourceDomainId resourceDomain() const noexcept {
    return ResourceDomainId::cpu();
  }
  virtual PaintImageHandle prepare(PaintImageHandle source) = 0;
};

inline PaintImageHandle prepareImage(PaintImageHandle source,
                                     ImagePreparer *preparer) {
  if (!source)
    throw std::invalid_argument("Image preparation requires a source");
  if (!math::isFinite(source->pixelSize()) ||
      !math::hasArea(source->pixelSize()) || !isValid(source->alphaMode()) ||
      !isValid(source->colorEncoding()))
    throw std::invalid_argument("Image source has unknown pixel semantics");
  auto result = preparer ? preparer->prepare(source) : source;
  if (!result || result->pixelSize() != source->pixelSize() ||
      result->alphaMode() != source->alphaMode() ||
      result->colorEncoding() != source->colorEncoding())
    throw std::runtime_error("Image preparer returned an invalid realization");
  return result;
}

} // namespace playground::rendering
