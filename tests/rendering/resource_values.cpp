#include <rendering/AllocationLimits.hpp>
#include <rendering/ResourceDomain.hpp>
#include <support/Test.hpp>
#include <string>

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
    test::require(limits.validateTarget({4112, 2514}, 8) == 82700544,
                  "reported high-DPI drawable fits the default frame policy");
    test::require(limits.validateTarget({6016, 3384}, 12) == 244297728,
                  "native 6K scene color and depth fit the default target policy");
    limits.validateUpload(256 * 1024 * 1024);
    test::rejects<std::length_error>(
        [&] { limits.validateUpload(256 * 1024 * 1024 + 1); },
        "default upload policy rejects oversized staging allocations");
    test::rejects<std::length_error>(
        [&] { limits.validateTarget({16385, 1}, 4); },
        "per-axis target safeguard");
    test::rejects<std::length_error>(
        [&] { limits.validateTarget({8192, 8192}, 8); }, "target byte budget");
    for (const bool dimensions : {true, false}) {
      rendering::AllocationLimits diagnosticLimits;
      diagnosticLimits.maxTextureDimension = dimensions ? 16 : 32;
      diagnosticLimits.maxTargetBytes = dimensions ? 4096 : 2048;
      std::string message;
      try {
        diagnosticLimits.validateTarget({32, 16}, 8, "Test layer capture");
      } catch (const std::length_error &error) {
        message = error.what();
      }
      test::require(
          message.contains("Test layer capture") && message.contains("32x16") &&
              message.contains("8 bytes/pixel") &&
              message.contains("4096 bytes") &&
              message.contains(dimensions ? "dimension limit" : "byte limit") &&
              message.contains(dimensions ? "16 pixels/axis" : "2048 bytes/target"),
          "target refusal identifies operation, extent, accounting and policy");
      diagnosticLimits.maxTextureDimension = 32;
      diagnosticLimits.maxTargetBytes = 4096;
      test::require(diagnosticLimits.validateTarget({32, 16}, 8) == 4096,
                    "exact dimension and byte limits remain admissible");
    }
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
    std::string uploadMessage;
    try {
      limits.validateUpload(9, "Test mesh upload");
    } catch (const std::length_error &error) {
      uploadMessage = error.what();
    }
    test::require(uploadMessage.contains("Test mesh upload") &&
                      uploadMessage.contains("requested 9 bytes") &&
                      uploadMessage.contains("1..8 bytes"),
                  "upload refusal includes operation and allowed byte range");
    limits.maxResidentBytes = 0;
    limits.validate();
  });
}
