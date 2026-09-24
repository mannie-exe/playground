#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

#include <math/ColorSpace.hpp>
#include <scene/ModelImport.hpp>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace playground::scene {
namespace {
struct ParserBudget {
  std::size_t used{}, limit;
};
struct alignas(std::max_align_t) AllocationHeader {
  std::size_t size;
};
void *allocateParser(void *user, cgltf_size size) {
  auto &budget = *static_cast<ParserBudget *>(user);
  if (size > budget.limit - budget.used ||
      size > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader))
    return nullptr;
  auto *header = static_cast<AllocationHeader *>(
      std::malloc(sizeof(AllocationHeader) + size));
  if (!header)
    return nullptr;
  header->size = size;
  budget.used += size;
  return header + 1;
}
void freeParser(void *user, void *pointer) {
  if (!pointer)
    return;
  auto *header = static_cast<AllocationHeader *>(pointer) - 1;
  static_cast<ParserBudget *>(user)->used -= header->size;
  std::free(header);
}
void check(bool condition, std::string_view message) {
  if (!condition)
    throw std::invalid_argument(std::string{message});
}

std::vector<std::byte> dataURI(std::string_view uri, std::size_t limit) {
  const auto comma = uri.find(',');
  check(comma != std::string_view::npos &&
            uri.substr(0, comma).ends_with(";base64"),
        "Only base64 glTF data URIs are supported");
  const auto data = uri.substr(comma + 1);
  check(data.size() % 4 == 0 && data.size() / 4 <= limit / 3 + 1,
        "Invalid or excessive base64 resource");
  std::vector<std::byte> result;
  result.reserve(std::min(limit, data.size() / 4 * 3));
  auto value = [](char c) -> unsigned {
    if (c >= 'A' && c <= 'Z')
      return c - 'A';
    if (c >= 'a' && c <= 'z')
      return c - 'a' + 26;
    if (c >= '0' && c <= '9')
      return c - '0' + 52;
    if (c == '+')
      return 62;
    if (c == '/')
      return 63;
    throw std::invalid_argument("Invalid base64 character");
  };
  for (std::size_t i = 0; i < data.size(); i += 4) {
    const bool pad2 = data[i + 2] == '=', pad3 = data[i + 3] == '=';
    check((!pad2 || pad3) && (!(pad2 || pad3) || i + 4 == data.size()),
          "Invalid base64 padding");
    const unsigned a = value(data[i]), b = value(data[i + 1]);
    const unsigned c = pad2 ? 0 : value(data[i + 2]);
    const unsigned d = pad3 ? 0 : value(data[i + 3]);
    check((!pad2 || (b & 15) == 0) && (!pad3 || pad2 || (c & 3) == 0),
          "Noncanonical base64 padding");
    result.push_back(std::byte((a << 2) | (b >> 4)));
    if (!pad2)
      result.push_back(std::byte(((b & 15) << 4) | (c >> 2)));
    if (!pad3)
      result.push_back(std::byte(((c & 3) << 6) | d));
  }
  check(result.size() <= limit, "Decoded model resource exceeds budget");
  return result;
}

void validateAccessors(const cgltf_data &data, const ModelImportProps &props) {
  for (std::size_t i = 0; i < data.buffer_views_count; ++i) {
    const auto &view = data.buffer_views[i];
    check(view.buffer && !view.has_meshopt_compression && !view.data,
          "Missing or compressed glTF buffer view");
    check(view.offset <= view.buffer->size &&
              view.size <= view.buffer->size - view.offset,
          "glTF buffer view exceeds its buffer");
  }
  for (std::size_t i = 0; i < data.accessors_count; ++i) {
    const auto &a = data.accessors[i];
    check(!a.is_sparse, "Sparse glTF accessors are not supported");
    check(a.buffer_view && a.count > 0 &&
              a.count <= std::max(props.maxVertices, props.maxIndices),
          "Missing or excessive glTF accessor storage");
    const auto size = cgltf_calc_size(a.type, a.component_type);
    const auto componentSize = cgltf_component_size(a.component_type);
    check(size && componentSize && a.stride >= size &&
              a.offset % componentSize == 0 && a.stride % componentSize == 0 &&
              a.buffer_view->offset % componentSize == 0,
          "Invalid glTF accessor element layout");
    check(a.offset <= a.buffer_view->size &&
              size <= a.buffer_view->size - a.offset,
          "glTF accessor exceeds buffer view");
    // Division-based check precedes cgltf_validate/accessor reads: cgltf 1.15
    // can overflow offset + stride * (count - 1) (upstream issue #301).
    check(a.count - 1 <= (a.buffer_view->size - a.offset - size) / a.stride,
          "glTF accessor stride/count exceeds buffer view");
  }
}

