"""Verify the four existing project integration changes against fixed upstream.

Only --capture reads a mirror and writes repository patch evidence. Both modes
apply and reverse the patch in a newly created temporary fixture, never vendor.
"""
import argparse
import difflib
import hashlib
import json
import pathlib
import subprocess
import tempfile

PIN = 'ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950'
FILES = ('main/display/screen/home_screen/home_screen.cc', 'main/CMakeLists.txt',
         'main/display/lv_adapter_display.cc', 'main/audio/audio_service.cc')
ROOT = pathlib.Path(__file__).resolve().parents[3]
PATCH = ROOT / 'integration/metalio_claw4/patches/project-ca3aa3fa.patch'
META = PATCH.with_suffix('.json')


def git(*args, cwd=None):
    return subprocess.check_output(['git', '-c', 'core.autocrlf=false', *map(str, args)], cwd=cwd)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vendor', required=True)
    parser.add_argument('--capture', metavar='READ_ONLY_MIRROR')
    args = parser.parse_args()
    base = {p: git('-C', args.vendor, 'show', f'{PIN}:{p}') for p in FILES}
    if args.capture:
        chunks, rows = [], []
        for p in FILES:
            raw = (pathlib.Path(args.capture) / p).read_bytes()
            target = raw.replace(b'\r\n', b'\n')
            source = base[p].replace(b'\r\n', b'\n')
            diff = difflib.unified_diff(
                source.decode('utf-8').splitlines(True), target.decode('utf-8').splitlines(True),
                fromfile='a/' + p, tofile='b/' + p)
            chunks.append(''.join(line if line.endswith('\n') else
                                  line + '\n\\ No newline at end of file\n' for line in diff))
            rows.append(dict(path=p, upstream_sha256=sha(source),
                             mirror_raw_sha256=sha(raw), patched_lf_sha256=sha(target)))
        PATCH.parent.mkdir(parents=True, exist_ok=True)
        PATCH.write_bytes(''.join(chunks).encode('utf-8'))
        META.write_text(json.dumps(dict(upstream_commit=PIN, patch_sha256=sha(PATCH.read_bytes()),
                                       files=rows), indent=2) + '\n', encoding='utf-8')
    meta = json.loads(META.read_text(encoding='utf-8'))
    assert meta['upstream_commit'] == PIN
    assert meta['patch_sha256'] == sha(PATCH.read_bytes())
    assert [row['path'] for row in meta['files']] == list(FILES)
    with tempfile.TemporaryDirectory(prefix='claw4-patch-') as tmp:
        fixture = pathlib.Path(tmp)
        for row in meta['files']:
            p = row['path']
            dest = fixture / p
            dest.parent.mkdir(parents=True, exist_ok=True)
            data = base[p].replace(b'\r\n', b'\n')
            assert sha(data) == row['upstream_sha256']
            dest.write_bytes(data)
        git('apply', '--check', PATCH, cwd=fixture)
        git('apply', PATCH, cwd=fixture)
        for row in meta['files']:
            assert sha((fixture / row['path']).read_bytes()) == row['patched_lf_sha256'], row['path']
        git('apply', '--reverse', '--check', PATCH, cwd=fixture)
        git('apply', '--reverse', PATCH, cwd=fixture)
        for row in meta['files']:
            assert sha((fixture / row['path']).read_bytes()) == row['upstream_sha256']
    print(f'Metalio project patch: 4 files apply/hash/reverse PASS; SHA256={meta["patch_sha256"]}')


if __name__ == '__main__':
    main()
