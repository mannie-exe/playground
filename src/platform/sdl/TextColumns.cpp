#include <platform/sdl/TextColumns.hpp>

#include <support/VerticalOrientation.hpp>
#include <utf8proc.h>

namespace playground::sdl {

bool sidewaysCluster(std::string_view cluster,
                     ui::TextOrientation orientation) {
  bool first = true, enclosing = false;
  ui::VerticalOrientation property{ui::VerticalOrientation::R};
  for (std::size_t offset = 0; offset < cluster.size();) {
    utf8proc_int32_t cp{};
    const auto bytes = utf8proc_iterate(
        reinterpret_cast<const utf8proc_uint8_t *>(cluster.data() + offset),
        cluster.size() - offset, &cp);
    if (bytes <= 0)
      throw std::invalid_argument("Invalid UTF-8 in vertical text");
    const auto *p = utf8proc_get_property(cp);
    if (p->bidi_class == UTF8PROC_BIDI_CLASS_R ||
        p->bidi_class == UTF8PROC_BIDI_CLASS_AL ||
        (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2066 && cp <= 0x2069))
      throw std::invalid_argument(
          "Vertical bidirectional paragraph layout is not supported");
    if (first)
      property = ui::verticalOrientation(cp);
    enclosing |= p->category == UTF8PROC_CATEGORY_ME;
    first = false;
    offset += bytes;
  }
  if (orientation == ui::TextOrientation::Sideways)
    return true;
  if (orientation == ui::TextOrientation::Upright || enclosing)
    return false;
  if (property == ui::VerticalOrientation::Tr)
    throw std::invalid_argument(
        "Mixed Tr glyph fallback cannot be verified by SDL_ttf; choose Upright "
        "or Sideways explicitly");
  return property == ui::VerticalOrientation::R;
}

TextColumns layoutTextColumns(AssetRegistry &assets, FontHandle font,
                              std::string_view value,
                              std::optional<float> height, ui::WritingMode mode,
                              ui::TextOrientation orientation,
                              layout::Align alignment) {
  TextColumns result;
  if (value.empty())
    return result;
  auto horizontalProps = font->getProps(), verticalProps = horizontalProps;
  horizontalProps.layout.direction = TTF_DIRECTION_LTR;
  verticalProps.layout.direction = TTF_DIRECTION_TTB;
  const auto horizontal = assets.getFont(horizontalProps),
             vertical = assets.getFont(verticalProps);
  struct Column {
    math::Size2 size;
    std::vector<TextColumnRun> runs;
  };
  auto shape = [&](std::string_view text) {
    Column column;
    const auto boundaries = ui::graphemeBoundaries(text);
    for (std::size_t i = 1; i < boundaries.size(); ++i) {
      const auto cluster =
          text.substr(boundaries[i - 1], boundaries[i] - boundaries[i - 1]);
      const bool sideways = sidewaysCluster(cluster, orientation);
      if (column.runs.empty() || column.runs.back().sideways != sideways)
        column.runs.push_back(
            {sideways ? horizontal : vertical, {}, {}, sideways});
      column.runs.back().value += cluster;
    }
    for (auto &run : column.runs) {
      int w{}, h{};
      if (!TTF_GetStringSize(run.font->get(), run.value.data(),
                             run.value.size(), &w, &h))
        throwSDLError("Measure vertical text run");
      run.bounds =
          math::rect(0, column.size.height, float(run.sideways ? h : w),
                     float(run.sideways ? w : h));
      column.size.width = std::max(column.size.width, run.bounds.w());
      column.size.height =
          layout::detail::checked(double(column.size.height) + run.bounds.h());
    }
    if (column.runs.empty())
      column.size.width = float(TTF_GetFontHeight(font->get()));
    return column;
  };
  std::vector<Column> columns;
  std::string current;
  const auto boundaries = ui::graphemeBoundaries(value);
  for (std::size_t i = 1; i < boundaries.size(); ++i) {
    const auto cluster =
        value.substr(boundaries[i - 1], boundaries[i] - boundaries[i - 1]);
    if (cluster == "\n" || cluster == "\r" || cluster == "\r\n") {
      columns.push_back(shape(current));
      current.clear();
      continue;
    }
    auto candidate = current + std::string{cluster};
    if (height && !current.empty() && shape(candidate).size.height > *height) {
      columns.push_back(shape(current));
      current.assign(cluster);
    } else
      current = std::move(candidate);
  }
  columns.push_back(shape(current));
  result.count = columns.size();
  const float gap = float(std::max(0, TTF_GetFontLineSkip(font->get()) -
                                          TTF_GetFontHeight(font->get())));
  for (const auto &column : columns) {
    result.size.width = layout::detail::checked(double(result.size.width) +
                                                column.size.width + gap);
    result.size.height = std::max(result.size.height, column.size.height);
  }
  result.size.width -= gap;
  float x{};
  for (auto &column : columns) {
    const float y = alignment == layout::Align::Center
                        ? (result.size.height - column.size.height) / 2
                    : alignment == layout::Align::End
                        ? result.size.height - column.size.height
                        : 0;
    const float physicalX = mode == ui::WritingMode::VerticalRl
                                ? result.size.width - x - column.size.width
                                : x;
    for (auto &run : column.runs) {
      run.bounds.position.x =
          physicalX + (column.size.width - run.bounds.w()) / 2;
      run.bounds.position.y += y;
      result.runs.push_back(std::move(run));
    }
    x += column.size.width + gap;
  }
  return result;
}

} // namespace playground::sdl