math::Transform3D nodeTransform(const cgltf_node &node, float units) {
  if (!node.has_matrix)
    return {{node.translation[0] * units, node.translation[1] * units,
             -node.translation[2] * units},
            {-node.rotation[0], -node.rotation[1], node.rotation[2],
             node.rotation[3]},
            {node.scale[0], node.scale[1], node.scale[2]}};
  math::Matrix4 matrix;
  std::copy_n(node.matrix, 16, matrix.elements.begin());
  const auto reflection = math::scaling({1, 1, -1});
  matrix = reflection * matrix * reflection;
  check(matrix.at(3, 0) == 0 && matrix.at(3, 1) == 0 && matrix.at(3, 2) == 0 &&
            matrix.at(3, 3) == 1,
        "glTF node matrix must be affine");
  math::Vec3f columns[3];
  float scales[3];
  for (int c = 0; c < 3; ++c) {
    columns[c] = {matrix.at(0, c), matrix.at(1, c), matrix.at(2, c)};
    scales[c] = float(std::hypot(double(columns[c].x), double(columns[c].y),
                                 double(columns[c].z)));
    check(std::isfinite(scales[c]) && scales[c] > 0,
          "Singular matrix-authored glTF nodes require explicit TRS");
    columns[c] = columns[c] * (1 / scales[c]);
  }
  if (math::dot(math::cross(columns[0], columns[1]), columns[2]) < 0) {
    scales[0] = -scales[0];
    columns[0] = columns[0] * -1;
  }
  math::Matrix4 r;
  for (int c = 0; c < 3; ++c) {
    r.at(0, c) = columns[c].x;
    r.at(1, c) = columns[c].y;
    r.at(2, c) = columns[c].z;
  }
  const float trace = r.at(0, 0) + r.at(1, 1) + r.at(2, 2);
  math::Quaternion q;
  if (trace > 0) {
    const float s = std::sqrt(trace + 1) * 2;
    q = {(r.at(2, 1) - r.at(1, 2)) / s, (r.at(0, 2) - r.at(2, 0)) / s,
         (r.at(1, 0) - r.at(0, 1)) / s, .25f * s};
  } else {
    int a = r.at(1, 1) > r.at(0, 0) ? 1 : 0;
    if (r.at(2, 2) > r.at(a, a))
      a = 2;
    const int b = (a + 1) % 3, c = (a + 2) % 3;
    const float s = std::sqrt(1 + r.at(a, a) - r.at(b, b) - r.at(c, c)) * 2;
    float v[3];
    v[a] = .25f * s;
    v[b] = (r.at(b, a) + r.at(a, b)) / s;
    v[c] = (r.at(c, a) + r.at(a, c)) / s;
    q = {v[0], v[1], v[2], (r.at(c, b) - r.at(b, c)) / s};
  }
  math::Transform3D result{{matrix.at(0, 3), matrix.at(1, 3), matrix.at(2, 3)},
                           math::normalizedRotation(q),
                           {scales[0], scales[1], scales[2]}};
  const auto rebuilt = result.matrix();
  for (std::size_t i = 0; i < 16; ++i)
    check(std::abs(rebuilt.elements[i] - matrix.elements[i]) <=
              1e-4f * std::max(1.f, std::abs(matrix.elements[i])),
          "Sheared/non-TRS glTF node matrices are unsupported");
  result.position = result.position * units;
  return result;
}

