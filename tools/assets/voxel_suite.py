"""Prepare and exercise independently compressed NRRD bricks (Python stdlib only).

This is an offline fixture tool and reference reader, not the engine DatasetStore.
Run with --help. Large downloads/products default to the ignored build directory.
"""
import argparse
import collections
import gzip
import hashlib
import itertools
import json
import math
from pathlib import Path
import platform
import re
import shutil
import struct
import sys
import tempfile
import time
import urllib.request
import zlib

ROOT = Path(__file__).resolve().parents[2]
LOCK = ROOT / 'assets/volumes/datasets.json'
DEFAULT_OUTPUT = ROOT / 'build/voxel-suite'
TYPES = {'short': ('h', 2), 'int': ('i', 4)}
MAX_MANIFEST_BYTES = 64 * 1024**2


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, value, max_bytes=None):
    encoded = (json.dumps(value, indent=2, allow_nan=False) + '\n').encode('utf-8')
    if max_bytes is not None and len(encoded) > max_bytes:
        raise ValueError('JSON document exceeds byte limit')
    path.write_bytes(encoded)


def fetch(spec, directory):
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / spec['filename']
    if path.exists():
        if path.stat().st_size > spec['max_download_bytes'] or digest(path.read_bytes()) != spec['sha256']:
            raise ValueError(f'{path}: existing source differs from pinned hash')
        return path
    with tempfile.NamedTemporaryFile(dir=directory, delete=False) as output:
        temporary = Path(output.name)
        try:
            request = urllib.request.Request(spec['url'], headers={'User-Agent': 'Playground-voxel-suite/1'})
            with urllib.request.urlopen(request, timeout=60) as source:
                size = 0
                checksum = hashlib.sha256()
                while block := source.read(1024 * 1024):
                    size += len(block)
                    if size > spec['max_download_bytes']:
                        raise ValueError('download exceeds source limit')
                    checksum.update(block)
                    output.write(block)
            if checksum.hexdigest() != spec['sha256']:
                raise ValueError('download SHA-256 mismatch')
            output.close()
            temporary.replace(path)
        finally:
            # Windows cannot unlink the file while this writer remains open.
            output.close()
            temporary.unlink(missing_ok=True)
    return path


def read_nrrd(path, limit):
    """Read only the pinned 3D signed-integer, little-endian raw/gzip subset."""
    with path.open('rb') as source:
        magic = source.readline(32)
        if magic.rstrip() not in (b'NRRD0004', b'NRRD0005'):
            raise ValueError('unsupported NRRD magic')
        fields = {}
        header_bytes = len(magic)
        while True:
            line = source.readline(65537)
            header_bytes += len(line)
            if header_bytes > 65536 or not line:
                raise ValueError('unterminated/oversized NRRD header')
            if not line.strip():
                break
            text = line.decode('ascii').strip()
            if text.startswith('#'):
                continue
            key, value = text.split(':', 1)
            if key in fields:
                raise ValueError('duplicate NRRD field')
            fields[key] = value.strip()
        if fields.get('dimension') != '3' or fields.get('type') not in TYPES:
            raise ValueError('expected three-dimensional short/int NRRD')
        if fields.get('endian') != 'little' or fields.get('encoding') not in ('gzip', 'raw'):
            raise ValueError('unsupported byte order/encoding')
        if any(k in fields for k in ('data file', 'byte skip', 'line skip')):
            raise ValueError('detached/offset input is outside this import subset')
        sizes = [int(x) for x in fields['sizes'].split()]
        if len(sizes) != 3 or min(sizes) <= 0:
            raise ValueError('invalid volume extents')
        expected = math.prod(sizes) * TYPES[fields['type']][1]
        if expected > limit:
            raise ValueError('decoded volume exceeds limit')
        reader = gzip.GzipFile(fileobj=source) if fields['encoding'] == 'gzip' else source
        try:
            data = reader.read(expected + 1)
        finally:
            if reader is not source:
                reader.close()
        if len(data) != expected:
            raise ValueError('decoded size does not match dimensions')
    vectors = re.findall(r'\(([^)]+)\)', fields['space directions'])
    directions = [[float(v) for v in vector.split(',')] for vector in vectors]
    origin = [float(v) for v in fields['space origin'].strip('()').split(',')]
    metadata = {'sizes': sizes, 'type': fields['type'], 'endian': 'little',
                'space': fields['space'], 'directions': directions, 'origin': origin}
    validate_metadata(metadata)
    return metadata, data


