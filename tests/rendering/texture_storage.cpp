#include <array>
#include <barrier>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>

#include <rendering/Texture.hpp>
#include <support/PreparationBudget.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::rendering;

int main() {
  return test::run([] {
    const auto globalBefore =
        defaultResourceLedger()->snapshot().memory[0].bytes;
    auto ledger = std::make_shared<ResourceLedger>();
    auto otherLedger = std::make_shared<ResourceLedger>();
    {
      auto texture = makeTexture(
          TextureLevel{{2, 2}, std::vector<math::Vec4f>(4, {1, .5f, 0, .5f})},
          TextureRole::Color, MipPolicy::Generate, 1024, ledger);
      auto copy = std::make_shared<Texture>(*texture);
      const auto &associated = copy->upload(false);
      const auto &opaque = copy->upload(true);
      test::require(
          associated.resources() == ledger && opaque.resources() == ledger &&
              ledger->snapshot().memory[0].bytes > texture->bytes() &&
              otherLedger->snapshot().memory[0].bytes == 0 &&
              defaultResourceLedger()->snapshot().memory[0].bytes ==
                  globalBefore,
          "copies, mips and upload variants retain injected accounting");
      auto budgets = ledger->snapshot().budgets;
      budgets.cpuBytes = 1;
      ledger->setBudgets(budgets);
      test::rejects<ResourcePressure>(
          [&] { Texture rejected{*texture}; },
          "copies obey the owning ledger's live cap");
    }
    test::require(ledger->snapshot().memory[0].bytes == 0,
                  "retained representations release their original ledger");
    for (unsigned bits = 0; bits <= 0xffff; ++bits) {
      if ((bits & 0x7c00) == 0x7c00)
        continue;
      std::vector<std::byte> bytes(8);
      const auto encoded = static_cast<std::uint16_t>(bits);
      std::memcpy(bytes.data(), &encoded, sizeof(encoded));
      const PackedTexels original{TextureFormat::RGBA16F, ColorEncoding::Linear,
                                  std::move(bytes)};
      const std::array value{original[0]};
      const PackedTexels roundTrip{TextureFormat::RGBA16F,
                                   ColorEncoding::Linear, std::span{value}};
      test::require(
          roundTrip == original,
          "every finite half value roundtrips exactly, including signed zero");
    }
    const std::array scalar{math::Vec4f{.5f, 0, 0, 1}};
    const PackedTexels red{TextureFormat::R8, ColorEncoding::Linear,
                           std::span{scalar}};
    test::require(red.data().size() == 1 && red[0].x == 128.f / 255 &&
                      red[0].y == 0 && red[0].w == 1,
                  "single-channel storage has defined missing-channel values");
    for (float value : {0.f, -0.f, 1.f, -2.f, 65504.f, .3333f,
                        std::ldexp(1.f, -24), std::ldexp(1.f, -14)}) {
      const std::array values{math::Vec4f{value, value, value, 1}};
      const PackedTexels half{TextureFormat::RGBA16F, ColorEncoding::Linear,
                              std::span{values}};
      test::require(half.data().size() == 8 &&
                        std::abs(half[0].x - value) <=
                            std::max(1e-8f, std::abs(value) * .0005f),
                    "half storage roundtrip within precision bounds");
    }
    const std::array excessive{math::Vec4f{70000, 0, 0, 1}};
    test::rejects<std::length_error>(
        [&] {
          PackedTexels{TextureFormat::RGBA16F, ColorEncoding::Linear,
                       std::span{excessive}};
        },
        "half overflow is explicit");
    PackedTexels precise{TextureFormat::RGBA32F, ColorEncoding::Linear,
                         std::span{excessive}};
    test::require(precise[0].x == 70000 && precise.data().size() == 16,
                  "full precision remains available");
    RGBA8Image pixels{{2, 1},
                      AlphaMode::Straight,
                      ColorEncoding::SRGB,
                      {255, 0, 0, 0, 0, 255, 0, 255}};
    auto source = makeTexture(pixels, TextureRole::Color);
    test::require(source->bytes() == 12,
                  "byte mip residency, not float working bytes");
    const auto opaque = makeOpaqueTexture(*source);
    test::require(
        std::abs(opaque->levels()[1].texels[0].x - .5f) < .005f,
        "opaque variant filters in linear light before byte encoding");
    auto associated =
        packTexture(*source, TextureFormat::RGBA8, ColorEncoding::SRGB, true);
    test::require(associated->alphaMode() == AlphaMode::Premultiplied &&
                      source->levels()[0].texels[0].x == 1 &&
                      associated->levels()[0].texels[0].x == 0,
                  "association is explicit and source RGB survives");
    test::rejects([&] { makeOpaqueTexture(*associated); },
                  "lost RGB cannot be reconstructed");
    test::rejects<std::length_error>(
        [&] {
          makeTexture(pixels, TextureRole::Color, MipPolicy::Generate, 8);
        },
        "budget includes packed mip tail");
    test::rejects(
        [&] {
          PackedTexels{TextureFormat::RGBA16F, ColorEncoding::SRGB,
                       std::span{excessive}};
        },
        "unsupported encoding/format pair");
    PreparationBudget budget{128};
    std::barrier gate{2};
    std::jthread worker{[&] {
      auto held = budget.acquire(100);
      gate.arrive_and_wait();
      gate.arrive_and_wait();
    }};
    gate.arrive_and_wait();
    bool refused{};
    try {
      auto blocked = budget.acquire(29);
    } catch (const std::length_error &) {
      refused = true;
    }
    auto snapshot = budget.snapshot();
    gate.arrive_and_wait();
    worker.join();
    test::require(
        refused && snapshot.used == 100 && budget.snapshot().used == 0 &&
            budget.snapshot().peak == 100,
        "shared admission refuses overcommit and releases on scope exit");
    auto lease = budget.acquire(32);
    auto moved = std::move(lease);
    test::require(budget.snapshot().used == 32,
                  "moving lease does not double-account");
  });
}
