"""Freeze source/config/artifact evidence after a successful M0 build."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from baseline import ROOT
from network_overlay import PACKAGE, verify


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def freeze(source, output):
    network_files = verify(source)
    description = json.loads((source / 'build/project_description.json').read_text(encoding='utf-8'))
    component = description['build_component_info'][PACKAGE]
    if Path(component['dir']).resolve() != (source / 'components' / PACKAGE).resolve():
        raise ValueError('Build did not select the reviewed local WiFi override')
    board = ROOT / 'integration/v6/board/claw4-learning-v6'
    staged_board = source / 'main/boards/metalio/claw4-learning-v6'
    expected = {p.relative_to(board).as_posix() for p in board.rglob('*') if p.is_file()}
    actual = {p.relative_to(staged_board).as_posix() for p in staged_board.rglob('*') if p.is_file()}
    if actual != expected:
        raise ValueError('Staged board inventory differs from authoritative overlay')
    for relative in expected:
        if digest(board / relative) != digest(staged_board / relative):
            raise ValueError(f'Stale staged board: {relative}')
    upstream = ROOT / 'vendor/xiaozhi-esp32'
    unchanged = ['main/application.cc', 'main/application.h', 'main/main.cc',
                 'main/device_state_machine.cc', 'main/audio/audio_service.cc',
                 'main/audio/engines/afe_audio_engine.cc', 'main/protocols/protocol.cc',
                 'main/protocols/websocket_protocol.cc', 'main/protocols/mqtt_protocol.cc']
    for relative in unchanged:
        original = subprocess.check_output(['git', '-C', str(upstream), 'show', f'HEAD:{relative}'])
        # Windows archives/checkouts may use CRLF; tolerate only that encoding
        # difference, while still rejecting every source-content change.
        if (source / relative).read_bytes().replace(b'\r\n', b'\n') != original.replace(b'\r\n', b'\n'):
            raise ValueError(f'Unexpected upstream runtime edit: {relative}')
    config = (source / 'sdkconfig').read_text(encoding='utf-8')
    required = ['CONFIG_CLAW4_M0_DIAGNOSTICS=y', 'CONFIG_BOARD_TYPE_CLAW4_LEARNING_V6=y',
                'CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y', 'CONFIG_USE_DEVICE_AEC=y',
                'CONFIG_SLAVE_IDF_TARGET_ESP32C5=y', 'CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y',
                'CONFIG_PARTITION_TABLE_OFFSET=0x9000', 'CONFIG_ESPTOOLPY_FLASHSIZE="32MB"',
                'CONFIG_ESPTOOLPY_FLASHMODE="dio"', 'CONFIG_ESPTOOLPY_FLASHFREQ="40m"',
                'CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y',
                'CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH=y']
    for setting in required:
        if setting not in config.splitlines():
            raise ValueError(f'Unapplied critical config: {setting}')
    files = ['sdkconfig', 'dependencies.lock', 'main/Kconfig.projbuild', 'main/CMakeLists.txt',
             'CMakeLists.txt', 'partitions/claw4-v6.csv', 'build/xiaozhi.bin', 'build/xiaozhi.elf',
             'build/bootloader/bootloader.bin', 'build/partition_table/partition-table.bin',
             'build/srmodels/srmodels.bin', 'build/generated_assets.bin', 'build/flasher_args.json']
    manifest = {'candidate': 'claw4-learning-v6-m0.1',
                'network_override_sha256': network_files,
                'network_component_directory': component['dir'],
                'upstream_runtime_unchanged': unchanged,
                'runtime_comparison': 'exact bytes after CRLF-to-LF normalization',
                'overlay_sha256': {p: digest(board / p) for p in sorted(expected)},
                'files': {p: {'sha256': digest(source / p), 'bytes': (source / p).stat().st_size}
                          for p in files},
                'device_validation': 'NOT_YET_RUN'}
    output.write_text(json.dumps(manifest, indent=2), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    freeze(args.source.resolve(), args.output)
