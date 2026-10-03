#pragma once

#include <charconv>
#include <cmath>
#include <iostream>
#include <optional>
#include <string_view>

namespace playground::test::cli {
// Explicit developer tools use 0 for success/help, 1 for execution failure
// and 2 for invalid usage. Code 77 belongs only to unsupported CTest checks.
inline bool helpRequested(int argc, char **argv) {
  return argc == 2 && std::string_view{argv[1]} == "--help";
}

inline int help(std::string_view usage) {
  std::cout << usage;
  return 0;
}

inline int usageError(std::string_view usage, std::string_view reason = {}) {
  if (!reason.empty())
    std::cerr << reason << '\n';
  std::cerr << usage;
  return 2;
}

// Locale-independent, whole-token parsing; no whitespace, NaN or infinity.
inline std::optional<double> number(std::string_view text, double minimum,
                                    double maximum) {
  if (text.empty())
    return {};
  double value{};
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() ||
      !std::isfinite(value) || value < minimum || value > maximum)
    return {};
  return value;
}
} // namespace playground::test::cli
