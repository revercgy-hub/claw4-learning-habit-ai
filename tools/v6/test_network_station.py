"""Compile actual patched station method bodies against a bounded host driver shim.

Explicit runner (not unittest discovery): --source <staged tree> --cxx <compiler>.
Requires the pinned managed dependency. No serial/network/device operations.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from baseline import ROOT
from network_overlay import SPEC, expected_files


def method(text, name):
    start = text.index('void WifiStation::' + name + '() {')
    end = text.index('\n}\n', start) + 3
    return text[start:end]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--cxx', type=Path, required=True)
    args = parser.parse_args()
    source = expected_files(args.source)['wifi_station.cc'].decode()
    fixture = (ROOT / 'tools/v6/network_station_fixture.cc').read_text()
    fixture = fixture.replace('// INSERT_PRODUCTION_METHODS',
                              method(source, 'HandleScanResult') + method(source, 'StartConnect'))
    env = os.environ.copy()
    env['PATH'] = str(args.cxx.parent) + os.pathsep + env['PATH']
    with tempfile.TemporaryDirectory(prefix='claw4-network-') as temp:
        cpp = Path(temp) / 'station.cc'
        exe = Path(temp) / 'station.exe'
        cpp.write_text(fixture, encoding='utf-8')
        subprocess.run([str(args.cxx), '-std=c++17', '-Wall', '-Wextra',
                        '-Wno-class-memaccess', '-Wno-missing-field-initializers',
                        '-I' + str(SPEC), str(cpp), '-o', str(exe)], check=True, env=env)
        subprocess.run([str(exe)], check=True, env=env)
    print('PASS: 7 production-method host scenarios; no hardware claims.')
