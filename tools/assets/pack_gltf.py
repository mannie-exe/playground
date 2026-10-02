"""Pack a local single-buffer glTF and PNG/JPEG images into GLB without re-exporting.

Usage: python3 tools/assets/pack_gltf.py INPUT.gltf OUTPUT.glb
This offline authoring tool reads only files beneath the input directory.
"""
import json
import pathlib
import struct
import sys

source, destination = map(pathlib.Path, sys.argv[1:])
root = source.resolve().parent
document = json.loads(source.read_text())

def read(uri):
    path = (root / uri).resolve()
    if not path.is_relative_to(root):
        raise ValueError(f'External resource escapes source directory: {uri}')
    return path.read_bytes()

if len(document['buffers']) != 1:
    raise ValueError('Expected exactly one source buffer')
blob = bytearray(read(document['buffers'][0]['uri']))
if len(blob) != document['buffers'][0]['byteLength']:
    raise ValueError('Source buffer size mismatch')
blob.extend(b'\0' * (-len(blob) % 4))
for image in document.get('images', []):
    uri = image.pop('uri')
    extension = pathlib.Path(uri).suffix.lower()
    mime = {'.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg'}[extension]
    encoded = read(uri)
    offset = len(blob)
    blob.extend(encoded)
    blob.extend(b'\0' * (-len(blob) % 4))
    image['bufferView'] = len(document.setdefault('bufferViews', []))
    image['mimeType'] = mime
    document['bufferViews'].append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(encoded)})
document['buffers'] = [{'byteLength': len(blob)}]
encoded = json.dumps(document, separators=(',', ':')).encode()
encoded += b' ' * (-len(encoded) % 4)
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_bytes(struct.pack('<III', 0x46546c67, 2, 28 + len(encoded) + len(blob)) +
                        struct.pack('<II', len(encoded), 0x4e4f534a) + encoded +
                        struct.pack('<II', len(blob), 0x004e4942) + blob)
