# Runtime content inventory

Files here are copied to `bin/assets` during install. Logical asset IDs live in
the C++ app catalogs; filenames are not an application-facing identity scheme.
Keep editable originals separate from generated build outputs. This inventory is
not a license grant; unknown provenance must be resolved before distribution.

| Files | Current role | Provenance / redistribution status |
|---|---|---|
| `images/IMG_6239.PNG` | Demo 2D image (`demo2d.image`) | Supplied study asset; source/author/permission not recorded |
| `ui/icons/add.svg`, `remove.svg` | Demo 2D stepper (`demo2d.add`, `demo2d.remove`) | Google Material Icons, Apache-2.0; white-fill adaptation, source URLs and license in [icon provenance](ui/icons/README.md) |
| `fonts/LBRITE.TTF` | Shared launcher/Demo 2D/Demo 3D font (`app.font`) | Source/license not recorded; font filename does not establish redistribution rights |
| `fonts/jurriaan_3d-fill.ttf` | Minesweeper font (`minesweeper.font`) | Source/license not recorded |
| `fonts/jurriaan_3d-dark.ttf`, `jurriaan_3d-ribbed.ttf`, `jurriaan_3d-shaded.ttf` | Retained study font variants | Source/license not recorded; not loaded by current catalogs |
| `minesweeper/images/bomb.svg`, `flag.svg` | Minesweeper vector icons | Original source/license not recorded; current SVGs contain fill and size attributes |
| `minesweeper/images/bomb.png`, `flag.png` | Retained raster study variants | Source/license not recorded; not loaded by current catalogs |
| `demo3d/FlightHelmet.glb`, `SciFiHelmet.glb` | Material Test helmet fixtures | CC0; [provenance](demo3d/README.md) |
| `demoscene/bistro/Bistro.glb` | Scene: Bistro exterior | CC-BY 4.0; [source and conversion](demoscene/bistro/README.md) |
| `demoscene/chess/ABeautifulGame.glb` | Scene: Chess | CC-BY 4.0; [provenance](demoscene/chess/README.md) |
| `demo3d/BoomBox.glb`, `Smoke30Frames.png`, `studio_small_09_1k.hdr` | Textured PBR prop, smoke flipbook and environment (`demo3d.*`) | CC0; authors, sources, hashes and license texts in [Demo 3D provenance](demo3d/README.md) |

For new content record: logical ID(s), source URL or authoring file, creator,
license text/required attribution, local changes, and dependencies. Models must
list external textures/buffers; generated content should additionally record its
tool/model version, generation inputs when available, and human review status.
Do not infer permissions from appearance, filenames or a generative tool's output.

Engine shaders are authored under `shaders/`; test shaders under `tests/shaders/`
are not production assets. Compiled SPIR-V belongs in `build/<preset>/shaders`
and the install tree, not here. No asset packing or filesystem watching is implied.
