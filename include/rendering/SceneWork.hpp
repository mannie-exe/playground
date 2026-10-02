#pragma once
#include <cstdint>

namespace playground::rendering {
struct SceneWork {
  std::uint64_t textureHits{}, textureMisses{}, meshHits{}, meshMisses{};
  std::uint64_t uploadBytes{}, uploads{}, evictions{}, refusedPlans{};
  std::uint64_t workingSetBytes{};
  double preparationMilliseconds{};

  void add(const SceneWork &v) noexcept {
    textureHits += v.textureHits;
    textureMisses += v.textureMisses;
    meshHits += v.meshHits;
    meshMisses += v.meshMisses;
    uploadBytes += v.uploadBytes;
    uploads += v.uploads;
    evictions += v.evictions;
    refusedPlans += v.refusedPlans;
    if (v.workingSetBytes)
      workingSetBytes = v.workingSetBytes;
    preparationMilliseconds += v.preparationMilliseconds;
  }
};
} // namespace playground::rendering
