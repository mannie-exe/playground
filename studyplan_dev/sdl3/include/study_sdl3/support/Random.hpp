#pragma once

#include <random>

namespace study_sdl3 {
class Random {
  std::random_device _randomDevice;
  std::mt19937 _randomGenerator;

public:
  Random() : _randomGenerator(_randomDevice()) {}

  int get(int min, int max) {
    std::uniform_int_distribution<int> gen{min, max};
    return gen(_randomGenerator);
  }

  float get(float min, float max) {
    std::uniform_real_distribution<float> gen{min, max};
    return gen(_randomGenerator);
  }

  double get(double min, double max) {
    std::uniform_real_distribution<double> gen{min, max};
    return gen(_randomGenerator);
  }

  Random(const Random &) = delete;
  Random &operator=(const Random &) = delete;
};
} // namespace study_sdl3