def validate_metadata(metadata):
    """Validate the fixture's little-endian, orthogonal LPS interpretation."""
    sizes = metadata['sizes']
    if (not isinstance(sizes, list) or len(sizes) != 3 or
            any(type(v) != int or v <= 0 for v in sizes)):
        raise ValueError('invalid volume dimensions')
    if metadata['type'] not in TYPES or metadata['endian'] != 'little':
        raise ValueError('unsupported fixture type/byte order')
    if math.prod(sizes) * TYPES[metadata['type']][1] > 256*1024**2:
        raise ValueError('volume exceeds fixture decoded-size limit')
    if metadata['space'] != 'left-posterior-superior':
        raise ValueError('unsupported fixture coordinate space')
    directions, origin = metadata['directions'], metadata['origin']
    if (not isinstance(directions, list) or len(directions) != 3 or
            any(not isinstance(v, list) or len(v) != 3 for v in directions) or
            not isinstance(origin, list) or len(origin) != 3):
        raise ValueError('invalid spatial mapping')
    if not all(type(v) in (int, float) and -sys.float_info.max <= v <= sys.float_info.max
               and math.isfinite(v) for row in directions + [origin] for v in row):
        raise ValueError('nonfinite spatial mapping')
    lengths = [math.hypot(*row) for row in directions]
    if any(not math.isfinite(v) or v <= 0 for v in lengths):
        raise ValueError('expected finite nonzero spatial directions')
    axes = [[v/length for v in row] for row, length in zip(directions, lengths)]
    if any(abs(sum(a*b for a, b in zip(axes[i], axes[j]))) > 1e-6
           for i in range(3) for j in range(i)):
        raise ValueError('expected orthogonal spatial directions')
    for index in itertools.product(*[(0, n-1) for n in sizes]):
        if not all(math.isfinite(origin[axis] + sum(index[i]*directions[i][axis] for i in range(3)))
                   for axis in range(3)):
            raise ValueError('spatial mapping exceeds finite coordinates')


def validate_units(record):
    if (record['units'] != 'mm' or not isinstance(record['units_basis'], str) or
            not record['units_basis'].strip() or len(record['units_basis']) > 1024):
        raise ValueError('fixture requires explicit millimetre units and provenance')


def extract(data, sizes, start, extent, width):
    nx, ny, _ = sizes
    x, y, z = start
    ex, ey, ez = extent
    out = bytearray(ex * ey * ez * width)
    offset = 0
    for iz in range(z, z+ez):
        for iy in range(y, y+ey):
            begin = ((iz*ny + iy)*nx + x)*width
            length = ex*width
            out[offset:offset+length] = data[begin:begin+length]
            offset += length
    return bytes(out)


def brick_header(metadata, start, extent, filename):
    origin = [metadata['origin'][axis] + sum(start[i]*metadata['directions'][i][axis] for i in range(3))
              for axis in range(3)]
    vector = lambda values: '(' + ','.join(format(x, '.17g') for x in values) + ')'
    return ('NRRD0005\n# Playground streaming fixture; sample values unchanged\n'
            f'type: {metadata["type"]}\ndimension: 3\nspace: {metadata["space"]}\n'
            f'sizes: {" ".join(map(str, extent))}\n'
            f'space directions: {" ".join(map(vector, metadata["directions"]))}\n'
            f'space origin: {vector(origin)}\nspace units: "mm" "mm" "mm"\n'
            f'endian: little\nencoding: gzip\ndata file: {filename}\n\n')


