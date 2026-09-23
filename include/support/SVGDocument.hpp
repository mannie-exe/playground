#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include <math/Color.hpp>

namespace playground {

struct NoSVGPaint {
  bool operator==(const NoSVGPaint &) const = default;
};
using SVGPaint = std::variant<NoSVGPaint, math::ColorRGBA8>;

struct SVGElementStyle {
  std::optional<SVGPaint> fill, stroke;
  std::optional<float> strokeWidth, opacity;
  bool operator==(const SVGElementStyle &) const = default;
};
using SVGStyleOverrides = std::map<std::string, SVGElementStyle>;

class SVGDocument {
  std::string _xml;

public:
  explicit SVGDocument(std::string xml);

  const std::string &xml() const noexcept { return _xml; }

  std::string styled(const SVGStyleOverrides &styles) const;
};

using SVGDocumentHandle = std::shared_ptr<const SVGDocument>;
using VectorSource = std::variant<std::string, SVGDocumentHandle>;

} // namespace playground
