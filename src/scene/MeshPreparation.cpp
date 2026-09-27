#include <algorithm>
#include <array>
#include <climits>
#include <limits>
#include <stdexcept>
#include <vector>

#include <meshoptimizer.h>
#include <mikktspace.h>

#include <scene/SceneRenderer.hpp>

namespace playground::scene {
namespace {
void split(MeshData &mesh) {
  mesh.validate();
  std::vector<Vertex3D> vertices;
  vertices.reserve(mesh.indices.size());
  for (auto i : mesh.indices)
    vertices.push_back(mesh.vertices[i]);
  mesh.vertices = std::move(vertices);
  for (std::size_t i = 0; i < mesh.indices.size(); ++i)
    mesh.indices[i] = static_cast<std::uint32_t>(i);
}

struct Tangents {
  MeshData &mesh;
  unsigned uvSet;
};

const Vertex3D &vertex(const SMikkTSpaceContext *ctx, int face, int corner) {
  auto &m = static_cast<Tangents *>(ctx->m_pUserData)->mesh;
  return m.vertices[std::size_t(face) * 3 + corner];
}

void normals(MeshData &mesh) {
  split(mesh);
  for (std::size_t i = 0; i < mesh.vertices.size(); i += 3) {
    const auto a = mesh.vertices[i].position, b = mesh.vertices[i + 1].position,
               c = mesh.vertices[i + 2].position;
    auto n = math::cross(b - a, c - a);
    n = math::dot(n, n) > 1e-16f ? math::normalized(n) : math::Vec3f{0, 0, 1};
    for (int j = 0; j < 3; ++j)
      mesh.vertices[i + j].normal = n;
  }
}

void tangents(MeshData &mesh, unsigned uvSet) {
  if (uvSet > 1 || mesh.indices.size() / 3 > static_cast<std::size_t>(INT_MAX))
    throw std::invalid_argument("Invalid tangent generation input");
  split(mesh);
  Tangents data{mesh, uvSet};
  SMikkTSpaceInterface api{};
  api.m_getNumFaces = [](const SMikkTSpaceContext *c) {
    return int(static_cast<Tangents *>(c->m_pUserData)->mesh.indices.size() /
               3);
  };
  api.m_getNumVerticesOfFace = [](const SMikkTSpaceContext *, int) {
    return 3;
  };
  api.m_getPosition = [](const SMikkTSpaceContext *c, float *p, int f, int v) {
    const auto x = vertex(c, f, v).position;
    p[0] = x.x;
    p[1] = x.y;
    p[2] = x.z;
  };
  api.m_getNormal = [](const SMikkTSpaceContext *c, float *p, int f, int v) {
    const auto x = vertex(c, f, v).normal;
    p[0] = x.x;
    p[1] = x.y;
    p[2] = x.z;
  };
  api.m_getTexCoord = [](const SMikkTSpaceContext *c, float *p, int f, int v) {
    const auto &x = vertex(c, f, v);
    const auto uv =
        static_cast<Tangents *>(c->m_pUserData)->uvSet ? x.uv1 : x.uv;
    p[0] = uv.x;
    p[1] = uv.y;
  };
  api.m_setTSpaceBasic = [](const SMikkTSpaceContext *c, const float *p,
                            float sign, int f, int v) {
    static_cast<Tangents *>(c->m_pUserData)
        ->mesh.vertices[std::size_t(f) * 3 + v]
        .tangent = {p[0], p[1], p[2], sign};
  };
  SMikkTSpaceContext context{&api, &data};
  if (!genTangSpaceDefault(&context))
    throw std::runtime_error("MikkTSpace tangent generation failed");
}

void reindex(MeshData &mesh) {
  const auto &v = mesh.vertices.front();
  const std::array<meshopt_Stream, 6> streams{
      {{&v.position, sizeof(v.position), sizeof(Vertex3D)},
       {&v.normal, sizeof(v.normal), sizeof(Vertex3D)},
       {&v.uv, sizeof(v.uv), sizeof(Vertex3D)},
       {&v.tangent, sizeof(v.tangent), sizeof(Vertex3D)},
       {&v.color, sizeof(v.color), sizeof(Vertex3D)},
       {&v.uv1, sizeof(v.uv1), sizeof(Vertex3D)}}};
  std::vector<unsigned> remap(mesh.vertices.size());
  const auto count = meshopt_generateVertexRemapMulti(
      remap.data(), mesh.indices.data(), mesh.indices.size(),
      mesh.vertices.size(), streams.data(), streams.size());
  std::vector<Vertex3D> vertices(count);
  meshopt_remapVertexBuffer(vertices.data(), mesh.vertices.data(),
                            mesh.vertices.size(), sizeof(Vertex3D),
                            remap.data());
  meshopt_remapIndexBuffer(mesh.indices.data(), mesh.indices.data(),
                           mesh.indices.size(), remap.data());
  mesh.vertices = std::move(vertices);
}
} // namespace

MeshPreparationStats prepareMesh(MeshData &mesh, MeshPrepareProps props) {
  mesh.validate();
  if (props.tangentUVSet > 1 || !props.maxScratchBytes ||
      mesh.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
      mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max())
    throw std::invalid_argument("Invalid mesh preparation policy");
  const auto corners = mesh.indices.size();
  const auto workingVertices = (props.generateNormals || props.generateTangents)
                                   ? std::max(corners, mesh.vertices.size())
                                   : mesh.vertices.size();
  // Candidate, corner expansion, remap/output and dependency working storage.
  constexpr auto vertexAllowance = 4 * sizeof(Vertex3D) + 64;
  if (workingVertices > props.maxScratchBytes / vertexAllowance ||
      corners >
          (props.maxScratchBytes - workingVertices * vertexAllowance) / 16)
    throw std::length_error("Mesh preparation exceeds scratch budget");
  MeshPreparationStats stats{mesh.vertices.size(),
                             corners,
                             0,
                             mesh.vertices.size() * sizeof(Vertex3D) +
                                 corners * sizeof(std::uint32_t),
                             0,
                             workingVertices * vertexAllowance + corners * 16};
  const auto lease =
      (props.admission ? *props.admission : resourcePreparationBudget())
          .acquire(stats.scratchEstimateBytes);
  auto candidate = mesh;
  if (props.generateNormals)
    normals(candidate);
  if (props.generateTangents)
    tangents(candidate, props.tangentUVSet);
  reindex(candidate);
  candidate.validate();
  stats.finalVertices = candidate.vertices.size();
  stats.finalBytes = candidate.vertices.size() * sizeof(Vertex3D) +
                     candidate.indices.size() * sizeof(std::uint32_t);
  mesh = std::move(candidate);
  return stats;
}

void generateNormals(MeshData &mesh) {
  prepareMesh(mesh, {.generateNormals = true});
}

void generateTangents(MeshData &mesh, unsigned uvSet) {
  prepareMesh(mesh, {.generateTangents = true, .tangentUVSet = uvSet});
}
} // namespace playground::scene
