#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace playground::ui {

enum class TextTruncation { None, EllipsisStart, EllipsisMiddle, EllipsisEnd };
enum class WritingMode { HorizontalTb, VerticalRl, VerticalLr };
enum class TextOrientation { Mixed, Upright, Sideways };

struct TextFlowProps {
  TextTruncation truncation{TextTruncation::None};
  std::optional<std::size_t> maximumLines;
  std::string ellipsis{"\xE2\x80\xA6"};
  WritingMode writingMode{WritingMode::HorizontalTb};
  TextOrientation orientation{TextOrientation::Mixed};
  bool operator==(const TextFlowProps &) const = default;
};

// Byte offsets are returned only at extended grapheme cluster boundaries.
std::vector<std::size_t> graphemeBoundaries(std::string_view value);

template <typename Fits>
std::string truncateText(std::string_view value, const TextFlowProps &props,
                         Fits fits) {
  if (props.truncation == TextTruncation::None || fits(value))
    return std::string{value};
  const auto boundaries = graphemeBoundaries(value);
  (void)graphemeBoundaries(props.ellipsis);
  if (!fits(props.ellipsis))
    return {};
  // Shaping can change at a join: do not assume monotonic character widths.
  for (std::size_t count = boundaries.size() - 1; count > 0; --count) {
    const std::size_t keep = count - 1, clusters = boundaries.size() - 1;
    std::string candidate;
    switch (props.truncation) {
    case TextTruncation::EllipsisStart:
      candidate = props.ellipsis +
                  std::string{value.substr(boundaries[clusters - keep])};
      break;
    case TextTruncation::EllipsisMiddle: {
      const auto leading = (keep + 1) / 2, trailing = keep / 2;
      candidate = std::string{value.substr(0, boundaries[leading])} +
                  props.ellipsis +
                  std::string{value.substr(boundaries[clusters - trailing])};
      break;
    }
    case TextTruncation::EllipsisEnd:
      candidate =
          std::string{value.substr(0, boundaries[keep])} + props.ellipsis;
      break;
    default:
      throw std::invalid_argument("Unknown text truncation mode");
    }
    if (fits(candidate))
      return candidate;
  }
  return props.ellipsis;
}

} // namespace playground::ui
