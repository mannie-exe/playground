#include <ui/content/ContentTypes.hpp>

namespace playground::ui::content_detail {

void validate(const ContentStyle &style) {
  switch (style.fit) {
  case ContentFit::None:
  case ContentFit::Stretch:
  case ContentFit::Contain:
  case ContentFit::Cover:
  case ContentFit::Shrink:
    break;
  default:
    throw std::invalid_argument("Unknown content fit mode");
  }
  for (auto align : {style.alignment.horizontal, style.alignment.vertical})
    if (align != layout::Align::Start && align != layout::Align::Center &&
        align != layout::Align::End)
      throw std::invalid_argument("Content alignment must be Start, Center, or "
                                  "End; use Stretch fit for stretching");
  if (style.paint.sampling != rendering::Sampling::Nearest &&
      style.paint.sampling != rendering::Sampling::Linear)
    throw std::invalid_argument("Unknown content sampling mode");
}

float alignmentFactor(layout::Align alignment, bool reverse) {
  if (alignment == layout::Align::Center)
    return 0.5f;
  return (alignment == layout::Align::End) != reverse ? 1.0f : 0.0f;
}

ResolvedContent resolve(math::Rect source, math::Size2 natural,
                        math::Rect destination, const ContentStyle &style,
                        layout::LayoutDirection direction) {
  if (!source.hasArea() || !math::hasArea(natural) || !destination.hasArea())
    return {source, {destination.position, {}}};
  const float horizontal =
      alignmentFactor(style.alignment.horizontal,
                      direction == layout::LayoutDirection::RightToLeft);
  const float vertical = alignmentFactor(style.alignment.vertical);
  if (style.fit == ContentFit::Stretch)
    return {source, destination};
  if (style.fit == ContentFit::Cover) {
    const float scale = std::max(destination.w() / natural.width,
                                 destination.h() / natural.height);
    const math::Size2 crop{
        source.w() * destination.w() / (natural.width * scale),
        source.h() * destination.h() / (natural.height * scale)};
    source.position += math::Vec2f{(source.w() - crop.width) * horizontal,
                                   (source.h() - crop.height) * vertical};
    source.size = crop;
    return {source, destination};
  }
  float scale = style.fit == ContentFit::None
                    ? 1.0f
                    : std::min(destination.w() / natural.width,
                               destination.h() / natural.height);
  if (style.fit == ContentFit::Shrink)
    scale = std::min(1.0f, scale);
  const auto size = natural * scale;
  return {source,
          {{destination.x() + (destination.w() - size.width) * horizontal,
            destination.y() + (destination.h() - size.height) * vertical},
           size}};
}

} // namespace playground::ui::content_detail
