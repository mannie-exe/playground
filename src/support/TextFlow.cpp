#include <support/TextFlow.hpp>

namespace playground::ui {

std::vector<std::size_t> graphemeBoundaries(std::string_view value) {
  return support::textBoundaries(value, support::TextBoundary::Grapheme);
}

} // namespace playground::ui
