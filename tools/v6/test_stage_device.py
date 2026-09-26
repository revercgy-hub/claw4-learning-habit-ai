import tempfile
import unittest
import hashlib
import json
import shutil
import sys
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import stage_device
from stage_device import (M1_ENDPOINT_PATCHES, NVS_ERASE_ANCHOR,
                          NVS_FAIL_CLOSED, PREFLIGHT_OTA_URL,
                          PREFLIGHT_WS_URL, patch_m1_endpoints, replace_once,
                          stage)
from build_device import verify_m1_endpoint_sources


class NvsPolicyPatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / 'main.cc'

    def test_exact_upstream_source_fails_closed_without_erase(self):
        original = '#include <nvs_flash.h>\nvoid app_main() {\n' + NVS_ERASE_ANCHOR + '\n}\n'
        self.source.write_text(original, encoding='utf-8')
        replace_once(self.source, NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED)
        result = self.source.read_text(encoding='utf-8')
        self.assertEqual(result, original.replace(NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED))
        self.assertNotIn('nvs_flash_erase(', result)
        self.assertIn('ESP_ERROR_CHECK(ret);', result)

    def test_anchor_drift_refuses_without_writing(self):
        drifted = NVS_ERASE_ANCHOR.replace('ESP_ERR_NVS_NEW_VERSION_FOUND', 'ESP_ERR_NVS_UNKNOWN')
        self.source.write_text(drifted, encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Upstream anchor drift'):
            replace_once(self.source, NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED)
        self.assertEqual(self.source.read_text(encoding='utf-8'), drifted)

    def test_ambiguous_anchor_refuses_without_writing(self):
        duplicate = NVS_ERASE_ANCHOR + '\n' + NVS_ERASE_ANCHOR
        self.source.write_text(duplicate, encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Upstream anchor drift'):
            replace_once(self.source, NVS_ERASE_ANCHOR, NVS_FAIL_CLOSED)
        self.assertEqual(self.source.read_text(encoding='utf-8'), duplicate)


class EndpointPatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.stage = Path(self.temp.name)
        self.write_originals()

    def write_originals(self):
        for relative, edits in M1_ENDPOINT_PATCHES.items():
            path = self.stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            voice_paths = ('\nprotocol_->OnIncomingAudio('
                           '\naudio_service_.PushPacketToDecodeQueue('
                           '\nstrcmp(type->valuestring, "tts")'
                           '\nBeginWakeWordInvoke(wake_word)') if relative == 'main/application.cc' else ''
            path.write_text('\n'.join(old for old, _ in edits) + voice_paths, encoding='utf-8')

    def test_m1_fixed_urls_and_no_external_protocol_or_upgrade(self):
        patch_m1_endpoints(self.stage)
        verify_m1_endpoint_sources(self.stage)
        ota = (self.stage / 'main/ota.cc').read_text(encoding='utf-8')
        app = (self.stage / 'main/application.cc').read_text(encoding='utf-8')
        ws = (self.stage / 'main/protocols/websocket_protocol.cc').read_text(encoding='utf-8')
        self.assertIn(PREFLIGHT_OTA_URL, ota)
        self.assertIn(PREFLIGHT_WS_URL, ota)
        self.assertIn(PREFLIGHT_WS_URL, ws)
        self.assertNotIn('settings.GetString("ota_url")', ota)
        self.assertNotIn('Settings settings("mqtt", true)', ota)
        self.assertNotIn('std::make_unique<MqttProtocol>()', app)
        self.assertNotIn('CheckAssetsVersion();', app)
        self.assertNotIn('CheckNewVersion();', app)
        self.assertNotIn('mcp_server.AddUserOnlyTools();', app)
        self.assertNotIn('StartNotification(std::move(url)', app)
        self.assertIn('mcp_server.AddCommonTools();', app)
        self.assertIn('protocol_ = std::make_unique<WebsocketProtocol>();', app)
        self.assertIn('return false;\n    auto& board = Board::GetInstance();', app)

    def test_each_drift_and_duplicate_anchor_refuses(self):
        for relative, edits in M1_ENDPOINT_PATCHES.items():
            for old, _ in edits:
                with self.subTest(relative=relative, anchor=old[:24]):
                    self.write_originals()
                    path = self.stage / relative
                    original = path.read_text(encoding='utf-8')
                    path.write_text(original.replace(old, 'drift', 1), encoding='utf-8')
                    with self.assertRaisesRegex(ValueError, 'anchor drift'):
                        patch_m1_endpoints(self.stage)
                    self.write_originals()
                    original = path.read_text(encoding='utf-8')
                    path.write_text(original.replace(old, old + '\n' + old, 1), encoding='utf-8')
                    with self.assertRaisesRegex(ValueError, 'anchor drift'):
                        patch_m1_endpoints(self.stage)

    def test_build_guard_rejects_changed_url_and_config(self):
        patch_m1_endpoints(self.stage)
        ota_path = self.stage / 'main/ota.cc'
        original = ota_path.read_text(encoding='utf-8')
        ota_path.write_text(original.replace(PREFLIGHT_OTA_URL, 'http://example.invalid/', 1), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'endpoint guard drift'):
            verify_m1_endpoint_sources(self.stage)
        ota_path.write_text(original, encoding='utf-8')
        (self.stage / 'sdkconfig').write_text('CONFIG_OTA_URL="https://api.tenclass.net/xiaozhi/ota/"\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'compiled OTA URL drift'):
            verify_m1_endpoint_sources(self.stage)


class M1InputDiagnosticsStageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'root'
        self.root.mkdir()
        board = Path(__file__).resolve().parents[2] / 'integration/v6/board'
        shutil.copytree(board / 'claw4-learning-v6',
                        self.root / 'integration/v6/board/claw4-learning-v6')
        for name in ('claw4-v6.csv', 'claw4-live-m1.csv'):
            shutil.copy2(board / name, self.root / 'integration/v6/board' / name)
        lock = self.root / 'integration/v6/idf61-components.lock'
        lock.write_text('reviewed component lock\n', encoding='utf-8')
        (self.root / 'integration/v6/baseline.lock.json').write_text(json.dumps({
            'sources': {'xiaozhi': {'sha': 'reviewed-test-sha'}},
            'device_component_lock': 'integration/v6/idf61-components.lock',
        }), encoding='utf-8')

        app = '\n'.join(old for old, _ in M1_ENDPOINT_PATCHES['main/application.cc'])
        self.source = {
            'main/main.cc': NVS_ERASE_ANCHOR,
            'main/Kconfig.projbuild': (
                '    config BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD\n'
                'depends on USE_AUDIO_PROCESSOR && (BOARD_TYPE_ESP32_S3_BOX_3'),
            'main/CMakeLists.txt': (
                'elseif(CONFIG_BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD)\n'
                'list(APPEND SOURCES ${BOARD_SOURCES})'),
            'CMakeLists.txt': 'set(PROJECT_VER "2.5.0")',
            'main/application.cc': app,
            'main/ota.cc': '\n'.join(old for old, _ in M1_ENDPOINT_PATCHES['main/ota.cc']),
            'main/protocols/websocket_protocol.cc': '\n'.join(
                old for old, _ in M1_ENDPOINT_PATCHES['main/protocols/websocket_protocol.cc']),
            'partitions/.keep': '',
        }

    def stage_variant(self, variant, enabled=False):
        destination = Path(self.temp.name) / f'{variant}-{enabled}'

        def make_archive(command, check):
            archive = Path(next(arg.split('=', 1)[1] for arg in command
                                if arg.startswith('--output=')))
            with zipfile.ZipFile(archive, 'w') as output:
                for relative, contents in self.source.items():
                    output.writestr(relative, contents)

        with patch.object(stage_device, 'ROOT', self.root), \
                patch.object(stage_device, 'verify_checkout'), \
                patch.object(stage_device.subprocess, 'run', side_effect=make_archive):
            stage(Path(self.temp.name), destination, variant, enabled)
        return destination

    def test_m0_stage_remains_independent_and_rejects_m1_opt_in(self):
        destination = self.stage_variant('m0')
        kconfig = (destination / 'main/Kconfig.projbuild').read_text(encoding='utf-8')
        manifest = json.loads((destination / 'v6-stage-manifest.json').read_text())
        self.assertNotIn('CLAW4_M1_INPUT_DIAGNOSTICS', kconfig)
        self.assertTrue(manifest['diagnostics'])
        self.assertNotIn('m1_input_diagnostics', manifest)
        with self.assertRaisesRegex(ValueError, 'require the M1 variant'):
            stage(Path(self.temp.name), Path(self.temp.name) / 'rejected', 'm0', True)
        self.assertFalse((Path(self.temp.name) / 'rejected').exists())

    def test_default_m1_is_off_and_opt_in_binds_manifest_without_m0_harness(self):
        ordinary = self.stage_variant('m1')
        diagnostic = self.stage_variant('m1', True)
        board_rel = 'main/boards/metalio/claw4-learning-v6/config.json'
        self.assertEqual((ordinary / board_rel).read_bytes(),
                         (diagnostic / board_rel).read_bytes())
        for destination, enabled, default in ((ordinary, False, 'n'),
                                               (diagnostic, True, 'y')):
            kconfig_path = destination / 'main/Kconfig.projbuild'
            kconfig = kconfig_path.read_text(encoding='utf-8')
            config = json.loads((destination / board_rel).read_text(encoding='utf-8'))
            profiles = {item['name']: item['sdkconfig_append'] for item in config['builds']}
            manifest = json.loads((destination / 'v6-stage-manifest.json').read_text())
            self.assertIn('CONFIG_CLAW4_M0_DIAGNOSTICS=n',
                          profiles['claw4-learning-v6-m1'])
            self.assertIn('CONFIG_CLAW4_M0_DIAGNOSTICS=y',
                          profiles['claw4-learning-v6-m0'])
            self.assertIn('depends on BOARD_TYPE_CLAW4_LEARNING_V6 && !CLAW4_M0_DIAGNOSTICS',
                          kconfig)
            self.assertIn(f'    default {default}\n',
                          kconfig.split('config CLAW4_M1_INPUT_DIAGNOSTICS', 1)[1])
            self.assertFalse(manifest['diagnostics'])
            self.assertEqual(manifest['m1_input_diagnostics'], enabled)
            self.assertEqual(manifest['staged_kconfig_sha256'],
                             hashlib.sha256(kconfig_path.read_bytes()).hexdigest())
            self.assertIn('list(REMOVE_ITEM SOURCES "main.cc")',
                          (destination / 'main/CMakeLists.txt').read_text(encoding='utf-8'))

if __name__ == '__main__':
    unittest.main()
