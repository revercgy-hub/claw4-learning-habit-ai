"""Fault-inject the two actual board camera methods in an isolated host shim."""
import os
from pathlib import Path
import subprocess
import unittest

from baseline import ROOT


BOARD = ROOT / 'integration/v6/board/claw4-learning-v6/claw4_board.cc'
FIXTURE = ROOT / 'tools/v6/camera_cleanup_fixture.cc'
OUT = ROOT / 'out/s02'
CXX = Path(os.environ.get('CLAW4_S02_CXX',
                          'E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe'))


def production_method(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == '{':
            depth += 1
        elif source[pos] == '}':
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError(f'unclosed production method: {signature}')


class CameraCleanupTest(unittest.TestCase):
    def test_real_camera_methods_with_faults(self):
        self.assertTrue(CXX.is_file(), f'C++ compiler missing: {CXX}')
        source = BOARD.read_text(encoding='utf-8')
        methods = []
        for signature in ('    bool CaptureSingleCameraFrame() {',
                          '    void ProbeCameraSensor() {'):
            extracted = production_method(source, signature).strip()
            methods.append(extracted)
        # Qualify only the extracted production signatures for out-of-class use.
        methods[0] = methods[0].replace('bool CaptureSingleCameraFrame()',
                                        'bool Claw4Board::CaptureSingleCameraFrame()', 1)
        methods[1] = methods[1].replace('void ProbeCameraSensor()',
                                        'void Claw4Board::ProbeCameraSensor()', 1)
        fixture = FIXTURE.read_text(encoding='utf-8')
        self.assertIn('// INSERT_PRODUCTION_METHODS', fixture)
        OUT.mkdir(parents=True, exist_ok=True)
        cpp = OUT / 'camera_cleanup_generated.cc'
        exe = OUT / 'camera_cleanup_host.exe'
        cpp.write_text(fixture.replace('// INSERT_PRODUCTION_METHODS',
                                       '\n\n'.join(methods)), encoding='utf-8')
        env = os.environ.copy()
        env['PATH'] = str(CXX.parent) + os.pathsep + env['PATH']
        compile_result = subprocess.run(
            [str(CXX), '-std=c++17', '-Wall', '-Wextra', '-Werror',
             '-Wno-unused-parameter', str(cpp), '-o', str(exe)],
            capture_output=True, text=True, env=env)
        self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
        run_result = subprocess.run([str(exe)], capture_output=True, text=True,
                                    env=env)
        self.assertEqual(run_result.returncode, 0,
                         run_result.stdout + run_result.stderr)
        self.assertIn('PASS: 27 production camera cleanup scenarios',
                      run_result.stdout)
        (OUT / 'camera_scenarios.log').write_text(run_result.stdout,
                                                  encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
