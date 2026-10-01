#include <limits>
#include <string>
#include <vector>

#include <rendering/AllocationBudget.hpp>
#include <rendering/AllocationLimits.hpp>
#include <rendering/Submission.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    rendering::AllocationBudget budget{16};
    auto allocation = budget.reserve(12);
    test::require(budget.bytes() == 12, "reservation accounts bytes");
    test::rejects<std::length_error>([&] { budget.reserve(5); },
                                     "live cap rejects growth");
    std::string message;
    try {
      budget.reserve(5);
    } catch (const std::length_error &error) {
      message = error.what();
    }
    test::require(message.contains("requested 5 bytes") &&
                      message.contains("12 bytes in use") &&
                      message.contains("16 byte limit"),
                  "budget refusal explains request, usage and capacity");
    test::require(budget.bytes() == 12,
                  "failed reservation preserves accounting");
    auto recording = std::make_shared<rendering::ResourceUse>(
        rendering::ResourceDomainId::cpu());
    recording->allocation = allocation;
    allocation.reset();
    test::require(budget.bytes() == 12,
                  "submission lease keeps allocation accounted");
    recording.reset();
    test::require(budget.bytes() == 0, "retirement releases accounting");
    auto full = budget.reserve(16);
    test::rejects<std::length_error>(
        [&] { budget.reserve(std::numeric_limits<std::size_t>::max()); },
        "overflow-sized request rejected");
    test::require(budget.bytes() == 16, "overflow rejection is atomic");
    rendering::AllocationBudget empty{0};
    test::rejects<std::length_error>([&] { empty.reserve(1); },
                                     "zero cap permits no allocation");

    const rendering::AllocationLimits limits;
    rendering::AllocationBudget targets{limits.maxLivePoolBytes};
    std::vector<std::shared_ptr<void>> frames;
    for (int frame = 0; frame < 2; ++frame) {
      // A 5K application frame, scene color/depth, and tone-mapped output,
      // with the preceding frame still awaiting GPU completion.
      for (const auto bytesPerPixel : {8, 8, 4, 8})
        frames.push_back(targets.reserve(
            limits.validateTarget({5120, 2880}, bytesPerPixel)));
    }
    test::require(targets.bytes() == 825753600,
                  "default live budget admits two overlapping 5K scene frames");
    const auto used = targets.bytes();
    test::rejects<std::length_error>(
        [&] { targets.reserve(limits.maxLivePoolBytes - used + 1); },
        "larger defaults still enforce the aggregate live ceiling");
    test::require(targets.bytes() == used,
                  "refused growth leaves outstanding reservations intact");
    frames.clear();
    test::require(targets.bytes() == 0,
                  "retiring the frame leases releases all accounted bytes");
  });
}
