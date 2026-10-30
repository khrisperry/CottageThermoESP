#!/usr/bin/env python3
"""Package the two explicitly selected ESP-IDF builds; never collect config/secrets."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEVICES = {
    'controller': ('controller/build-supermini', 'cottage_thermo_controller'),
    'thermostat': ('touchscreen/build-4b', 'cottage_thermo_room_4b'),
}


def package(device, build, output, version):
    app = DEVICES[device][1]
    relative_files = ['bootloader/bootloader.bin', 'partition_table/partition-table.bin',
                      app + '.bin', 'flash_args', 'flasher_args.json']
    contents = {name: (build / name).read_bytes() for name in relative_files}
    image = contents[app + '.bin']
    # ESP image header (24) + first segment header (8), then esp_app_desc_t.
    if len(image) < 80 or image[0] != 0xE9 or struct.unpack_from('<I', image, 32)[0] != 0xABCD5432:
        raise ValueError(f'{device}: missing ESP application descriptor')
    actual = image[48:80].split(b'\0', 1)[0].decode('ascii')
    if actual != version:
        raise ValueError(f'{device}: binary is {actual!r}; expected {version!r}. Rebuild first.')
    identity = image[80:112].split(b'\0', 1)[0].decode('ascii')
    if identity != app:
        raise ValueError(f'{device}: wrong application identity {identity!r}')
    revision = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    hashes = {name: hashlib.sha256(data).hexdigest() for name, data in contents.items()}
    contents['manifest.json'] = (json.dumps({'release': 'V' + version, 'device': device,
        'application': app, 'source_commit': revision, 'sha256': hashes}, indent=2) + '\n').encode()
    contents['SHA256SUMS'] = ''.join(f'{digest}  {name}\n' for name, digest in hashes.items()).encode()
    contents['FLASHING.txt'] = (f'CottageThermoESP V{version} / {device}\n'
        'Extract the complete archive. From this directory in an ESP-IDF terminal:\n'
        'python -m esptool --chip esp32s3 --port COMxx write-flash @flash_args\n'
        'Replace COMxx with this device port. Do not erase flash/NVS.\n'
        'Use the thermostat application .bin only for its webpage OTA update.\n'
        'Controller updates use USB. Local builds can contain private credentials.\n').encode()
    output.mkdir(parents=True, exist_ok=True)
    target = output / f'CottageThermoESP-V{version}-{device}.zip'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in contents.items():
            archive.writestr(name, data)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', choices=['both', *DEVICES], default='both')
    parser.add_argument('--build-dir', type=Path, help='Override build directory for one device')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist')
    args = parser.parse_args()
    if args.build_dir and args.device == 'both':
        parser.error('--build-dir requires one --device')
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        parser.error('VERSION must be major.minor.patch')
    for device in DEVICES if args.device == 'both' else [args.device]:
        build = args.build_dir or ROOT / DEVICES[device][0]
        print(package(device, build, args.output_dir, version))


if __name__ == '__main__':
    main()
