#pragma once

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace playground::test {
inline void require(bool condition, std::string_view message) {
  if (!condition)
    throw std::runtime_error(std::string{message});
}

template <class Exception = std::invalid_argument, class Operation>
void rejects(Operation operation, std::string_view message) {
  try {
    operation();
  } catch (const Exception &) {
    return;
  }
  throw std::runtime_error(std::string{message});
}

template <typename Test> int run(Test test) {
  try {
    test();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
  } catch (const std::string &error) {
    std::cerr << error << '\n';
  } catch (...) {
    std::cerr << "Unexpected non-standard exception\n";
  }
  return 1;
}
} // namespace playground::test
