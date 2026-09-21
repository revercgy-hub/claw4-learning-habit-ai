"""Build the isolated M0 variant; never flashes or changes global environment."""
import argparse
import os
import subprocess
import sys
from pathlib import Path
from baseline import ROOT


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
    env = environment(args.idf.resolve(), args.tools.resolve())
    if args.incremental:
        if not (args.source / 'sdkconfig').is_file():
            parser.error('Incremental build requires an existing sdkconfig')
        command = [sys.executable, str(args.idf.resolve() / 'tools/idf.py'), 'build']
    else:
        command = [sys.executable, str(args.source.resolve() / 'scripts/build.py'),
                   'metalio/claw4-learning-v6', '--name', 'claw4-learning-v6-m0']
    result = subprocess.run(command, cwd=args.source, env=env)
    sys.exit(result.returncode)
