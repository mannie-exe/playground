#pragma once
#include <random>

namespace Random {
using namespace std;

random_device _randomDevice;
mt19937 randomGenerator{_randomDevice()};

int Int(int min, int max) {
  uniform_int_distribution<int> get{min, max};
  return get(randomGenerator);
}
} // namespace Random
