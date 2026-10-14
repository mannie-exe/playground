# Medical volume streaming fixtures

MRHead and CTChest are real 3D Slicer sample volumes. The source lock is
[datasets.json](datasets.json); the original source bytes remain unchanged.
The Python authoring tool repackages their scalar samples into independently
compressed bricks, preserving signed values, voxel order and spatial mapping.

| Dataset | Samples | Type | Decoded size | 32³ bricks | Compressed brick payloads |
|---|---|---|---|---|---|
| MRHead | 256 × 256 × 130 | signed int16 | 16.25 MiB | 320 | 6,332,709 bytes |
| CTChest | 512 × 512 × 139 | signed int32 | 139 MiB | 1,280 | 39,773,977 bytes |

Sizes were measured from the pinned files, not inferred from screenshots. Both
volumes have partial boundary bricks. MRI axes include permutation/sign changes;
CT spacing is anisotropic. Do not narrow the CT integers or normalize source values
to 8-bit display colors. A transfer function is a separate view parameter.

## Obtain and prepare

Use Python 3.10 or newer; no pip, NumPy, Slicer installation or CMake build is needed.
Run from the repository root:

```sh
python3 tools/assets/voxel_suite.py prepare
python3 tools/assets/voxel_suite.py verify build/voxel-suite/mrhead-b32/manifest.json
python3 tools/assets/voxel_suite.py verify build/voxel-suite/ctchest-b32/manifest.json
python3 tools/assets/voxel_suite.py exercise build/voxel-suite/ctchest-b32/manifest.json --cache-mib 8 --report build/voxel-suite/ctchest-b32/streaming-report.json
python3 tools/assets/voxel_suite.py exercise build/voxel-suite/mrhead-b32/manifest.json --cache-mib 2 --report build/voxel-suite/mrhead-b32/streaming-report.json
```

`prepare` downloads only the selected pinned files, checks SHA-256, and stages a
complete fixture before publication. An existing matching fixture is verified;
corruption is reported rather than overwritten. `--dataset mrhead` limits acquisition.
`--brick-edge 16` or `64` creates another granularity under the same output root.
`--output PATH` changes that root. Source and decoded byte limits are explicit.
Compression is reproducible for the recorded Python/zlib versions; decoded hashes
are stable independently of compressor changes.

Downloads and generated payloads live in ignored `build/voxel-suite/`, not the Git
object store or normal application install. The recipe and license are tracked.
The initial downloads total 48,796,416 bytes. Source plus default packaged payloads
occupy approximately 100 MiB, plus filesystem overhead. Packaging and reconstruction
load a whole volume into authoring memory; the traversal reader does not.

## Fixture layout and reading

Each dataset directory contains:

- `manifest.json`: source identity, dimensions/mapping, brick addresses/extents,
  payload lengths and compressed/decoded SHA-256 hashes.
- `bricks/x_y_z.nhdr`: ordinary detached NRRD headers with the brick's physical origin.
- `bricks/x_y_z.raw.gz`: independently compressed, x-fastest little-endian samples.
- `preview.png`: three central index-axis slices; a diagnostic contact sheet, with
  square panels rather than anatomical aspect/orientation labels.
- `SLICER-LICENSE.txt`: the upstream contribution and software/data agreement.

The small JSON index is a versioned **test fixture**, not a new engine storage
format. A future SpatialDataSource adapter can read one indexed brick and publish
a ChunkSnapshot; standard NRRD tools can open individual headers already. No C++
volume loader or medical viewer is added by this fixture tool.

Source headers use LPS coordinates but omit explicit unit strings. The fixture
records millimeters as an explicit import convention, preserving the original
origin/direction values. Generated headers include those units. Source origin denotes
sample index zero; brick origins add integer index offsets through the direction
matrix. Do not add an extra half-voxel translation. A world adapter converts mm to
meters and LPS to its chosen world basis explicitly.

The reference reader verifies a brick before caching it. Its byte-limited LRU
exercises axis-slab traversals, reverse traversal and far-end teleports. An 8 MiB CT
cache is smaller than the volume and forces eviction. The cap covers decoded cache
payloads, not Python objects, metadata, compressed/decode scratch or the OS page
cache. It does not establish a process-memory cap or cold physical-disk performance.

## Verification and next integration

```sh
python3 -m unittest discover -s tools/assets -p 'test_voxel_suite.py' -v
```

Offline checks cover exact signed samples, odd extents, permuted/negative axes,
physical brick origins, repeatable outputs, cache eviction, corrupted data/headers,
missing/duplicate descriptors, escaped paths and bounded decompression. Full-volume
verification reconstructs the source sample array and compares its SHA-256.

The acquisition run reconstructed all 1,600 bricks exactly. The CT traversal made
8,064 requests, 7,586 reads and 7,492 evictions while retaining at most 8 MiB of
decoded payloads. The MRI traversal made 2,304 requests, 1,985 reads and 1,953 evictions
with a 2 MiB cap. Reports remain beside generated assets. These are Python
file/decode/cache observations, not Playground asynchronous, GPU or rendering tests.

Next, connect this source to the [spatial workloads](../../docs/platform/SPATIAL_TESTING.md):
three synchronized slice views, oblique/cross-boundary sampling, cancellation on fast
scrub, small residency budgets, delayed/corrupt bricks, and renderer replacement.
These scans exercise scalar streaming. They do not by themselves cover palette
block meshing, transparent terrain materials, simulated fluids or physical contacts.
Keep synthetic labeled volumes and physical-body demo fixtures for those boundaries.

## Provenance

The source registry is pinned to Slicer revision
`69bdf326e7feca062442b37461e42d87e9c3e5cb`. Its
[SampleData definition](https://github.com/Slicer/Slicer/blob/69bdf326e7feca062442b37461e42d87e9c3e5cb/Modules/Scripted/SampleData/SampleData.py)
publishes the file hashes and states that MRHead was donated for unrestricted use.
Slicer maintainers identify the applicable terms for
[MRHead](https://discourse.slicer.org/t/usage-rights-to-sample-data/17619) and
[CTChest](https://discourse.slicer.org/t/origin-of-ct-chest-sample-data/37731).
Retain [the complete agreement](SLICER-LICENSE.txt) with redistributed sources or
converted bricks. Credit the 3D Slicer project and sample-data contributors. CTChest's
original acquisition/patient metadata is not established here; do not invent it.
These fixtures are for software visualization/streaming tests.
