"""Version-locked local component override; never edits managed_components.

The original full inventory must match the recorded package. An existing local
override must equal the generated output exactly; unknown edits fail closed.
"""
import hashlib
import json
from pathlib import Path
from baseline import ROOT

PACKAGE = '78__esp-wifi-connect'
SPEC = ROOT / 'integration/v6/network'


def inventory(directory):
    return {p.relative_to(directory).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}


def expected_files(source):
    original = source / 'managed_components' / PACKAGE
    spec = json.loads((SPEC / 'overlay.json').read_text(encoding='utf-8'))
    if inventory(original) != spec['original_sha256']:
        raise ValueError('WiFi dependency differs from pinned full inventory')
    files = {p: (original / p).read_bytes() for p in spec['original_sha256']}
    # Registry checksums describe the original package, not this local override.
    for name in ['.component_hash', 'CHECKSUMS.json']:
        files.pop(name, None)
    for name, changes in spec['replacements'].items():
        content = files[name].decode('utf-8').replace('\r\n', '\n')
        for old, new in changes:
            if content.count(old) != 1:
                raise ValueError(f'WiFi source anchor drift: {name}')
            content = content.replace(old, new)
        files[name] = content.encode('utf-8')
    files['include/saved_network_policy.h'] = (SPEC / 'saved_network_policy.h').read_bytes()
    return files


def verify(source):
    expected = {p: hashlib.sha256(data).hexdigest() for p, data in expected_files(source).items()}
    if inventory(source / 'components' / PACKAGE) != expected:
        raise ValueError('Local WiFi override missing, stale or modified')
    return expected


def prepare(source):
    files = expected_files(source)
    target = source / 'components' / PACKAGE
    if target.exists():
        return verify(source)
    target.mkdir(parents=True)
    for name, data in files.items():
        path = target / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return verify(source)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source.resolve())
    print('Local WiFi override prepared and verified; managed package unchanged.')
