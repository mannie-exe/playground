#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <unordered_map>
#include <utility>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <math/ColorSpace.hpp>
#include <scene/ModelImport.hpp>

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
                       std::vector<std::string> warnings,
                       std::vector<AnimationClip> clips)
    : _nodes{std::move(nodes)}, _warnings{std::move(warnings)},
      _clips{std::move(clips)} {
  for (const auto &clip : _clips)
    clip.validate(_nodes.size());
  for (std::size_t i = 0; i < _nodes.size(); ++i) {
    const auto &node = _nodes[i];
    node.transform.matrix();
    check(!node.parent || *node.parent < i, "Model parent must precede child");
    for (const auto &primitive : node.primitives)
      validate({{}, {1, 1}, {}},
               std::array{MeshDraw{primitive.mesh, primitive.material, {}}});
  }
}

void ModelAsset::applyAnimation(Scene3D &scene, const ModelInstance &instance,
                                std::size_t clipIndex,
                                const Playback &playback) const {
  const auto &clip = _clips.at(clipIndex);
  if (instance.modelIdentity != _identity ||
      instance.nodes.size() != _nodes.size() || !scene.contains(instance.root))
    throw std::invalid_argument("Animation instance is stale or incompatible");
  std::vector<math::Transform3D> poses;
  poses.reserve(_nodes.size());
  for (std::size_t i = 0; i < _nodes.size(); ++i) {
    if (!scene.contains(instance.nodes[i]))
      throw std::invalid_argument("Animation target is stale");
    poses.push_back(_nodes[i].transform);
  }
  const auto time = playback.sampleTime(clip.duration);
  for (const auto &track : clip.tracks) {
    auto &pose = poses[track.node];
    const auto value = track.sample(time);
    switch (track.path) {
    case TrackPath::Translation:
      pose.position = {value.x, value.y, value.z};
      break;
    case TrackPath::Scale:
      pose.scale = {value.x, value.y, value.z};
      break;
    case TrackPath::Rotation:
      pose.orientation = {value.x, value.y, value.z, value.w};
      break;
    }
  }
  for (const auto &pose : poses)
    pose.matrix();
  for (std::size_t i = 0; i < poses.size(); ++i)
    scene.applyPatch(instance.nodes[i], {.transform = poses[i]});
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
  result.modelIdentity = _identity;
  result.nodes.reserve(nodeIndices.size());
  const auto ids = scene.createBatch(objects, parents, parent);
  result.root = ids.front();
  for (const auto index : nodeIndices)
    result.nodes.push_back(ids[index]);
  return result;
}

void ModelImportProps::validate() const {
  check(maxDocumentBytes && std::isfinite(unitsPerMeter) && unitsPerMeter > 0 &&
            maxResourceBytes && maxTotalResourceBytes && maxNodes &&
            maxVertices && maxIndices && maxParserBytes &&
            maxPreparationBytes && maxAnimationClips && maxAnimationKeys,
        "Invalid model import limits");
}

