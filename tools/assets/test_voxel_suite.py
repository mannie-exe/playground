"""Offline contract checks: python3 -m unittest discover -s tools/assets -p 'test_voxel_suite.py'."""
import gzip
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

import voxel_suite as suite


class VolumeSuiteTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sizes = [17, 10, 9]
        self.raw = b''.join(struct.pack('<h', x+31*y+401*z-2000)
                            for z in range(9) for y in range(10) for x in range(17))
        self.header = ('NRRD0004\ntype: short\ndimension: 3\nspace: left-posterior-superior\n'
                       'sizes: 17 10 9\nspace directions: (0,1,0) (0,0,-2) (-3,0,0)\n'
                       'space origin: (-12,30,5)\nendian: little\nencoding: gzip\n\n')
        self.source = self.root/'input.nrrd'
        self.source.write_bytes(self.header.encode()+gzip.compress(self.raw, mtime=0))
        self.spec = {'id':'synthetic','sha256':suite.digest(self.source.read_bytes()),
                     'url':'https://example.invalid/fixture','units':'mm','units_basis':'synthetic',
                     'max_decoded_bytes':10000,'preview_range':[-2000,2000]}

    def packed(self, name='fixture'):
        target = self.root/name
        suite.pack(self.spec,self.source,target,8)
        return target/'manifest.json'

    def test_reconstruction_and_partial_edge_bricks(self):
        path=self.packed();m=suite.load_manifest(path)
        self.assertEqual(m['grid'],[3,2,2])
        self.assertEqual(m['bricks'][-1]['sizes'],[1,2,1])
        self.assertEqual(suite.verify(path)['decoded_sha256'],suite.digest(self.raw))
        b=m['bricks'][-1]
        actual=suite.read_brick(path.parent,b)
        expected=b''.join(struct.pack('<h',16+31*y+401*8-2000) for y in (8,9))
        self.assertEqual(actual,expected)
        # x=16,y=8,z=8 transforms to (-36,46,-11), with no half-voxel shift.
        self.assertIn('space origin: (-36,46,-11)',(path.parent/b['header']).read_text())

    def test_repeatable_pack(self):
        a=self.packed('a').parent;b=self.packed('b').parent
        for file in a.rglob('*'):
            if file.is_file():self.assertEqual(file.read_bytes(),(b/file.relative_to(a)).read_bytes())

    def test_cache_eviction_and_oversized_brick(self):
        path=self.packed();report=suite.exercise(path,2048)
        self.assertLessEqual(report['peak_resident_bytes'],2048)
        self.assertGreater(report['evictions'],0)
        self.assertGreater(report['hits'],0)
        with self.assertRaisesRegex(ValueError,'exceeds cache'):suite.exercise(path,4)

    def test_corrupt_payload_and_header(self):
        path=self.packed();m=suite.load_manifest(path);b=m['bricks'][0]
        payload=path.parent/b['payload'];data=bytearray(payload.read_bytes());data[12]^=1;payload.write_bytes(data)
        with self.assertRaisesRegex(ValueError,'checksum'):suite.verify(path)
        path2=self.packed('other');m2=suite.load_manifest(path2)
        (path2.parent/m2['bricks'][0]['header']).write_text('wrong geometry')
        with self.assertRaisesRegex(ValueError,'spatial header'):suite.verify(path2)

    def test_missing_duplicate_and_escaping_descriptors(self):
        path=self.packed();original=json.loads(path.read_text())
        for mutation in ('missing','duplicate','escape'):
            m=json.loads(json.dumps(original))
            if mutation=='missing':m['bricks'].pop()
            elif mutation=='duplicate':m['bricks'][1]=m['bricks'][0]
            else:m['bricks'][0]['payload']='../../outside.raw.gz'
            path.write_text(json.dumps(m))
            with self.assertRaises(ValueError):suite.load_manifest(path)

    def test_decode_limit_wrong_endian_and_length(self):
        with self.assertRaisesRegex(ValueError,'limit'):suite.read_nrrd(self.source,4)
        self.source.write_bytes(self.header.replace('little','big').encode()+gzip.compress(self.raw))
        with self.assertRaisesRegex(ValueError,'byte order'):suite.read_nrrd(self.source,10000)
        self.source.write_bytes(self.header.encode()+gzip.compress(self.raw+b'\0\0'))
        with self.assertRaisesRegex(ValueError,'decoded size'):suite.read_nrrd(self.source,10000)

    def test_download_failures_preserve_error_and_remove_temporary_files(self):
        # Model Windows file-sharing rules while using real temporary files.
        original_temporary = tempfile.NamedTemporaryFile
        original_unlink = Path.unlink
        opened = []

        def track_temporary(*args, **kwargs):
            output = original_temporary(*args, **kwargs)
            opened.append(output)
            return output

        def unlink_closed(path, *args, **kwargs):
            if any(not output.closed and Path(output.name) == path for output in opened):
                raise PermissionError('cannot delete an open file on Windows')
            return original_unlink(path, *args, **kwargs)

        cases = [('checksum', ValueError, 'SHA-256'),
                 ('limit', ValueError, 'limit'),
                 ('timeout', TimeoutError, 'download timeout')]
        for case, error, message in cases:
            with self.subTest(case=case):
                directory = self.root/case
                spec = dict(self.spec, filename='download.nrrd', max_download_bytes=4,
                            sha256=suite.digest(b'data'))
                if case == 'checksum':
                    spec['sha256'] = '0'*64
                elif case == 'limit':
                    spec['max_download_bytes'] = 3
                with mock.patch.object(suite.urllib.request, 'urlopen') as request, \
                     mock.patch.object(suite.tempfile, 'NamedTemporaryFile', side_effect=track_temporary), \
                     mock.patch.object(Path, 'unlink', autospec=True, side_effect=unlink_closed):
                    if case == 'timeout':
                        request.side_effect = TimeoutError('download timeout')
                    else:
                        request.return_value = io.BytesIO(b'data')
                    with self.assertRaisesRegex(error, message):
                        suite.fetch(spec, directory)
                self.assertEqual(list(directory.iterdir()), [])
                self.assertTrue(all(output.closed for output in opened))

    def test_rejects_invalid_fixture_interpretation(self):
        path = self.packed()
        original = json.loads(path.read_text())
        cases = [
            (('volume', 'endian'), 'big'),
            (('volume', 'space'), 'unknown-coordinate-system'),
            (('volume', 'sizes'), [10**400, 10, 9]),
            (('volume', 'origin'), [0, float('nan'), 0]),
            (('volume', 'origin'), [0, 0]),
            (('volume', 'directions'), [[0, 0, 0], [0, 1, 0], [0, 0, 1]]),
            (('volume', 'directions'), [[1, 0, 0], [1, 1, 0], [0, 0, 1]]),
            (('volume', 'directions'), [[float('inf'), 0, 0], [0, 1, 0], [0, 0, 1]]),
            (('units',), 'm'),
            (('units_basis',), ''),
            (('sample_layout',), 'cell corners'),
        ]
        for keys, value in cases:
            with self.subTest(field=keys, value=value):
                manifest = json.loads(json.dumps(original))
                target = manifest
                for key in keys[:-1]:
                    target = target[key]
                target[keys[-1]] = value
                path.write_text(json.dumps(manifest))
                with self.assertRaises(ValueError):
                    suite.load_manifest(path)
                with self.assertRaises(ValueError):
                    suite.verify(path)

    def test_bounded_brick_decompression(self):
        path=self.packed();m=suite.load_manifest(path);b=m['bricks'][0]
        encoded=gzip.compress(b'\0'*(b['decoded_bytes']+100000),mtime=0)
        (path.parent/b['payload']).write_bytes(encoded)
        b['encoded_bytes']=len(encoded);b['sha256']=suite.digest(encoded)
        with self.assertRaisesRegex(ValueError,'compressed brick'):suite.read_brick(path.parent,b)


if __name__=='__main__':
    unittest.main()
