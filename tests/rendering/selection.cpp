#include <array>

#include <rendering/RendererTypes.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::rendering;

int main() {
  return test::run([] {
    const RendererCandidate software{.capabilities = {.paint2D = true}};
    const RendererCandidate gpu{RendererKind::SDLGPU,
                                GPUDriver::Vulkan,
                                {true, true, CompositionSpace::Linear}};
    const std::array both{software, gpu};
    const std::array onlySoftware{software};
    const RendererRequirements pbr{.metallicRoughness = true};
    test::rejects<std::runtime_error>(
        [&] { selectRenderer({}, pbr, both); },
        "unlit 3D capability does not imply PBR support");
    const std::array litGPU{
        RendererCandidate{RendererKind::SDLGPU,
                          GPUDriver::Vulkan,
                          {true, true, CompositionSpace::Linear, true}}};
    test::require(selectRenderer({}, pbr, litGPU).candidateIndex == 0,
                  "explicit PBR capability is negotiated");
    test::require(
        selectRenderer({}, {}, both).candidateIndex == 1,
        "auto prefers a capable GPU even if software is listed first");
    test::require(selectRenderer({}, {}, onlySoftware).candidateIndex == 0,
                  "auto can choose software without pretending it fell back");
    test::require(!selectRenderer({}, {}, onlySoftware).state.isFallback(),
                  "auto software is not an explicit-preference fallback");
    test::require(
        selectRenderer({RendererChoice::Software}, {}, both).candidateIndex ==
            0,
        "explicit software beats GPU priority");
    const auto fallback =
        selectRenderer({RendererChoice::SDLGPU}, {}, onlySoftware);
    test::require(fallback.state.isFallback() &&
                      fallback.state.requested.backend ==
                          RendererChoice::SDLGPU &&
                      fallback.state.selected.backend == RendererKind::Software,
                  "fallback preserves requested and actual identities");
    test::rejects<std::runtime_error>(
        [&] {
          selectRenderer({RendererChoice::SDLGPU, GPUDriver::Auto, false}, {},
                         onlySoftware);
        },
        "strict unavailable renderer rejected");
    for (RendererRequirements needs :
         {RendererRequirements{true, true, false},
          RendererRequirements{true, false, true}}) {
      test::require(
          selectRenderer({}, needs, both).candidateIndex == 1,
          "3D/linear requirements select a compatible implementation");
      test::rejects<std::runtime_error>(
          [&] { selectRenderer({}, needs, onlySoftware); },
          "fallback never discards requirements");
    }
    const auto driverFallback = selectRenderer(
        {RendererChoice::Auto, GPUDriver::Vulkan}, {}, onlySoftware);
    test::require(driverFallback.state.isFallback() &&
                      driverFallback.candidateIndex == 0,
                  "unavailable Vulkan can fall back to software");
    test::rejects<std::runtime_error>(
        [&] {
          selectRenderer({RendererChoice::Auto, GPUDriver::Vulkan, false}, {},
                         onlySoftware);
        },
        "strict driver remains strict with automatic backend");
    test::require(
        !selectRenderer({RendererChoice::Software, GPUDriver::Vulkan, false},
                        {}, both)
             .state.isFallback(),
        "GPU preferences are dormant for explicit software");
    for (auto driver : {GPUDriver::Vulkan}) {
      const std::array candidates{
          software,
          RendererCandidate{RendererKind::SDLGPU, driver, gpu.capabilities}};
      test::require(selectRenderer({RendererChoice::SDLGPU, driver, false}, {},
                                   candidates)
                            .candidateIndex == 1,
                    "all concrete driver variants negotiate");
    }
    test::rejects<std::runtime_error>([] { selectRenderer({}, {}, {}); },
                                      "empty candidate set fails");
    test::rejects(
        [] { RendererPreferences{static_cast<RendererChoice>(99)}.validate(); },
        "unknown preference rejected");
    test::rejects(
        [] {
          RendererPreferences{RendererChoice::Auto, static_cast<GPUDriver>(99)}
              .validate();
        },
        "unknown driver rejected");
    test::rejects([] { RendererCandidate{RendererKind::SDLGPU}.validate(); },
                  "candidate must name actual GPU driver");
    test::rejects(
        [] {
          RendererCandidate{RendererKind::Software, GPUDriver::Vulkan}
              .validate();
        },
        "software cannot claim GPU driver");
    test::rejects(
        [] {
          RendererCapabilities{true, false, static_cast<CompositionSpace>(99)}
              .supports({});
        },
        "unknown color behavior rejected");
  });
}
