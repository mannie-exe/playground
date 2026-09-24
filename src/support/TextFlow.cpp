#include <utf8proc.h>

#include <support/TextFlow.hpp>

namespace playground::ui {

std::vector<std::size_t> graphemeBoundaries(std::string_view value) {
  std::vector<std::size_t> result{0};
  utf8proc_int32_t previous{}, state{};
  for (std::size_t offset = 0; offset < value.size();) {
    utf8proc_int32_t current{};
    const auto length = utf8proc_iterate(
        reinterpret_cast<const utf8proc_uint8_t *>(value.data() + offset),
        static_cast<utf8proc_ssize_t>(value.size() - offset), &current);
    if (length <= 0)
      throw std::invalid_argument("Text must contain valid UTF-8");
    if (offset && utf8proc_grapheme_break_stateful(previous, current, &state))
      result.push_back(offset);
    previous = current;
    offset += static_cast<std::size_t>(length);
  }
  if (!value.empty())
    result.push_back(value.size());
  return result;
}

} // namespace playground::ui
