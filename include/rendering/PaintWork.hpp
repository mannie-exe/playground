#pragma once

#include <cstdint>

#include <rendering/SceneWork.hpp>

namespace playground::rendering {
// Recording/submission counters, not fragment invocations or GPU utilization.
struct PaintWork {
  SceneWork scene;
  std::uint64_t considered{}, rejected{}, quads{}, drawCalls{}, streamedBytes{};
  std::uint64_t rectangularQuads{}, generalQuads{}, presentationQuads{};

  void add(const PaintWork &v) noexcept {
    scene.add(v.scene);
    considered += v.considered;
    rejected += v.rejected;
    quads += v.quads;
    drawCalls += v.drawCalls;
    streamedBytes += v.streamedBytes;
    rectangularQuads += v.rectangularQuads;
    generalQuads += v.generalQuads;
    presentationQuads += v.presentationQuads;
  }
};
} // namespace playground::rendering
