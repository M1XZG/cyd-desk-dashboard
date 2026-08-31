#!/usr/bin/env python3

import argparse
import hashlib
import json
import re
from pathlib import Path


VERSION_PATTERN = re.compile(
    r'^v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z]+(?:\.[0-9A-Za-z]+)*)?$'
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--firmware', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version-header', type=Path)
    args = parser.parse_args()

    if VERSION_PATTERN.fullmatch(args.version) is None:
        raise SystemExit(f'Invalid release version: {args.version}')
    if args.version_header is not None:
        header = args.version_header.read_text(encoding='utf-8')
        expected = f'kFirmwareVersion = "{args.version}"'
        if expected not in header:
            raise SystemExit(
                f'{args.version_header} does not declare {args.version}'
            )
    firmware = args.firmware.read_bytes()
    manifest = {
        'schema': 1,
        'version': args.version,
        'firmware': {
            'asset': 'firmware.bin',
            'size': len(firmware),
            'sha256': hashlib.sha256(firmware).hexdigest(),
        },
    }
    args.output.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + '\n',
        encoding='utf-8',
    )


if __name__ == '__main__':
    main()
