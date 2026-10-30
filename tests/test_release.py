"""Release packaging rejects stale/wrong firmware and excludes local credentials."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_package', ROOT / 'tools/package-release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleasePackaging(unittest.TestCase):
    def test_version_identity_and_file_boundaries(self):
        version = (ROOT / 'VERSION').read_text().strip()
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            for device, (_, identity) in release.DEVICES.items():
                build = base / device
                (build / 'bootloader').mkdir(parents=True)
                (build / 'partition_table').mkdir()
                for relative in ['bootloader/bootloader.bin', 'partition_table/partition-table.bin',
                                 'flash_args', 'flasher_args.json']:
                    (build / relative).write_bytes(b'fixture')
                (build / 'secrets.h').write_text('PRIVATE-DO-NOT-PACKAGE')
                (build / 'sdkconfig').write_text('PRIVATE-DO-NOT-PACKAGE')
                image = bytearray(256)
                image[0] = 0xE9
                struct.pack_into('<I', image, 32, 0xABCD5432)
                image[48:48+len(version)] = version.encode()
                image[80:80+len(identity)] = identity.encode()
                app = build / (identity + '.bin')
                app.write_bytes(image)
                target = release.package(device, build, base / 'dist', version)
                with zipfile.ZipFile(target) as archive:
                    self.assertNotIn('secrets.h', archive.namelist())
                    self.assertNotIn('sdkconfig', archive.namelist())
                    manifest = json.loads(archive.read('manifest.json'))
                    for name, digest in manifest['sha256'].items():
                        self.assertEqual(release.hashlib.sha256(archive.read(name)).hexdigest(), digest)
                image[48:80] = b'0.0.0' + bytes(27)
                app.write_bytes(image)
                with self.assertRaisesRegex(ValueError, 'Rebuild first'):
                    release.package(device, build, base / 'dist', version)
                image[48:80] = version.encode().ljust(32, b'\0')
                image[80:112] = b'wrong-board'.ljust(32, b'\0')
                app.write_bytes(image)
                with self.assertRaisesRegex(ValueError, 'wrong application identity'):
                    release.package(device, build, base / 'dist', version)
                app.unlink()
                with self.assertRaises(FileNotFoundError):
                    release.package(device, build, base / 'dist', version)


if __name__ == '__main__':
    unittest.main()