TextureAddress address(cgltf_wrap_mode mode) {
  switch (mode) {
  case cgltf_wrap_mode_clamp_to_edge:
    return TextureAddress::Clamp;
  case cgltf_wrap_mode_repeat:
    return TextureAddress::Repeat;
  case cgltf_wrap_mode_mirrored_repeat:
    return TextureAddress::MirroredRepeat;
  default:
    throw std::invalid_argument("Unsupported glTF texture address mode");
  }
}
} // namespace

ModelAsset::ModelAsset(std::vector<ModelNode> nodes,
                       std::vector<std::string> warnings)
    : _nodes{std::move(nodes)}, _warnings{std::move(warnings)} {
  for (std::size_t i = 0; i < _nodes.size(); ++i) {
    const auto &node = _nodes[i];
    node.transform.matrix();
    check(!node.parent || *node.parent < i, "Model parent must precede child");
    for (const auto &primitive : node.primitives)
      validate({{}, {1, 1}, {}},
               std::array{MeshDraw{primitive.mesh, primitive.material, {}}});
  }
}

ModelInstance ModelAsset::instantiate(Scene3D &scene,
                                      math::Transform3D transform,
                                      std::optional<ObjectId> parent) const {
  std::vector<ObjectProps> objects{{.transform = transform}};
  std::vector<std::optional<std::size_t>> parents{std::nullopt};
  std::vector<std::size_t> nodeIndices;
  for (const auto &node : _nodes) {
    const auto index = objects.size();
    objects.push_back({.transform = node.transform});
    parents.push_back(node.parent ? nodeIndices[*node.parent] : 0);
    nodeIndices.push_back(index);
    for (const auto &primitive : node.primitives) {
      objects.push_back(
          {.mesh = primitive.mesh, .material = primitive.material});
      parents.push_back(index);
    }
  }
  ModelInstance result;
  result.nodes.reserve(nodeIndices.size());
  const auto ids = scene.createBatch(objects, parents, parent);
  result.root = ids.front();
  for (const auto index : nodeIndices)
    result.nodes.push_back(ids[index]);
  return result;
}

