"""Build the isolated M0 variant; never flashes or changes global environment."""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from baseline import ROOT
from network_overlay import prepare

BOARD_OVERLAY = ROOT / 'integration/v6/board/claw4-learning-v6'
BOARD_RELATIVE = Path('main/boards/metalio/claw4-learning-v6')


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
    parser.add_argument('--idf', type=Path, default=ROOT / 'vendor/esp-idf')
    parser.add_argument('--tools', type=Path, default=ROOT / 'toolchains/idf61')
    args = parser.parse_args()
    sync_board(args.source.resolve())
    env = environment(args.idf.resolve(), args.tools.resolve())
    if args.incremental:
        if not (args.source / 'sdkconfig').is_file():
            parser.error('Incremental build requires an existing sdkconfig')
        prepare(args.source.resolve())
        command = [sys.executable, str(args.idf.resolve() / 'tools/idf.py'), 'reconfigure', 'build']
    else:
        command = [sys.executable, str(args.source.resolve() / 'scripts/build.py'),
                   'metalio/claw4-learning-v6', '--name', 'claw4-learning-v6-m0']
    result = subprocess.run(command, cwd=args.source, env=env)
    if result.returncode == 0 and not args.incremental:
        # Initial upstream build resolves/downloads the pinned managed package.
        # Only the rebuilt local override is eligible for candidate freezing.
        prepare(args.source.resolve())
        result = subprocess.run([sys.executable, str(args.idf.resolve() / 'tools/idf.py'),
                                 'reconfigure', 'build'], cwd=args.source, env=env)
    sys.exit(result.returncode)
