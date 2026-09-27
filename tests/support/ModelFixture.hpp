#pragma once

#include <string_view>

#include <assets/AssetCatalog.hpp>

namespace playground::test {
inline assets::ByteSource modelFixture() {
  constexpr std::string_view text = R"({
    "asset":{"version":"2.0"},
    "buffers":[{"byteLength":66,"uri":"data:application/octet-stream;base64,AACAvwAAgL8AAIA/AACAPwAAgL8AAIA/AAAAAAAAgD8AAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"}],
    "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":60,"byteLength":6}],
    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0
  })";
  const auto bytes = std::as_bytes(std::span{text.data(), text.size()});
  return {{bytes.begin(), bytes.end()}};
}
} // namespace playground::test
