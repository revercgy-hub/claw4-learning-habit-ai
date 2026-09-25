"""Build an isolated M0/M1 variant; never flash or change global environment."""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from baseline import ROOT
from network_overlay import prepare

BOARD_OVERLAY = ROOT / 'integration/v6/board/claw4-learning-v6'
BOARD_RELATIVE = Path('main/boards/metalio/claw4-learning-v6')


def expected_local_component_lock(reviewed, manifest_hash):
    """Derive the sole allowed IDF lock rewrite for our local Wi-Fi component."""
    try:
        start = reviewed.index('  78/esp-wifi-connect:\n')
        end = reviewed.index('  78/esp_lcd_nv3023:\n', start)
        if '    version: 3.3.1\n' not in reviewed[start:end]:
            raise ValueError('Reviewed Wi-Fi component version drift')
        reviewed = reviewed[:start] + (
            '  78/esp-wifi-connect:\n'
            '    dependencies: []\n'
            '    source:\n'
            '      path: components/78__esp-wifi-connect\n'
            '      type: local\n'
            '    version: 3.3.1\n') + reviewed[end:]
        start = reviewed.index('  espressif/cjson:\n')
        end = reviewed.index('  espressif/cmake_utilities:\n', start)
        cjson = reviewed[start:end]
        old = '      registry_url: https://components.espressif.com\n'
        if cjson.count(old) != 1:
            raise ValueError('Reviewed cjson lock anchor drift')
        reviewed = reviewed[:start] + cjson.replace(old, old.rstrip('\n') + '/\n') + reviewed[end:]
        direct = '- espressif/button\n'
        if reviewed.count(direct) != 1:
            raise ValueError('Reviewed direct-dependency lock anchor drift')
        reviewed = reviewed.replace(direct, direct + '- espressif/cjson\n')
        if len(re.findall(r'^manifest_hash: [0-9a-f]{64}$', reviewed, re.M)) != 1:
            raise ValueError('Reviewed manifest hash anchor drift')
        reviewed = re.sub(r'^manifest_hash: [0-9a-f]{64}$',
                          'manifest_hash: ' + manifest_hash, reviewed,
                          count=1, flags=re.M)
    except (ValueError, IndexError) as error:
        raise ValueError('Reviewed component lock anchor drift') from error
    return reviewed


def verify_component_lock(source, expected, local_override=False):
    """Allow only IDF's exact lock rewrite for the reviewed local Wi-Fi override."""
    actual = source / 'dependencies.lock'
    if not actual.is_file():
        raise ValueError('M1 component lock drift; candidate build is not eligible')
    reviewed = expected.read_text(encoding='utf-8')
    actual_text = actual.read_text(encoding='utf-8')
    if local_override:
        actual_manifest = re.search(r'^manifest_hash: ([0-9a-f]{64})$', actual_text, re.M)
        if actual_manifest is None:
            raise ValueError('M1 component lock manifest hash missing')
        reviewed = expected_local_component_lock(reviewed, actual_manifest.group(1))
    if actual_text != reviewed:
        raise ValueError('M1 component lock drift; candidate build is not eligible')


def sync_board(source, overlay=BOARD_OVERLAY):
    """Copy the authoritative board overlay into a staged source tree."""
    staged_board = source / BOARD_RELATIVE
    if not staged_board.is_dir():
        raise ValueError(f'Missing staged board directory: {staged_board}')

    expected = {path.relative_to(overlay).as_posix()
                for path in overlay.rglob('*') if path.is_file()}
    actual = {path.relative_to(staged_board).as_posix()
              for path in staged_board.rglob('*') if path.is_file()}
    if actual != expected:
        raise ValueError('Staged board inventory differs from authoritative overlay')

    for relative in sorted(expected):
        shutil.copyfile(overlay / relative, staged_board / relative)
    return len(expected)


def environment(idf, tools):
    env = os.environ.copy()
    env['IDF_PATH'] = str(idf)
    env['IDF_TOOLS_PATH'] = str(tools)
    env['PYTHONUTF8'] = '1'
    env['PYTHONIOENCODING'] = 'utf-8'
    exported = subprocess.check_output(
        [sys.executable, str(idf / 'tools/idf_tools.py'), 'export', '--format', 'key-value'],
        env=env, text=True)
    for line in exported.splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            if key == 'PATH':
                value = value.replace('%PATH%', env['PATH']).replace('$PATH', env['PATH'])
            env[key] = value
    return env


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--incremental', action='store_true',
                        help='Rebuild an already configured tree with idf.py build')
    parser.add_argument('--variant', choices=('m0', 'm1'), default='m0')
    parser.add_argument('--idf', type=Path, default=ROOT / 'vendor/esp-idf')
    parser.add_argument('--tools', type=Path, default=ROOT / 'toolchains/idf61')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'integration/v6/baseline.lock.json').read_text())
    expected_lock = ROOT / lock['device_component_lock']
    if args.variant == 'm1':
        verify_component_lock(args.source.resolve(), expected_lock,
                              local_override=args.incremental)
    sync_board(args.source.resolve())
    env = environment(args.idf.resolve(), args.tools.resolve())
    if args.incremental:
        if not (args.source / 'sdkconfig').is_file():
            parser.error('Incremental build requires an existing sdkconfig')
        prepare(args.source.resolve())
        command = [sys.executable, str(args.idf.resolve() / 'tools/idf.py'), 'reconfigure', 'build']
    else:
        command = [sys.executable, str(args.source.resolve() / 'scripts/build.py'),
                   'metalio/claw4-learning-v6', '--name',
                   f'claw4-learning-v6-{args.variant}']
    result = subprocess.run(command, cwd=args.source, env=env)
    if args.variant == 'm1':
        verify_component_lock(args.source.resolve(), expected_lock,
                              local_override=args.incremental)
    if result.returncode == 0 and not args.incremental:
        # Initial upstream build resolves/downloads the pinned managed package.
        # Only the rebuilt local override is eligible for candidate freezing.
        prepare(args.source.resolve())
        result = subprocess.run([sys.executable, str(args.idf.resolve() / 'tools/idf.py'),
                                 'reconfigure', 'build'], cwd=args.source, env=env)
        if args.variant == 'm1':
            verify_component_lock(args.source.resolve(), expected_lock, local_override=True)
    sys.exit(result.returncode)
