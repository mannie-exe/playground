#include <rendering/AllocationLimits.hpp>
#include <rendering/ResourceDomain.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    const auto first = rendering::acquireResourceDomain();
    const auto second = rendering::acquireResourceDomain();
    test::require(
        first != second && first != rendering::ResourceDomainId::cpu(),
        "device domains are unique and differ from CPU compatibility");
    test::require(!rendering::ResourceDomainId{} && bool(first),
                  "default domain is explicitly invalid");

    rendering::AllocationLimits limits;
    test::require(limits.validateTarget({32, 16}, 8) == 4096,
                  "target accounting uses actual format byte size");
    test::rejects<std::length_error>(
        [&] { limits.validateTarget({16385, 1}, 4); },
        "per-axis target safeguard");
    test::rejects<std::length_error>(
        [&] { limits.validateTarget({8192, 8192}, 8); }, "target byte budget");
    test::rejects<std::invalid_argument>(
        [&] { limits.validateTarget({0, 16}, 8); }, "empty target rejected");
    test::rejects<std::overflow_error>(
        [] {
          rendering::AllocationLimits::textureBytes(
              {2, 2}, std::numeric_limits<std::size_t>::max());
        },
        "byte product overflow rejected before multiplication");
    limits.maxUploadBytes = 8;
    limits.validateUpload(8);
    test::rejects<std::length_error>(
        [&] { limits.validateUpload(9); },
        "upload policy applies before native narrowing");
    limits.maxResidentBytes = 0;
    limits.validate();
  });
}
