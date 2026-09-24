#include <limits>

#include <rendering/AllocationBudget.hpp>
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
  });
}
