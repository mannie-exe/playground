#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>

#include <ui/controls/Editing.hpp>

namespace playground::ui {
NumberParse parseNumber(std::string_view text) {
  if (text.empty())
    return {ParseState::Empty};
  if (text == "+" || text == "-" || text == "." || text == "+." || text == "-.")
    return {ParseState::Incomplete};
  if (text.front() == '+')
    text.remove_prefix(1);
  double value{};
  auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
      std::isfinite(value))
    return {ParseState::Valid, value};
  const std::string_view suffix{
      result.ptr, std::size_t(text.data() + text.size() - result.ptr)};
  if (result.ec == std::errc{} && std::isfinite(value) &&
      (suffix == "e" || suffix == "E" || suffix == "e-" || suffix == "e+" ||
       suffix == "E-" || suffix == "E+"))
    return {ParseState::Incomplete};
  return {ParseState::Invalid};
}

std::string formatNumber(double value) {
  std::array<char, 128> buffer;
  auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (result.ec != std::errc{})
    throw std::invalid_argument("Cannot format number");
  return {buffer.data(), result.ptr};
}

ValidationResult validateNumber(double v, const RangeValue &r, bool integer,
                                std::optional<double> multiple) {
  if (!std::isfinite(v))
    return ValidationIssue{"number", "Enter a finite number"};
  if (v < r.minimum || v > r.maximum)
    return ValidationIssue{"range", "Enter a value from " +
                                        formatNumber(r.minimum) + " to " +
                                        formatNumber(r.maximum)};
  if (integer && std::trunc(v) != v)
    return ValidationIssue{"integer", "Enter a whole number"};
  if (multiple) {
    if (!std::isfinite(*multiple) || *multiple <= 0)
      throw std::invalid_argument("Invalid numeric increment constraint");
    const auto q = (v - r.minimum) / *multiple;
    if (!std::isfinite(q) ||
        std::abs(q - std::round(q)) > 1e-9 * std::max(1., std::abs(q)))
      return ValidationIssue{"increment",
                             "Use increments of " + formatNumber(*multiple)};
  }
  return {};
}
} // namespace playground::ui
