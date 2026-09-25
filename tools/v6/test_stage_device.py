import tempfile
import unittest
from pathlib import Path

from stage_device import (M1_ENDPOINT_PATCHES, NVS_ERASE_ANCHOR,
                          NVS_FAIL_CLOSED, PREFLIGHT_OTA_URL,
                          PREFLIGHT_WS_URL, patch_m1_endpoints, replace_once)
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

if __name__ == '__main__':
    unittest.main()
