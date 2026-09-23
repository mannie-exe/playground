#include <support/SVGDocument.hpp>

#include <cmath>
#include <format>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <pugixml.hpp>

namespace playground {

static pugi::xml_node find(pugi::xml_node node, const std::string &id) {
  pugi::xml_node result;
  std::vector<pugi::xml_node> pending{node};
  while (!pending.empty()) {
    auto current = pending.back();
    pending.pop_back();
    if (current.attribute("id").value() == id) {
      if (result)
        throw std::invalid_argument("Duplicate SVG element ID: " + id);
      result = current;
    }
    for (auto child : current.children())
      pending.push_back(child);
  }
  return result;
}

SVGDocument::SVGDocument(std::string xml) : _xml{std::move(xml)} {
  pugi::xml_document doc;
  const auto result = doc.load_buffer(_xml.data(), _xml.size());
  if (!result || std::string_view{doc.document_element().name()} != "svg")
    throw std::invalid_argument("Invalid SVG document");
}

std::string SVGDocument::styled(const SVGStyleOverrides &styles) const {
  if (styles.empty())
    return _xml;
  pugi::xml_document document;
  if (!document.load_buffer(_xml.data(), _xml.size()))
    throw std::runtime_error("Failed to copy SVG document");
  for (const auto &[id, style] : styles) {
    if (id.empty())
      throw std::invalid_argument("SVG override requires an element ID");
    auto node = find(document.document_element(), id);
    if (!node)
      throw std::invalid_argument("SVG element ID not found: " + id);
    const std::string_view name = node.name();
    if (name != "svg" && name != "g" && name != "path" && name != "rect" &&
        name != "circle" && name != "ellipse" && name != "line" &&
        name != "polyline" && name != "polygon")
      throw std::invalid_argument("Unsupported SVG style target: " +
                                  std::string{name});
    if (std::string_view{node.attribute("style").value()}.find("!important") !=
        std::string_view::npos)
      throw std::invalid_argument(
          "SVG !important cascade is not supported by overrides");
    auto set = [&](const char *name, const std::string &value) {
      auto attribute = node.attribute(name);
      if (!attribute)
        attribute = node.append_attribute(name);
      if (!attribute.set_value(value.c_str()))
        throw std::bad_alloc{};
      // Inline declarations override presentation attributes. Append the
      // explicitly authored value last, preserving unrelated declarations.
      auto inlineStyle = node.attribute("style");
      if (inlineStyle) {
        const auto updated =
            std::string{inlineStyle.value()} + ";" + name + ":" + value;
        if (!inlineStyle.set_value(updated.c_str()))
          throw std::bad_alloc{};
      }
    };
    auto paint = [&](const char *name, const char *alphaName,
                     const SVGPaint &value) {
      if (const auto *color = std::get_if<math::ColorRGBA8>(&value)) {
        set(name,
            std::format("#{:02x}{:02x}{:02x}", color->r, color->g, color->b));
        set(alphaName, std::to_string(color->a / 255.0f));
      } else
        set(name, "none");
    };
    if (style.fill)
      paint("fill", "fill-opacity", *style.fill);
    if (style.stroke)
      paint("stroke", "stroke-opacity", *style.stroke);
    if (style.strokeWidth) {
      if (!std::isfinite(*style.strokeWidth) || *style.strokeWidth < 0)
        throw std::invalid_argument("Invalid SVG stroke width");
      set("stroke-width", std::to_string(*style.strokeWidth));
    }
    if (style.opacity) {
      if (!std::isfinite(*style.opacity) || *style.opacity < 0 ||
          *style.opacity > 1)
        throw std::invalid_argument("Invalid SVG opacity");
      set("opacity", std::to_string(*style.opacity));
    }
  }
  std::ostringstream output;
  document.save(output, "", pugi::format_raw);
  return output.str();
}

} // namespace playground
