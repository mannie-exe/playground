#pragma once

#include <stdexcept>
#include <ui/PaintImage.hpp>

namespace playground::rendering {

// Source pixels are immutable for the lifetime of a handle. Replace the source
// handle after edits. Implementations own device-local realization/caching;
// preparation may enqueue uploads but must preserve dimensions and alpha mode.
class ImagePreparer {
public:
  virtual ~ImagePreparer() = default;
  virtual ui::PaintImageHandle prepare(ui::PaintImageHandle source) = 0;
};

inline ui::PaintImageHandle prepareImage(ui::PaintImageHandle source,
                                         ImagePreparer *preparer) {
  if (!source)
    throw std::invalid_argument("Image preparation requires a source");
  auto result = preparer ? preparer->prepare(source) : source;
  if (!result || result->pixelSize() != source->pixelSize())
    throw std::runtime_error("Image preparer returned an invalid realization");
  return result;
}

} // namespace playground::rendering
