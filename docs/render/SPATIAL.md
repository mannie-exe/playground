# Spatial presentation and selection

[Spatial products](../platform/SPATIAL_PRODUCTS.md) prepare immutable CPU products;
renderers realize them through existing mesh/texture ownership and frame admission.
Dataset content, semantic interpretation, placement and native resource identity
remain separate. This contract includes baseline voxel surfaces and scalar/label
slice presentation, not a requirement to add a new general rendering backend.

## Recipes and state

| API/value | Shape and contract |
|---|---|
| `VoxelSurfaceProfile` | Occupancy/label channels, interpretation, exposed-face/material rules, neighbor policy and output limits |
| `VoxelSurfaceRecipe` | Prepare a bounded immutable local-space Mesh product with surface/profile references |
| `VolumePresentationProfile` | Selected channels, bounded slice plane/resolution, sampling mode and versioned value-to-color mapping |
| `VolumePresentationRecipe` | Prepare immutable slice image/geometry with explicit data/color encoding |
| `SpatialPresentation` | Dataset instance, product leases, world/local placement, visibility and FreshnessPolicy |
| `SpatialQueryProfile` | Selection channels, hit predicate, supported ray/overlap operations and traversal limits |
| `SpatialQueryAdapter` | Versioned bounded sample-space selection independent of render mesh/GPU readback |
| `SpatialHit` | VoxelSelection, distance in declared units, coverage/completeness and represented source/product versions |

The baseline surface recipe removes internal block faces and merges compatible
coplanar faces only when their complete appearance attributes permit it. Preserve
material boundaries, winding, sample support and deterministic output ordering.
Unknown neighbors use a declared TemporaryBoundaryFaces or Wait policy; temporary
geometry records absent-neighbor dependencies. Known empty output is successful;
unavailable coverage is not an empty successful mesh.

Generate the intended immutable upload representation once per product version.
Do not route every block vertex through multiple unrelated intermediate formats.
Reserve worst-case candidate output or use a bounded builder that refuses overflow
without partial publication. Fragmented/checkerboard data must fit explicit bounds
or fail; a small chunk count is no guarantee of small geometry.

The baseline volume recipe visualizes bounded slices using existing image/mesh
paths. Integer labels use nearest sampling; continuous channels permit declared
interpolation. Transfer mapping is a versioned presentation input, not a mutation
to the scalar field or its physical meaning. Output carries color-space and alpha
association; numerical source values never receive accidental sRGB conversion.
Other representations report Unsupported unless a concrete recipe/backend supplies
them. The interface does not imply smooth isosurfaces, ray marching or transparency
features beyond the renderer's declared capabilities.

## Scene and renderer integration

World placement uses DatasetBinding and consistent frame samples. Dataset-only
inspection can use local coordinates; world views use camera-relative float
conversion at extraction. Rigid placement changes update transforms, not meshes.
Anisotropic grid spacing is baked into local surface positions and correct normals;
adapter capabilities validate subsequent transforms independently.

Native realization is bounded by missing-resource plans, upload bytes and frame
credits. The render callback never waits for generation or edits. Successful
product publication invalidates the appropriate retained scene/UI view and requests
paint; unchanged products keep stable handles across frames and camera motion.
An idle view must not rebuild/upload data. Backend replacement recreates device
realizations while retaining compatible CPU products.

Stale presentation carries its actual revision and age. A consumer can show the
previous complete product while replacement runs, within AllowPrevious policy.
Expired/missing presentation produces the caller's explicit placeholder or wait
state. Failure preserves the prior binding where allowed; it never reports the
new geometry as rendered. Multi-chunk coherent presentation uses a captured root
and a declared publication group when mixed visual revisions are unacceptable.

Existing unlit preview and PBR paths keep their capability requirements. Unsupported
physical/render properties cannot be inferred from sample labels. Color, opacity,
collision, navigation area and sound category are separately authored mappings;
see [voxel interpretation](../platform/VOXELS.md#values-and-interpretation).

## Queries and interaction

Baseline selection traverses a regular grid for exact occupied/labeled cell hits,
with bounded visited samples, finite rays and checked index conversion. Hit
predicates come from registered interpretation/profile code, not serialized native
callbacks. Boundary/tie ordering and ray-start-inside behavior are deterministic:
report the starting matching cell at distance zero; simultaneous crossings advance
all tied axes before testing the next cell. Supercover queries are a separate
explicit capability, not silently substituted for this convention.

Return Complete, Miss, NeedsData, Partial, Unsupported or BudgetExceeded as
appropriate. Miss requires complete relevant coverage. Results include exact
sample/channel identity and the source/placement revisions, even when the visible
mesh is older. Apps choose current-data selection or a retained displayed-generation
selection explicitly. Convert hit positions/normals through the sampled placement;
stale placement cannot authorize a world-space edit without revalidation.

Inspection controls, UI tools and gameplay consume the same selection result.
UI ownership/capture follows existing input contracts; selecting a sample does not
implicitly capture the pointer or switch camera modes. Brushes submit bounded
SpatialEdit commands. Scene graphics never become the writable dataset authority.

## Verification

[Spatial workloads](../platform/SPATIAL_TESTING.md) check exact grid hits, topology,
seams, material boundaries, anisotropic placement, negative/far origins, stale
selection conflicts, idle reuse and fragmented output limits. Software images and
native GPU readback supplement data/product assertions. Unsupported native devices
are skipped coverage, not a pass. New shaders or advanced lighting are not needed
to establish these source/edit/publication contracts.
