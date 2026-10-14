# Scalar volume inspection application

`playground_voxels` exercises bounded sampled data, independently resident products
and persistent label edits. Medical workflow, intensity interpretation and brush
semantics belong to the application. No ECS, clinical workflow framework or generic
editor workspace is required to construct the initial viewer.

## Data and presentation

The [volume fixture suite](../../assets/volumes/README.md) supplies pinned MRHead and
CTChest sources and a Python reference importer/reader. Its independent 32-cubed
NRRD bricks allow both small-cache churn and cold-read tests. Packaging and Python
reconstruction validate source conversion, not native engine streaming or GPU
performance. The dataset manifest retains axis vectors, origin, scalar type, shape
and declared units; the fixtures' millimetre units are explicit import assumptions.

Use three orthogonal slices, one shared physical crosshair, window/level controls
and an overview viewport. `VolumeViewState` carries dataset identity/epoch, physical
crosshair and active scalar/label channel. Each panel owns its plane, pan/zoom,
output resolution and product demand. Intensity remains signed/source-typed and
independent of color mapping. A missing brick is unavailable coverage, never zero
intensity. Dataset replacement invalidates pending slice and edit results.

The physical-to-index transform determines sampling and picking, including negative
axis directions and anisotropic spacing. Out-of-bounds crosshairs clamp or reject
according to an explicit view policy. Every panel uses the same policy. Product
keys include source/channel revisions, sampling plane and color-map parameters;
reuse raw samples independently when only presentation changes.

The first viewer's planes follow the three orthogonal **grid axes** from GridMapping,
including rotated/reflected grids; each plane fixes one index coordinate and spans
the other two. They are not fixed world/anatomical-axis planes. Use the mapping to
place those planes in physical space and label their orientation. Select the nearest
sample plane and nearest labels; scalar linear interpolation may be
an explicitly selected product capability. Window width must be finite and positive;
the initial linear map is `clamp((value - (level - width/2)) / width, 0, 1)`.
Apply it after source-type decoding; NaN/missing coverage gets a diagnostic mask.
This is an inspection transfer function, not a claim of DICOM presentation parity.
Pan uses physical in-plane displacement; zoom preserves the physical point under
the cursor. The overview shows dataset bounds and the three selected planes using
existing scene primitives; it does not require volume ray marching. Golden samples
cover signed intensity, rotated/reflected grids, anisotropic spacing and boundary
pixels. A rotated-grid fixture must select the same index samples before/after
rigid placement; arbitrary oblique resampling remains future.

## Editing and storage

A separate label channel supports application-owned brushes through DatasetStore
and EditService transactions. Each gesture captures dataset epoch and base revisions,
previews locally, then commits validated changed regions. Cancellation publishes no
edit; revision conflicts reject or explicitly rebase through the application.
Preview and pending persistence are distinct. Saved checkpoints identify original
source plus committed label changes; the source intensity is not destructively
rewritten by window/level or label tools. See [voxel edits](../platform/VOXELS.md).

The initial app may compose existing controls and layout directly. The future
[edit-session helpers](../ui/CONTRACTS.md#future-edit-sessions-and-history),
[workspace/timeline controls](../ui/CONTROLS.md#future-workspace-panels) and
[asset browser](../ui/ASYNC.md#future-file-and-asset-browser) are reusable follow-up
work, not prerequisites for a four-panel viewer or local edits.

## Verification and extensions

Traverse recorded crosshair/plane traces with a cache smaller than the dataset;
measure source reads, evictions, stale cancellation, product/upload bytes and visible
coverage. Compare reconstructed voxels to the source and sampled pixels to CPU
references. Test label commit/cancel/conflict, interrupted save and reopen. Describe
both source and product budgets so cached slices cannot conceal broken source I/O.

Oblique resampling, ray marching, isosurfaces, DICOM/NIfTI adapters and time-series
import are future application/module work. Existing `.vox` imports exercise categorical
solid geometry; NRRD scalar volumes exercise sampled intensity. They share lifetime,
revision, spatial mapping and residency contracts without sharing material semantics.
A later trace timeline can exercise exact-time controls without requiring a 4D scan.
