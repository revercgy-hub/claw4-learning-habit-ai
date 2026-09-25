"""Create a fresh, exact-SHA XiaoZhi build tree with the reviewed Claw4 overlay."""
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path
from baseline import ROOT, verify_checkout

NVS_ERASE_ANCHOR = '''    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);'''

NVS_FAIL_CLOSED = '''    // Preserve live NVS on initialization errors; recovery needs separate review.
    esp_err_t ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);'''


def replace_once(path, old, new):
    source = path.read_text(encoding='utf-8')
    if source.count(old) != 1:
        raise ValueError(f'Upstream anchor drift: {path}: {old[:60]}')
    path.write_text(source.replace(old, new), encoding='utf-8', newline='\n')


def stage(upstream, destination, variant='m0'):
    if variant not in ('m0', 'm1'):
        raise ValueError(f'Unknown build variant: {variant}')
    lock = json.loads((ROOT / 'integration/v6/baseline.lock.json').read_text())
    pin = lock['sources']['xiaozhi']['sha']
    verify_checkout(upstream, pin)
    if destination.exists():
        raise ValueError('Destination must be new; existing trees are never overwritten')
    destination.mkdir(parents=True)
    with tempfile.TemporaryDirectory() as temporary:
        archive = Path(temporary) / 'source.zip'
        subprocess.run(['git', '-C', str(upstream), 'archive', '--format=zip',
                        f'--output={archive}', pin], check=True)
        with zipfile.ZipFile(archive) as source:
            source.extractall(destination)
    if variant == 'm1':
        replace_once(destination / 'main/main.cc', NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED)
        # Seed the reviewed component versions before IDF can resolve ranges.
        shutil.copy2(ROOT / lock['device_component_lock'], destination / 'dependencies.lock')
    board = ROOT / 'integration/v6/board/claw4-learning-v6'
    shutil.copytree(board, destination / 'main/boards/metalio/claw4-learning-v6')
    partition_name = 'claw4-live-m1.csv' if variant == 'm1' else 'claw4-v6.csv'
    shutil.copy2(ROOT / 'integration/v6/board' / partition_name,
                 destination / 'partitions' / partition_name)
    replace_once(destination / 'main/Kconfig.projbuild',
                 '    config BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD',
                 '    config BOARD_TYPE_CLAW4_LEARNING_V6\n'
                 '        bool "Metalio Claw4 Learning V6"\n'
                 '        depends on IDF_TARGET_ESP32P4\n'
                 '    config BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD')
    with (destination / 'main/Kconfig.projbuild').open('a', encoding='utf-8') as output:
        output.write('\nconfig CLAW4_M0_DIAGNOSTICS\n'
                     '    bool "Claw4 local hardware harness (no cloud)"\n'
                     '    depends on BOARD_TYPE_CLAW4_LEARNING_V6\n'
                     '    default y\n')
    replace_once(destination / 'main/Kconfig.projbuild',
                 'depends on USE_AUDIO_PROCESSOR && (BOARD_TYPE_ESP32_S3_BOX_3',
                 'depends on USE_AUDIO_PROCESSOR && (BOARD_TYPE_CLAW4_LEARNING_V6 || BOARD_TYPE_ESP32_S3_BOX_3')
    cmake = destination / 'main/CMakeLists.txt'
    replace_once(cmake, 'elseif(CONFIG_BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD)',
                 'elseif(CONFIG_BOARD_TYPE_CLAW4_LEARNING_V6)\n'
                 '    set(BOARD_DIR "metalio/claw4-learning-v6")\n'
                 'elseif(CONFIG_BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD)')
    replace_once(cmake, 'list(APPEND SOURCES ${BOARD_SOURCES})',
                 'list(APPEND SOURCES ${BOARD_SOURCES})\n'
                 'if(CONFIG_CLAW4_M0_DIAGNOSTICS)\n'
                 '    list(REMOVE_ITEM SOURCES "main.cc")\n'
                 'endif()')
    # CMake identity is recorded separately; do not masquerade as upstream releases.
    replace_once(destination / 'CMakeLists.txt', 'set(PROJECT_VER "2.5.0")',
                 f'set(PROJECT_VER "6.0.0-{variant}.1")')
    entries = {}
    for path in sorted(board.rglob('*')):
        if path.is_file():
            entries[path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = {'upstream_sha': pin, 'overlay_sha256': entries,
                'partition_csv': f'partitions/{partition_name}',
                'learning_linked': False, 'diagnostics': variant == 'm0',
                'variant': variant,
                'nvs_init_policy': 'fail_closed_no_erase' if variant == 'm1' else 'upstream',
                'component_lock_sha256': hashlib.sha256((destination / 'dependencies.lock').read_bytes()).hexdigest()
                    if variant == 'm1' else None}
    (destination / 'v6-stage-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--variant', choices=('m0', 'm1'), default='m0')
    args = parser.parse_args()
    stage(args.upstream.resolve(), args.destination.resolve(), args.variant)
