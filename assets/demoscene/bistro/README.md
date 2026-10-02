# Bistro provenance

Amazon Lumberyard Bistro, Open Research Content Archive (ORCA), 2017.
[Original distribution](https://developer.nvidia.com/orca/amazon-lumberyard-bistro).
License: [Creative Commons Attribution 4.0](CC-BY-4.0.txt).
Downloaded 2026-10-02 from https://developer.nvidia.com/bistro.

Source: Bistro_v5_2.zip, SHA-256 `0d50e3c724c6c5da19f8eb99ad3f53e36fec37ffa2df9621f9ccf0603f3934e1`.
Output: Bistro.glb (`demoscene.bistro`), SHA-256 `fa23f764061fa5c1feadfbf38ca07c87f56d610ebf89cf4e1b816399de293c83`.

Converted with Blender 4.4.3 and tools/assets/convert_bistro.py. The exterior's
geometry and hierarchy are retained without decimation. Zero-length exported
tangents at degenerate UVs receive a perpendicular fallback basis; valid tangents
are preserved. Cameras/lights are omitted.
DDS images become PNG, capped at 512 pixels on their longest edge. Normal maps
convert DirectX orientation to glTF and renormalize after resizing. Source
Specular G/B channels remain roughness/metallic. The reserved red channel is
uniformly zero in all 131 source maps and is not bound as glTF occlusion;
[Falcor's source format](https://github.com/NVIDIAGameWorks/Falcor/blob/master/docs/usage/scene-formats.md)
also leaves this channel unsupported. Binding zero as occlusion would eliminate
environment lighting. Solid surfaces use
OPAQUE; source alpha cutouts use MASK with cutoff 0.5. No new glass or lighting
technique is baked or implemented. The demo supplies its existing studio IBL and
starts at the source FBX street camera, converted from Blender world coordinates
to the runtime basis.

The original referenced texture dimensions imply roughly 4.67 GiB of RGBA8 mip
storage; the 512-pixel derivative is roughly 299 MiB before geometry and staging.
These are estimates, not measured process RSS or driver residency. The full source
archive and intermediate files are authoring inputs and are not shipped.

From the repository root, after extracting the original archive:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python tools/assets/convert_bistro.py -- SOURCE/Bistro_v5_2 assets/demoscene/bistro/Bistro.glb 512
```

On other hosts substitute the Blender 4.4.3 executable. Conversion is explicit;
CMake/install and runtime never invoke Blender or download this source.
