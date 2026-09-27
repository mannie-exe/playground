#pragma once

#include <cstdint>

namespace playground::rendering {
// Recording/submission counters, not fragment invocations or GPU utilization.
struct PaintWork {
  std::uint64_t considered{}, rejected{}, quads{}, drawCalls{}, streamedBytes{};
  std::uint64_t rectangularQuads{}, generalQuads{}, presentationQuads{};

  void add(const PaintWork &v) noexcept {
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