def preview(path, metadata, data, display_range):
    """Write a diagnostic contact sheet in index axes, not anatomical orientation."""
    sizes = metadata['sizes']; code, width = TYPES[metadata['type']]
    panel = 256; image = bytearray(panel*3*panel)
    low, high = display_range
    for fixed in range(3):
        axes = [i for i in range(3) if i != fixed]
        for py in range(panel):
            for px in range(panel):
                p = [v//2 for v in sizes]
                p[axes[0]] = min(sizes[axes[0]]-1, px*sizes[axes[0]]//panel)
                p[axes[1]] = min(sizes[axes[1]]-1, py*sizes[axes[1]]//panel)
                offset = ((p[2]*sizes[1]+p[1])*sizes[0]+p[0])*width
                value = struct.unpack_from('<'+code, data, offset)[0]
                image[py*panel*3+fixed*panel+px] = max(0, min(255, round((value-low)*255/(high-low))))
    def chunk(kind, payload):
        return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
    scanlines = b''.join(b'\0'+image[y*panel*3:(y+1)*panel*3] for y in range(panel))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',panel*3,panel,8,0,0,0,0))+
                     chunk(b'IDAT',zlib.compress(scanlines))+chunk(b'IEND',b''))


def pack(spec, source, output, edge):
    if output.exists():
        raise ValueError(f'{output} already exists; choose a new output or verify the existing fixture')
    validate_units(spec)
    metadata, data = read_nrrd(source, spec['max_decoded_bytes'])
    if digest(source.read_bytes()) != spec['sha256']:
        raise ValueError('source SHA-256 mismatch')
    sizes = metadata['sizes']; width = TYPES[metadata['type']][1]
    grid = [(v+edge-1)//edge for v in sizes]
    if math.prod(grid) > 100000:
        raise ValueError('brick count exceeds fixture limit')
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=output.name+'.',dir=output.parent))
    try:
        bricks = staging/'bricks'; bricks.mkdir()
        records = []
        for z,y,x in itertools.product(range(grid[2]), range(grid[1]), range(grid[0])):
            start = [x*edge,y*edge,z*edge]
            extent = [min(edge, sizes[i]-start[i]) for i in range(3)]
            raw = extract(data,sizes,start,extent,width)
            encoded = gzip.compress(raw,compresslevel=6,mtime=0)
            name = f'{x}_{y}_{z}'
            (bricks/(name+'.raw.gz')).write_bytes(encoded)
            header = brick_header(metadata,start,extent,name+'.raw.gz')
            (bricks/(name+'.nhdr')).write_text(header)
            records.append({'address':[x,y,z],'start':start,'sizes':extent,
                            'header':f'bricks/{name}.nhdr','payload':f'bricks/{name}.raw.gz',
                            'encoded_bytes':len(encoded),'decoded_bytes':len(raw),
                            'sha256':digest(encoded),'decoded_sha256':digest(raw)})
        manifest = {'fixture_schema':1,'id':spec['id'],'source_sha256':spec['sha256'],
                    'source_url':spec['url'],'volume':metadata,'units':spec['units'],
                    'units_basis':spec['units_basis'],'sample_layout':'x-fastest; voxel centers',
                    'brick_edge':edge,'grid':grid,'decoded_bytes':len(data),'decoded_sha256':digest(data),
                    'encoded_brick_bytes':sum(b['encoded_bytes'] for b in records),
                    'tool_version':1,'python':sys.version.split()[0], 'zlib':zlib.ZLIB_RUNTIME_VERSION,
                    'bricks':records}
        write_json(staging/'manifest.json', manifest, max_bytes=MAX_MANIFEST_BYTES)
        preview(staging/'preview.png',metadata,data,spec['preview_range'])
        shutil.copyfile(LOCK.parent/'SLICER-LICENSE.txt',staging/'SLICER-LICENSE.txt')
        staging.rename(output)
    finally:
        if staging.exists():shutil.rmtree(staging)
    return manifest


def load_manifest(path):
    if path.stat().st_size > MAX_MANIFEST_BYTES:raise ValueError('manifest too large')
    m = json.loads(path.read_text())
    if m['fixture_schema'] != 1 or not 0 < len(m['bricks']) <= 100000:
        raise ValueError('unsupported fixture schema/size')
    validate_metadata(m['volume'])
    validate_units(m)
    if m['sample_layout'] != 'x-fastest; voxel centers':
        raise ValueError('unsupported fixture sample layout')
    sizes=m['volume']['sizes'];edge=m['brick_edge']
    if len(sizes)!=3 or any(type(v)!=int or v<=0 for v in sizes) or type(edge)!=int or not 8<=edge<=128:
        raise ValueError('invalid fixture bounds')
    expected_grid=[(n+edge-1)//edge for n in sizes]
    if m['grid'] != expected_grid or len(m['bricks']) != math.prod(expected_grid):
        raise ValueError('incomplete brick coverage')
    width=TYPES[m['volume']['type']][1];seen=set()
    for b in m['bricks']:
        key=tuple(b['address'])
        if len(key)!=3 or key in seen or any(type(v)!=int or not 0<=v<expected_grid[i] for i,v in enumerate(key)):
            raise ValueError('duplicate/invalid brick address')
        seen.add(key);start=[v*edge for v in key];extent=[min(edge,sizes[i]-start[i]) for i in range(3)]
        if b['start']!=start or b['sizes']!=extent or b['decoded_bytes']!=math.prod(extent)*width:
            raise ValueError('inconsistent brick bounds')
        if type(b['encoded_bytes'])!=int or not 0 < b['encoded_bytes'] <= b['decoded_bytes']*2+1024:
            raise ValueError('invalid encoded size')
        for field in ('payload','header'):
            resolved=(path.parent/b[field]).resolve()
            if not resolved.is_relative_to(path.parent.resolve()):raise ValueError('path escapes fixture')
    if m['decoded_bytes']!=math.prod(sizes)*width or m['decoded_bytes']>256*1024**2:
        raise ValueError('invalid fixture decoded size')
    return m


def read_brick(root, record):
    path = root/record['payload']
    if path.stat().st_size != record['encoded_bytes']:
        raise ValueError('encoded brick size mismatch')
    encoded = path.read_bytes()
    if digest(encoded) != record['sha256']:raise ValueError('encoded brick checksum mismatch')
    decoder=zlib.decompressobj(16+zlib.MAX_WBITS)
    raw=decoder.decompress(encoded, record['decoded_bytes']+1)
    if len(raw)!=record['decoded_bytes'] or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('invalid/oversized compressed brick')
    if digest(raw)!=record['decoded_sha256']:raise ValueError('decoded brick checksum mismatch')
    return raw


def verify(path):
    m=load_manifest(path);sizes=m['volume']['sizes'];width=TYPES[m['volume']['type']][1]
    result=bytearray(m['decoded_bytes'])
    for b in m['bricks']:
        raw=read_brick(path.parent,b);x,y,z=b['start'];ex,ey,ez=b['sizes'];offset=0
        expected=brick_header(m['volume'],b['start'],b['sizes'],Path(b['payload']).name)
        if (path.parent/b['header']).read_text()!=expected:raise ValueError('brick spatial header differs')
        for iz in range(z,z+ez):
            for iy in range(y,y+ey):
                begin=((iz*sizes[1]+iy)*sizes[0]+x)*width;length=ex*width
                result[begin:begin+length]=raw[offset:offset+length];offset+=length
    if digest(result)!=m['decoded_sha256']:raise ValueError('reassembled source checksum mismatch')
    return {'dataset':m['id'],'verified_bricks':len(m['bricks']),'decoded_bytes':len(result),
            'decoded_sha256':digest(result),'spatial_headers':'verified'}


def exercise(path, capacity):
    m=load_manifest(path);records=m['bricks'];cache=collections.OrderedDict()
    stats={'dataset':m['id'],'python':sys.version.split()[0],'platform':platform.platform(),
           'tool_sha256':digest(Path(__file__).read_bytes()),'manifest_sha256':digest(path.read_bytes()),
           'cache_capacity_bytes':capacity,'peak_resident_bytes':0,
           'requests':0,'hits':0,'reads':0,'evictions':0,'encoded_bytes_read':0,'decoded_bytes_read':0}
    resident=0;durations=[]
    # Full slice-slab traversals in each index axis, followed by far-end teleports.
    traces=[]
    for axis in range(3):
        order=sorted(range(len(records)),key=lambda i:(records[i]['address'][axis],records[i]['address']))
        traces.extend(order);traces.extend(reversed(order))
    for i in range(min(128,len(records))):traces.extend([i,len(records)-1-i,i])
    for i in traces:
        stats['requests']+=1
        if i in cache:
            stats['hits']+=1;cache.move_to_end(i);continue
        b=records[i]
        if b['decoded_bytes']>capacity:raise ValueError('one brick exceeds cache capacity')
        while resident+b['decoded_bytes']>capacity:
            _,old=cache.popitem(last=False);resident-=len(old);stats['evictions']+=1
        start=time.perf_counter();raw=read_brick(path.parent,b);durations.append((time.perf_counter()-start)*1000)
        cache[i]=raw;resident+=len(raw)
        stats['peak_resident_bytes']=max(stats['peak_resident_bytes'],resident)
        stats['reads']+=1;stats['encoded_bytes_read']+=b['encoded_bytes'];stats['decoded_bytes_read']+=len(raw)
    durations.sort()
    stats.update(read_decode_ms_p50=durations[len(durations)//2],read_decode_ms_p95=durations[int((len(durations)-1)*.95)],
                 read_decode_ms_max=max(durations),trace='axis-slab sweeps forward/reverse plus far-end teleports',
                 coverage='Python file/decode/cache only; OS cache uncontrolled; no engine/GPU/async evidence')
    return stats


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    commands=parser.add_subparsers(dest='command',required=True)
    prepare=commands.add_parser('prepare');prepare.add_argument('--dataset',choices=['all','mrhead','ctchest'],default='all')
    prepare.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    prepare.add_argument('--brick-edge',type=int,choices=[8,16,32,64,128],default=32)
    for name in ('verify','exercise'):
        sub=commands.add_parser(name);sub.add_argument('manifest',type=Path)
        if name=='exercise':
            sub.add_argument('--cache-mib',type=int,default=8);sub.add_argument('--report',type=Path)
    args=parser.parse_args()
    if args.command=='prepare':
        lock=json.loads(LOCK.read_text())
        for spec in lock['datasets']:
            if args.dataset not in ('all',spec['id']):continue
            source=fetch(spec,args.output/'sources')
            target=args.output/(spec['id']+f'-b{args.brick_edge}')
            if target.exists():
                m=load_manifest(target/'manifest.json')
                if m['source_sha256']!=spec['sha256'] or m['brick_edge']!=args.brick_edge:
                    raise ValueError('existing fixture differs from requested source/profile')
                print(json.dumps(verify(target/'manifest.json')))
            else:
                m=pack(spec,source,target,args.brick_edge)
                print(json.dumps({'dataset':m['id'],'bricks':len(m['bricks']),'decoded_bytes':m['decoded_bytes'],
                                  'encoded_brick_bytes':m['encoded_brick_bytes'],'manifest':str(target/'manifest.json')}))
    else:
        if args.command=='exercise' and not 1<=args.cache_mib<=1024:raise ValueError('cache MiB must be in [1,1024]')
        result=verify(args.manifest) if args.command=='verify' else exercise(args.manifest,args.cache_mib*1024**2)
        print(json.dumps(result,indent=2))
        if getattr(args,'report',None):write_json(args.report,result)


if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError,TypeError,EOFError,zlib.error) as error:
        print(f'voxel_suite: {error}',file=sys.stderr);sys.exit(1)
