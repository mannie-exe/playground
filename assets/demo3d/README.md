# Demo 3D asset provenance

Downloaded 2026-09-26. The original BoomBox, smoke and HDR files are unmodified; preparation happens
in memory. These assets are installed with the application, not fetched at runtime.

| File | Author / source | License | SHA-256 |
|---|---|---|---|
| BoomBox.glb | Microsoft, [Khronos sample](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/BoomBox) | CC0-1.0 | f8b918445ebdd006768232205a62f5182d2208ca57f84c6ccc084943c0bc8f15 |
| Smoke30Frames.png | Beast, [Smoke Aura](https://opengameart.org/content/smoke-aura) | CC0-1.0 | 241f22c61bece5efcdc8afd7c95ddd107d6c9daf1bb95c2f28453be9ea96f92c |
| studio_small_09_1k.hdr | [Poly Haven Studio Small 09](https://polyhaven.com/a/studio_small_09) | CC0-1.0 | e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45 |

Licenses: [CC0 legal text](https://creativecommons.org/publicdomain/zero/1.0/legalcode),
[Poly Haven asset policy](https://polyhaven.com/license).

Direct downloads:

- https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets/main/Models/BoomBox/glTF-Binary/BoomBox.glb
- https://opengameart.org/sites/default/files/Smoke30Frames_0.png
- https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/studio_small_09_1k.hdr

The downloaded smoke atlas is 1536x1279: six columns and five rows of nominal
256x256 frames, with one pixel cropped from the last row. Pixel-grid playback
clamps those last six frames to 255 pixels high rather than distorting every row.
Sampling uses
half-texel insets and no whole-atlas mip generation; it is not a volumetric simulation.

`shaders/scene_tone.frag.hlsl` adapts the Khronos PBR Neutral operator from
[Khronos ToneMapping](https://github.com/KhronosGroup/ToneMapping), Apache-2.0.
The original license is distributed in this directory. Changes: HLSL syntax,
scene exposure, and straight/premultiplied conversion around tone mapping.

## Helmet fixtures

Khronos source revision: `edc7c9e67c639d230715049ee31f9a96a6babbbe`. Downloaded 2026-10-02.
Both helmets are CC0-1.0; the license is [CC0.txt](CC0.txt).
`tools/assets/pack_gltf.py` embeds the original buffer and PNG files in GLB;
geometry, textures, material definitions and source resolution are unchanged.
Flight Helmet transmission uses the explicitly approximate core-material preview.

| File / logical ID | Credit and pinned source | SHA-256 |
|---|---|---|
| SciFiHelmet.glb / demo3d.scifi-helmet | Michael Pavlovic; Norbert Nopper (glTF conversion), [source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/edc7c9e67c639d230715049ee31f9a96a6babbbe/Models/SciFiHelmet) | ae5c7378e52755c3cc26f65ff496d75ef10ea4b8c47dc457e6b25e4e304c53e1 |
| FlightHelmet.glb / demo3d.flight-helmet | Public CC0 asset; Gary Hsu (Maya conversion), [source](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/edc7c9e67c639d230715049ee31f9a96a6babbbe/Models/FlightHelmet) | c02664bf7765ce369f3014f12d9a1bf146e104ca4268aff689845fca125b0580 |

Pack a downloaded model with:

```sh
python3 tools/assets/pack_gltf.py SOURCE/glTF/FlightHelmet.gltf assets/demo3d/FlightHelmet.glb
python3 tools/assets/pack_gltf.py SOURCE/glTF/SciFiHelmet.gltf assets/demo3d/SciFiHelmet.glb
```
