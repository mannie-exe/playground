"""Offline Bistro conversion. Run with Blender 4.4.3 --background --python this_file -- SOURCE_DIR OUTPUT.glb [MAX_TEXTURE_EDGE].

Inputs: NVIDIA ORCA Bistro_v5_2, with its original FBX and Textures directory.
This tool is not part of configuration, installation or runtime loading.
"""
import json
import struct
import pathlib
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index('--') + 1:]
source = pathlib.Path(args[0]).resolve()
output = pathlib.Path(args[1]).resolve()
edge = int(args[2]) if len(args) > 2 else 512
if edge < 1:
    raise ValueError('Texture edge must be positive')
output.parent.mkdir(parents=True, exist_ok=True)
# Temporary converted images belong beside the authoring inputs, never assets/.
images_dir = source / f'converted-{edge}'
images_dir.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source / 'BistroExterior.fbx'))
materials = {slot.material for obj in bpy.context.scene.objects
             if obj.type == 'MESH' for slot in obj.material_slots if slot.material}
converted = {}
alpha_materials = set()

def image_for(path, role):
    key = (str(path), role)
    if key in converted:
        return converted[key]
    image = bpy.data.images.load(str(path), check_existing=False)
    image.colorspace_settings.name = 'sRGB' if role in ('BaseColor', 'Emissive') else 'Non-Color'
    width, height = image.size
    if not width or not height:
        raise ValueError(f'Cannot decode {path}')
    if role == 'Normal':
        pixels = np.empty(width * height * 4, dtype=np.float32)
        image.pixels.foreach_get(pixels)
        pixels[1::4] = 1.0 - pixels[1::4]
        image.pixels.foreach_set(pixels)
    if max(width, height) > edge:
        factor = edge / max(width, height)
        image.scale(max(1, round(width * factor)), max(1, round(height * factor)))
    if role == 'Normal':
        width, height = image.size
        pixels = np.empty(width * height * 4, dtype=np.float32)
        image.pixels.foreach_get(pixels)
        rgb = pixels.reshape((-1, 4))[:, :3]
        normals = rgb * 2 - 1
        lengths = np.linalg.norm(normals, axis=1, keepdims=True)
        rgb[:] = (normals / np.maximum(lengths, 1e-8) + 1) * .5
        image.pixels.foreach_set(pixels)
    image.filepath_raw = str(images_dir / (path.stem + '.png'))
    image.file_format = 'PNG'
    image.save()
    converted[key] = image
    return image

for material in sorted(materials, key=lambda m: m.name):
    paths = {path.stem.rsplit('_', 1)[-1]: path
             for node in material.node_tree.nodes if node.type == 'TEX_IMAGE' and node.image
             for path in [pathlib.Path(node.image.filepath)]}
    nodes = material.node_tree.nodes
    nodes.clear()
    links = material.node_tree.links
    shader = nodes.new('ShaderNodeBsdfPrincipled')
    shader.inputs['Roughness'].default_value = .8
    shader.inputs['Metallic'].default_value = 0
    output_node = nodes.new('ShaderNodeOutputMaterial')
    links.new(shader.outputs['BSDF'], output_node.inputs['Surface'])
    for role, path in paths.items():
        if role not in ('BaseColor', 'Normal', 'Specular', 'Emissive'):
            continue
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = image_for(path, role)
        if role == 'BaseColor':
            links.new(tex.outputs['Color'], shader.inputs['Base Color'])
            pixels = np.empty(tex.image.size[0] * tex.image.size[1] * 4, dtype=np.float32)
            tex.image.pixels.foreach_get(pixels)
            if np.min(pixels[3::4]) < .999:
                alpha_materials.add(material.name)
                links.new(tex.outputs['Alpha'], shader.inputs['Alpha'])
        elif role == 'Normal':
            normal = nodes.new('ShaderNodeNormalMap')
            links.new(tex.outputs['Color'], normal.inputs['Color'])
            links.new(normal.outputs['Normal'], shader.inputs['Normal'])
        elif role == 'Specular':
            split = nodes.new('ShaderNodeSeparateColor')
            links.new(tex.outputs['Color'], split.inputs['Color'])
            links.new(split.outputs['Green'], shader.inputs['Roughness'])
            links.new(split.outputs['Blue'], shader.inputs['Metallic'])
            # Bistro's reserved red channel is uniformly zero, and Falcor
            # does not consume it as occlusion. Do not bind it to glTF AO.
        else:
            links.new(tex.outputs['Color'], shader.inputs['Emission Color'])
            shader.inputs['Emission Strength'].default_value = 1
    # Cutout is an existing core renderer feature; do not introduce glass.
    material.surface_render_method = 'DITHERED'
    material.use_transparent_shadow = False

# Preserve source meshes and hierarchy without geometry decimation.
for obj in bpy.context.scene.objects:
    obj.select_set(obj.type == 'MESH')
# FBX lights/cameras do not become newly supported runtime features.
bpy.ops.export_scene.gltf(filepath=str(output), export_format='GLB',
                          use_selection=True, export_yup=True, export_animations=False,
                          export_cameras=False, export_lights=False, export_tangents=True)
# Keep depth writes for solid geometry and use existing alpha masking for cutouts.
# A source opacity texture does not make every building a blended surface.
raw = output.read_bytes()
json_length = struct.unpack_from('<I', raw, 12)[0]
document = json.loads(raw[20:20 + json_length])
for material in document['materials']:
    material['alphaMode'] = 'MASK' if material['name'] in alpha_materials else 'OPAQUE'
    if material['alphaMode'] == 'MASK':
        material['alphaCutoff'] = .5
encoded = json.dumps(document, separators=(',', ':')).encode()
encoded += b' ' * (-len(encoded) % 4)
binary = bytearray(raw[20 + json_length:])
repaired = 0
visited = set()
for mesh in document['meshes']:
    for primitive in mesh['primitives']:
        tangent_index = primitive['attributes'].get('TANGENT')
        if tangent_index is None or tangent_index in visited:
            continue
        visited.add(tangent_index)
        tangent = document['accessors'][tangent_index]
        normal = document['accessors'][primitive['attributes']['NORMAL']]
        def values(accessor, components):
            if accessor['componentType'] != 5126:
                raise ValueError('Expected float directions from Blender')
            view = document['bufferViews'][accessor['bufferView']]
            return np.ndarray((accessor['count'], components), dtype='<f4', buffer=binary,
                              offset=8 + view.get('byteOffset', 0) + accessor.get('byteOffset', 0),
                              strides=(view.get('byteStride', components * 4), 4))
        t, n = values(tangent, 4), values(normal, 3)
        bad = np.linalg.norm(t[:, :3], axis=1) == 0
        if np.any(bad):
            normals = n[bad]
            axis = np.zeros_like(normals)
            axis[np.arange(len(axis)), np.argmin(np.abs(normals), axis=1)] = 1
            perpendicular = np.cross(normals, axis)
            perpendicular /= np.linalg.norm(perpendicular, axis=1, keepdims=True)
            t[bad, :3] = perpendicular
            repaired += int(np.count_nonzero(bad))
# Release array views before assembling immutable output bytes.
binary = bytes(binary)
output.write_bytes(struct.pack('<III', 0x46546c67, 2, 20 + len(encoded) + len(binary)) +
                   struct.pack('<II', len(encoded), 0x4e4f534a) + encoded + binary)
print('OUTPUT', output, output.stat().st_size, 'textures', len(converted),
      'masked materials', len(alpha_materials), 'repaired tangents', repaired, flush=True)