ModelHandle importGLTF(std::span<const std::byte> document,
                       const ModelImportServices &services,
                       const ModelImportProps &props) {
  props.validate();
  PreparationBudget admission{services.resources};
  check(!document.empty() && document.size() <= props.maxDocumentBytes,
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
  check(data->nodes_count <= props.maxNodes &&
            data->animations_count <= props.maxAnimationClips &&
            !data->skins_count,
        "Excessive nodes or skinning is unsupported");
  std::vector<std::string> warnings;
  for (std::size_t i = 0; i < data->extensions_required_count; ++i) {
    const std::string_view extension{data->extensions_required[i]};
    const bool supported = extension == "KHR_materials_unlit" ||
                           extension == "KHR_texture_transform" ||
                           extension == "KHR_texture_basisu";
    const bool materialFallback =
        extension == "KHR_materials_clearcoat" ||
        extension == "KHR_materials_transmission" ||
        extension == "KHR_materials_volume" ||
        extension == "KHR_materials_ior" ||
        extension == "KHR_materials_specular" ||
        extension == "KHR_materials_sheen" ||
        extension == "KHR_materials_emissive_strength" ||
        extension == "KHR_materials_iridescence" ||
        extension == "KHR_materials_diffuse_transmission" ||
        extension == "KHR_materials_anisotropy" ||
        extension == "KHR_materials_dispersion" ||
        extension == "KHR_materials_pbrSpecularGlossiness";
    if (!supported) {
      check(props.allowMaterialFallback && materialFallback,
            "Unsupported required glTF extension");
      warnings.emplace_back(
          "Required material extension " + std::string{extension} +
          " ignored by explicit core-material preview policy");
    }
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
  std::unordered_map<const cgltf_material *, MaterialProps> materials;
  std::map<std::pair<const cgltf_image *, rendering::TextureRole>,
           rendering::TextureHandle>
      textures;
  auto binding = [&](const cgltf_texture_view &view,
                     rendering::TextureRole role) {
    rendering::TextureBinding result;
    if (!view.texture)
      return result;
    const auto *texture = view.texture;
    const auto *image =
        texture->has_basisu ? texture->basisu_image : texture->image;
    check(!texture->has_webp && image, "Unsupported/missing texture image");
    check(bool(services.decodeTexture),
          "Model material needs a numerical texture decoder");
    const auto key = std::pair{image, role};
    if (auto found = textures.find(key); found != textures.end())
      result.texture = found->second;
    else {
      const auto *source = image;
      std::vector<std::byte> bytes;
      std::span<const std::byte> encoded;
      if (source->uri) {
        bytes = readURI(source->uri);
        account(bytes.size());
        encoded = bytes;
      } else {
        check(source->buffer_view != nullptr, "Texture has no storage");
        const auto &v = *source->buffer_view;
        encoded = {static_cast<const std::byte *>(v.buffer->data) + v.offset,
                   v.size};
      }
      result.texture = services.decodeTexture(
          encoded, source->mime_type ? source->mime_type : "", role);
      check(result.texture && result.texture->role() == role,
            "Texture decoder returned wrong interpretation");
      account(result.texture->bytes());
      textures.emplace(key, result.texture);
    }
    int uvSet = view.has_transform && view.transform.has_texcoord
                    ? view.transform.texcoord
                    : view.texcoord;
    check(uvSet >= 0 && uvSet <= 1, "Only TEXCOORD_0 and TEXCOORD_1 supported");
    result.uvSet = unsigned(uvSet);
    if (view.has_transform)
      result.transform = {{view.transform.offset[0], view.transform.offset[1]},
                          {view.transform.scale[0], view.transform.scale[1]},
                          view.transform.rotation};
    if (const auto *s = texture->sampler) {
      result.sampler.addressU = address(s->wrap_s);
      result.sampler.addressV = address(s->wrap_t);
      check(s->mag_filter == cgltf_filter_type_undefined ||
                s->mag_filter == cgltf_filter_type_linear ||
                s->mag_filter == cgltf_filter_type_nearest,
            "Invalid magnification filter");
      result.sampler.magnification = s->mag_filter == cgltf_filter_type_nearest
                                         ? rendering::Sampling::Nearest
                                         : rendering::Sampling::Linear;
      switch (s->min_filter) {
      case cgltf_filter_type_nearest:
        result.sampler.minification = rendering::Sampling::Nearest;
        result.sampler.mip = rendering::MipFilter::None;
        break;
      case cgltf_filter_type_linear:
        result.sampler.mip = rendering::MipFilter::None;
        break;
      case cgltf_filter_type_nearest_mipmap_nearest:
        result.sampler.minification = rendering::Sampling::Nearest;
        result.sampler.mip = rendering::MipFilter::Nearest;
        break;
      case cgltf_filter_type_linear_mipmap_nearest:
        result.sampler.mip = rendering::MipFilter::Nearest;
        break;
      case cgltf_filter_type_nearest_mipmap_linear:
        result.sampler.minification = rendering::Sampling::Nearest;
        break;
      case cgltf_filter_type_undefined:
      case cgltf_filter_type_linear_mipmap_linear:
        break;
      default:
        throw std::invalid_argument("Invalid minification filter");
      }
    }
    result.validate();
    return result;
  };
  auto material = [&](const cgltf_material *source) {
    if (!source)
      return MaterialProps{.doubleSided = false,
                           .pbr = MetallicRoughnessProps{}};
    if (const auto found = materials.find(source); found != materials.end())
      return found->second;
    if (source->has_clearcoat || source->has_transmission ||
        source->has_volume || source->has_ior || source->has_specular ||
        source->has_sheen || source->has_emissive_strength ||
        source->has_iridescence || source->has_diffuse_transmission ||
        source->has_anisotropy || source->has_dispersion ||
        source->has_pbr_specular_glossiness)
      warnings.emplace_back("Optional extended material properties ignored; "
                            "using core metallic-roughness fallback");
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
    const auto &pbr = source->pbr_metallic_roughness;
    if (!source->unlit) {
      MetallicRoughnessProps p;
      p.baseColor = {pbr.base_color_factor[0], pbr.base_color_factor[1],
                     pbr.base_color_factor[2], pbr.base_color_factor[3]};
      p.metallic = pbr.metallic_factor;
      p.roughness = pbr.roughness_factor;
      p.emissive = {source->emissive_factor[0], source->emissive_factor[1],
                    source->emissive_factor[2]};
      p.normalScale = source->normal_texture.scale;
      p.occlusionStrength = source->occlusion_texture.scale;
      p.baseColorTexture =
          binding(pbr.base_color_texture, rendering::TextureRole::Color);
      p.metallicRoughnessTexture =
          binding(pbr.metallic_roughness_texture, rendering::TextureRole::Data);
      p.normalTexture =
          binding(source->normal_texture, rendering::TextureRole::Normal);
      p.occlusionTexture =
          binding(source->occlusion_texture, rendering::TextureRole::Data);
      p.emissiveTexture =
          binding(source->emissive_texture, rendering::TextureRole::Emission);
      if (result.alpha == MaterialProps::Alpha::Mask &&
          p.baseColorTexture.texture && p.baseColor.w > 0 &&
          result.alphaCutoff / p.baseColor.w <= 1) {
        account(p.baseColorTexture.texture->bytes());
        p.baseColorTexture.texture = rendering::preserveAlphaCoverage(
            p.baseColorTexture.texture, result.alphaCutoff / p.baseColor.w);
      }
      result.pbr = std::move(p);
      validate(result);
      materials.emplace(source, result);
      return result;
    }
    result.colorTexture =
        binding(pbr.base_color_texture, rendering::TextureRole::Color);
    result.baseColor =
        math::toSRGB({pbr.base_color_factor[0], pbr.base_color_factor[1],
                      pbr.base_color_factor[2], pbr.base_color_factor[3]});
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
      const auto *color =
          cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0);
      const auto *tangent =
          cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0);
      const auto normalizedFloat = [](const cgltf_accessor *a) {
        return (a->component_type == cgltf_component_type_r_32f &&
                !a->normalized) ||
               ((a->component_type == cgltf_component_type_r_8u ||
                 a->component_type == cgltf_component_type_r_16u) &&
                a->normalized);
      };
      check(!color ||
                ((color->type == cgltf_type_vec3 ||
                  color->type == cgltf_type_vec4) &&
                 color->count == position->count && normalizedFloat(color)),
            "Invalid vertex color accessor");
      check(!tangent ||
                (tangent->type == cgltf_type_vec4 &&
                 tangent->count == position->count &&
                 tangent->component_type == cgltf_component_type_r_32f &&
                 !tangent->normalized),
            "Invalid tangent accessor");
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
      int uvSet = 0;
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
          vertex.normal = math::normalized({xyz[0], xyz[1], -xyz[2]});
        }
        if (uv) {
          float xy[2];
          check(cgltf_accessor_read_float(uv, v, xy, 2), "Cannot read glTF UV");
          vertex.uv = {xy[0], xy[1]};
        }
        if (const auto *uv1 = cgltf_find_accessor(
                &primitive, cgltf_attribute_type_texcoord, 1)) {
          check(uv1->type == cgltf_type_vec2 && uv1->count == position->count &&
                    normalizedFloat(uv1),
                "Invalid second UV set");
          float xy[2];
          check(cgltf_accessor_read_float(uv1, v, xy, 2),
                "Cannot read second UV set");
          vertex.uv1 = {xy[0], xy[1]};
        }
        if (color) {
          float rgba[4]{1, 1, 1, 1};
          check(cgltf_accessor_read_float(color, v, rgba, 4),
                "Cannot read vertex color");
          if (color->type == cgltf_type_vec3)
            rgba[3] = 1;
          for (float c : rgba)
            check(std::isfinite(c) && c >= 0 && c <= 1,
                  "Vertex color must be normalized");
          vertex.color = {rgba[0], rgba[1], rgba[2], rgba[3]};
        }
        if (tangent) {
          float t[4];
          check(cgltf_accessor_read_float(tangent, v, t, 4),
                "Cannot read tangent");
          check(t[3] == 1 || t[3] == -1, "Invalid tangent handedness");
          const auto n = math::normalized({t[0], t[1], -t[2]});
          vertex.tangent = {n.x, n.y, n.z, -t[3]};
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
      auto mat = material(primitive.material);
      auto requireUV = [&](const rendering::TextureBinding &b) {
        check(!b.texture ||
                  cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord,
                                      int(b.uvSet)),
              "Missing texture-selected UV set");
      };
      requireUV(mat.colorTexture);
      if (mat.pbr) {
        const auto &p = *mat.pbr;
        requireUV(p.baseColorTexture);
        requireUV(p.normalTexture);
        requireUV(p.metallicRoughnessTexture);
        requireUV(p.occlusionTexture);
        requireUV(p.emissiveTexture);
      }
      const auto prepared = prepareMesh(
          geometry, {.generateNormals = !normal,
                     .generateTangents = !tangent && mat.pbr &&
                                         bool(mat.pbr->normalTexture.texture),
                     .tangentUVSet = mat.pbr ? mat.pbr->normalTexture.uvSet : 0,
                     .maxScratchBytes = props.maxPreparationBytes,
                     .admission = &admission});
      account(prepared.finalBytes);
      totalVertices -= position->count;
      check(geometry.vertices.size() <= props.maxVertices - totalVertices,
            "Generated mesh exceeds vertex budget");
      totalVertices += geometry.vertices.size();
      result.push_back(
          {makeMesh(std::move(geometry), services.resources), std::move(mat)});
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
  std::vector<std::optional<std::size_t>> nodeMap(data->nodes_count);
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
    nodeMap[original] = index;
    nodes.push_back(std::move(node));
    for (std::size_t i = source->children_count; i > 0; --i)
      pending.push_back({source->children[i - 1], index});
  }
  std::vector<AnimationClip> clips;
  std::size_t totalKeys{};
  for (std::size_t a = 0; a < data->animations_count; ++a) {
    const auto &source = data->animations[a];
    AnimationClip clip{source.name ? source.name : ""};
    for (std::size_t c = 0; c < source.channels_count; ++c) {
      const auto &channel = source.channels[c];
      check(channel.target_node && channel.sampler,
            "Missing animation target/sampler");
      const auto mapped =
          nodeMap[std::size_t(channel.target_node - data->nodes)];
      if (!mapped)
        continue;
      check(!channel.target_node->has_matrix, "Animated nodes must use TRS");
      TransformTrack track;
      track.node = *mapped;
      switch (channel.target_path) {
      case cgltf_animation_path_type_translation:
        track.path = TrackPath::Translation;
        break;
      case cgltf_animation_path_type_rotation:
        track.path = TrackPath::Rotation;
        break;
      case cgltf_animation_path_type_scale:
        track.path = TrackPath::Scale;
        break;
      default:
        throw std::invalid_argument("Morph animation unsupported");
      }
      const auto &s = *channel.sampler;
      switch (s.interpolation) {
      case cgltf_interpolation_type_step:
        track.interpolation = TrackInterpolation::Step;
        break;
      case cgltf_interpolation_type_linear:
        track.interpolation = TrackInterpolation::Linear;
        break;
      case cgltf_interpolation_type_cubic_spline:
        track.interpolation = TrackInterpolation::CubicSpline;
        break;
      default:
        throw std::invalid_argument("Invalid animation interpolation");
      }
      check(s.input && s.output && s.input->type == cgltf_type_scalar &&
                s.input->component_type == cgltf_component_type_r_32f &&
                s.output->component_type == cgltf_component_type_r_32f &&
                s.output->type == (track.path == TrackPath::Rotation
                                       ? cgltf_type_vec4
                                       : cgltf_type_vec3),
            "Invalid animation accessor types");
      check(!s.input->normalized && !s.output->normalized &&
                s.input->count <= props.maxAnimationKeys - totalKeys,
            "Animation key budget exceeded or normalized float accessor");
      totalKeys += s.input->count;
      const auto multiplier =
          track.interpolation == TrackInterpolation::CubicSpline ? 3u : 1u;
      check(s.output->count / multiplier == s.input->count &&
                s.output->count % multiplier == 0,
            "Animation output count does not match interpolation");
      check(s.input->count <= props.maxResourceBytes / sizeof(float) &&
                s.output->count <= props.maxResourceBytes / sizeof(math::Vec4f),
            "Animation allocation exceeds limit");
      account(s.input->count * sizeof(float));
      account(s.output->count * sizeof(math::Vec4f));
      for (std::size_t k = 0; k < s.input->count; ++k) {
        float time;
        check(cgltf_accessor_read_float(s.input, k, &time, 1),
              "Cannot read animation time");
        track.times.push_back(time);
      }
      for (std::size_t k = 0; k < s.output->count; ++k) {
        float v[4]{};
        check(cgltf_accessor_read_float(s.output, k, v, 4),
              "Cannot read animation key");
        if (track.path == TrackPath::Translation) {
          v[0] *= props.unitsPerMeter;
          v[1] *= props.unitsPerMeter;
          v[2] *= -props.unitsPerMeter;
        }
        if (track.path == TrackPath::Rotation) {
          v[0] = -v[0];
          v[1] = -v[1];
        }
        track.values.push_back({v[0], v[1], v[2], v[3]});
      }
      track.validate(nodes.size());
      clip.duration = std::max(clip.duration, double(track.times.back()));
      clip.tracks.push_back(std::move(track));
    }
    clip.validate(nodes.size());
    clips.push_back(std::move(clip));
  }
  return std::make_shared<const ModelAsset>(
      std::move(nodes), std::move(warnings), std::move(clips));
}

} // namespace playground::scene