ModelHandle importGLTF(std::span<const std::byte> document,
                       const ModelImportServices &services,
                       const ModelImportProps &props) {
  check(!document.empty() && document.size() <= props.maxDocumentBytes &&
            std::isfinite(props.unitsPerMeter) && props.unitsPerMeter > 0 &&
            props.maxResourceBytes && props.maxTotalResourceBytes &&
            props.maxNodes && props.maxVertices && props.maxIndices &&
            props.maxParserBytes,
        "Invalid model input/import limits");
  ParserBudget parserBudget{0, props.maxParserBytes};
  cgltf_options options{};
  options.memory = {allocateParser, freeParser, &parserBudget};
  cgltf_data *raw{};
  check(cgltf_parse(&options, document.data(), document.size(), &raw) ==
            cgltf_result_success,
        "Cannot parse glTF/GLB document");
  const std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data{raw,
                                                                cgltf_free};
  check(data->asset.version && std::string_view{data->asset.version} == "2.0",
        "Only glTF 2.0 is supported");
  check(!data->asset.min_version ||
            std::string_view{data->asset.min_version} == "2.0",
        "Model requires a newer glTF version");
  check(data->nodes_count <= props.maxNodes && !data->skins_count &&
            !data->animations_count,
        "Excessive nodes, skinning or animation is unsupported");
  for (std::size_t i = 0; i < data->extensions_required_count; ++i) {
    const std::string_view extension{data->extensions_required[i]};
    check(extension == "KHR_materials_unlit" ||
              extension == "KHR_texture_transform",
          "Unsupported required glTF extension");
  }
  std::size_t totalBytes{};
  auto account = [&](std::size_t bytes) {
    check(bytes <= props.maxResourceBytes &&
              bytes <= props.maxTotalResourceBytes - totalBytes,
          "Model resource budget exceeded");
    totalBytes += bytes;
  };
  auto readURI = [&](std::string_view uri) {
    if (uri.starts_with("data:"))
      return dataURI(uri, props.maxResourceBytes);
    check(bool(services.readResource),
          "External model resource needs a URI resolver");
    auto bytes = services.readResource(uri);
    check(bytes.size() <= props.maxResourceBytes,
          "External model resource exceeds budget");
    return bytes;
  };
  std::vector<std::vector<std::byte>> storage(data->buffers_count);
  for (std::size_t i = 0; i < data->buffers_count; ++i) {
    auto &buffer = data->buffers[i];
    account(buffer.size);
    if (buffer.uri) {
      storage[i] = readURI(buffer.uri);
      check(storage[i].size() == buffer.size,
            "glTF buffer byte length mismatch");
      buffer.data = storage[i].data();
    } else {
      check(i == 0 && data->bin && buffer.size <= data->bin_size,
            "Missing glTF binary buffer");
      buffer.data = const_cast<void *>(data->bin);
    }
    buffer.data_free_method = cgltf_data_free_method_none;
  }
  validateAccessors(*data, props);
  check(cgltf_validate(data.get()) == cgltf_result_success,
        "Invalid glTF structure");
  std::vector<std::string> warnings;
  std::unordered_map<const cgltf_image *, rendering::PaintImageHandle> images;
  auto image = [&](const cgltf_image *source) {
    check(source && bool(services.decodeImage),
          "Model texture needs an image decoder");
    if (const auto found = images.find(source); found != images.end())
      return found->second;
    std::vector<std::byte> bytes;
    std::span<const std::byte> encoded;
    if (source->uri) {
      bytes = readURI(source->uri);
      account(bytes.size());
      encoded = bytes;
    } else {
      check(source->buffer_view != nullptr, "Model image has no storage");
      const auto &view = *source->buffer_view;
      encoded = {static_cast<const std::byte *>(view.buffer->data) +
                     view.offset,
                 view.size};
    }
    auto decoded = services.decodeImage(
        encoded, source->mime_type ? source->mime_type : "");
    check(decoded &&
              decoded->colorEncoding() == rendering::ColorEncoding::SRGB &&
              decoded->alphaMode() == rendering::AlphaMode::Straight &&
              math::isFinite(decoded->pixelSize()) &&
              math::hasArea(decoded->pixelSize()),
          "glTF base-color decoder must return straight sRGB image");
    images.emplace(source, decoded);
    return decoded;
  };
  std::unordered_map<const cgltf_material *, MaterialProps> materials;
  auto material = [&](const cgltf_material *source) {
    if (!source)
      return MaterialProps{.doubleSided = false};
    if (const auto found = materials.find(source); found != materials.end())
      return found->second;
    MaterialProps result;
    result.doubleSided = source->double_sided;
    result.alphaCutoff = source->alpha_cutoff;
    if (source->alpha_mode == cgltf_alpha_mode_opaque)
      result.alpha = MaterialProps::Alpha::Opaque;
    else if (source->alpha_mode == cgltf_alpha_mode_mask)
      result.alpha = MaterialProps::Alpha::Mask;
    else if (source->alpha_mode == cgltf_alpha_mode_blend)
      result.alpha = MaterialProps::Alpha::Blend;
    else
      throw std::invalid_argument("Invalid glTF material alpha mode");
    if (!source->unlit)
      warnings.emplace_back("Lighting material imported as unlit base color");
    const auto &pbr = source->pbr_metallic_roughness;
    if (source->has_pbr_metallic_roughness)
      result.baseColor =
          math::toSRGB({pbr.base_color_factor[0], pbr.base_color_factor[1],
                        pbr.base_color_factor[2], pbr.base_color_factor[3]});
    if (const auto *texture = pbr.base_color_texture.texture) {
      check(!texture->has_basisu && !texture->has_webp,
            "Compressed glTF image extension unsupported");
      result.baseColorImage = image(texture->image);
      result.sampling = rendering::Sampling::Linear;
      result.addressU = result.addressV = TextureAddress::Repeat;
      if (const auto *sampler = texture->sampler) {
        result.addressU = address(sampler->wrap_s);
        result.addressV = address(sampler->wrap_t);
        const auto mag = sampler->mag_filter;
        const auto min = sampler->min_filter;
        check(min == cgltf_filter_type_undefined ||
                  min == cgltf_filter_type_nearest ||
                  min == cgltf_filter_type_linear ||
                  min == cgltf_filter_type_nearest_mipmap_nearest ||
                  min == cgltf_filter_type_linear_mipmap_nearest ||
                  min == cgltf_filter_type_nearest_mipmap_linear ||
                  min == cgltf_filter_type_linear_mipmap_linear,
              "Invalid glTF minification filter");
        check(mag == cgltf_filter_type_undefined ||
                  mag == cgltf_filter_type_nearest ||
                  mag == cgltf_filter_type_linear,
              "Invalid glTF magnification filter");
        result.sampling = mag == cgltf_filter_type_nearest
                              ? rendering::Sampling::Nearest
                              : rendering::Sampling::Linear;
        if (sampler->min_filter != cgltf_filter_type_undefined &&
            sampler->min_filter != mag)
          warnings.emplace_back("Texture minification/mipmap policy reduced to "
                                "the magnification filter");
      }
    }
    validate(result);
    materials.emplace(source, result);
    return result;
  };
  std::unordered_map<const cgltf_mesh *, std::vector<ModelPrimitive>> meshes;
  std::size_t totalVertices{}, totalIndices{};
  auto mesh = [&](const cgltf_mesh *source) {
    if (const auto found = meshes.find(source); found != meshes.end())
      return found->second;
    std::vector<ModelPrimitive> result;
    for (std::size_t p = 0; p < source->primitives_count; ++p) {
      const auto &primitive = source->primitives[p];
      check(primitive.type == cgltf_primitive_type_triangles &&
                !primitive.targets_count &&
                !primitive.has_draco_mesh_compression,
            "Only static, uncompressed glTF triangle primitives are supported");
      const auto *position =
          cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
      const auto *normal =
          cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
      check(position && position->type == cgltf_type_vec3 &&
                position->component_type == cgltf_component_type_r_32f &&
                !position->normalized,
            "glTF primitive needs float VEC3 positions");
      check(!normal ||
                (normal->type == cgltf_type_vec3 &&
                 normal->component_type == cgltf_component_type_r_32f &&
                 !normal->normalized && normal->count == position->count),
            "glTF normals must be matching float VEC3 values");
      check(!cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0),
            "Vertex color attributes are not implemented");
      check(
          !cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0) &&
              !cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0),
          "Skinning attributes are unsupported");
      if (const auto *indices = primitive.indices)
        check(indices->type == cgltf_type_scalar && !indices->normalized &&
                  (indices->component_type == cgltf_component_type_r_8u ||
                   indices->component_type == cgltf_component_type_r_16u ||
                   indices->component_type == cgltf_component_type_r_32u),
              "glTF indices must be unsigned integer scalars");
      const auto *textureView =
          primitive.material
              ? &primitive.material->pbr_metallic_roughness.base_color_texture
              : nullptr;
      int uvSet = textureView ? textureView->texcoord : 0;
      if (textureView && textureView->has_transform &&
          textureView->transform.has_texcoord)
        uvSet = textureView->transform.texcoord;
      const auto *uv =
          cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, uvSet);
      check(!uv ||
                (uv->type == cgltf_type_vec2 && uv->count == position->count &&
                 ((uv->component_type == cgltf_component_type_r_32f &&
                   !uv->normalized) ||
                  ((uv->component_type == cgltf_component_type_r_8u ||
                    uv->component_type == cgltf_component_type_r_16u) &&
                   uv->normalized))),
            "glTF UVs must be matching float/normalized unsigned VEC2 values");
      check(!textureView || !textureView->texture || uv,
            "Textured primitive lacks selected UVs");
      const auto indexCount =
          primitive.indices ? primitive.indices->count : position->count;
      check(position->count <= props.maxVertices - totalVertices &&
                indexCount <= props.maxIndices - totalIndices &&
                indexCount % 3 == 0 &&
                position->count <= std::numeric_limits<std::uint32_t>::max(),
            "Model geometry exceeds import limits or triangle topology");
      totalVertices += position->count;
      totalIndices += indexCount;
      MeshData geometry;
      geometry.vertices.resize(position->count);
      geometry.indices.resize(indexCount);
      for (std::size_t v = 0; v < position->count; ++v) {
        float xyz[3];
        check(cgltf_accessor_read_float(position, v, xyz, 3),
              "Cannot read glTF position");
        auto &vertex = geometry.vertices[v];
        vertex.position = {xyz[0] * props.unitsPerMeter,
                           xyz[1] * props.unitsPerMeter,
                           -xyz[2] * props.unitsPerMeter};
        if (normal) {
          check(cgltf_accessor_read_float(normal, v, xyz, 3),
                "Cannot read glTF normal");
          vertex.normal = {xyz[0], xyz[1], -xyz[2]};
        }
        if (uv) {
          float xy[2];
          check(cgltf_accessor_read_float(uv, v, xy, 2), "Cannot read glTF UV");
          vertex.uv = {xy[0], xy[1]};
          if (textureView && textureView->has_transform) {
            const auto &t = textureView->transform;
            const float x = xy[0] * t.scale[0], y = xy[1] * t.scale[1];
            vertex.uv = {t.offset[0] + std::cos(t.rotation) * x -
                             std::sin(t.rotation) * y,
                         t.offset[1] + std::sin(t.rotation) * x +
                             std::cos(t.rotation) * y};
          }
        }
      }
      for (std::size_t i = 0; i < indexCount; ++i) {
        const auto index = primitive.indices
                               ? cgltf_accessor_read_index(primitive.indices, i)
                               : i;
        check(index < geometry.vertices.size(), "glTF index exceeds vertices");
        geometry.indices[i] = static_cast<std::uint32_t>(index);
      }
      for (std::size_t i = 0; i < indexCount; i += 3)
        std::swap(geometry.indices[i + 1], geometry.indices[i + 2]);
      result.push_back(
          {makeMesh(std::move(geometry)), material(primitive.material)});
    }
    meshes.emplace(source, result);
    return result;
  };
  const cgltf_scene *selected = data->scene;
  if (props.sceneIndex) {
    check(*props.sceneIndex < data->scenes_count,
          "glTF scene index out of range");
    selected = &data->scenes[*props.sceneIndex];
  }
  if (!selected && data->scenes_count)
    selected = &data->scenes[0];
  struct Pending {
    const cgltf_node *node;
    std::optional<std::size_t> parent;
  };
  std::vector<Pending> pending;
  if (selected) {
    for (std::size_t i = selected->nodes_count; i > 0; --i)
      pending.push_back({selected->nodes[i - 1], {}});
  } else {
    for (std::size_t i = data->nodes_count; i > 0; --i)
      if (!data->nodes[i - 1].parent)
        pending.push_back({&data->nodes[i - 1], {}});
  }
  std::vector<bool> visited(data->nodes_count);
  std::vector<ModelNode> nodes;
  while (!pending.empty()) {
    const auto [source, parent] = pending.back();
    pending.pop_back();
    const auto original = std::size_t(source - data->nodes);
    check(original < visited.size() && !visited[original],
          "Repeated/cyclic glTF node");
    visited[original] = true;
    check(!source->skin && !source->weights_count &&
              !source->has_mesh_gpu_instancing,
          "Skinned/morphed/instanced glTF nodes are unsupported");
    if (source->camera || source->light)
      warnings.emplace_back(
          "Imported node retains transform but not camera/light attachment");
    ModelNode node{source->name ? source->name : "",
                   nodeTransform(*source, props.unitsPerMeter), parent};
    if (source->mesh)
      node.primitives = mesh(source->mesh);
    const auto index = nodes.size();
    nodes.push_back(std::move(node));
    for (std::size_t i = source->children_count; i > 0; --i)
      pending.push_back({source->children[i - 1], index});
  }
  return std::make_shared<const ModelAsset>(std::move(nodes),
                                            std::move(warnings));
}

} // namespace playground::scene
