import tempfile
import unittest
import hashlib
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import stage_device
from stage_device import (M1_AFE_DIAG_PATCHES, M1_ENDPOINT_PATCHES, NVS_ERASE_ANCHOR,
                          NVS_FAIL_CLOSED, PREFLIGHT_OTA_URL,
                          PREFLIGHT_WS_URL, patch_m1_afe_runtime_diagnostics,
                          patch_m1_endpoints, replace_once, stage)
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
        for relative, edits in M1_AFE_DIAG_PATCHES.items():
            self.source[relative] = '\n'.join(old for old, _ in edits)

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
        for relative in M1_AFE_DIAG_PATCHES:
            self.assertEqual((destination / relative).read_text(encoding='utf-8'),
                             self.source[relative])
        with self.assertRaisesRegex(ValueError, 'require the M1 variant'):
            stage(Path(self.temp.name), Path(self.temp.name) / 'rejected', 'm0', True)
        self.assertFalse((Path(self.temp.name) / 'rejected').exists())

    def test_default_m1_is_off_and_opt_in_binds_manifest_without_m0_harness(self):
        ordinary = self.stage_variant('m1')
        diagnostic = self.stage_variant('m1', True)
        for relative in M1_AFE_DIAG_PATCHES:
            self.assertEqual((ordinary / relative).read_text(encoding='utf-8'),
                             self.source[relative])
        afe = (diagnostic / 'main/audio/engines/afe_audio_engine.cc').read_text(
            encoding='utf-8')
        header = (diagnostic / 'main/audio/engines/afe_audio_engine.h').read_text(
            encoding='utf-8')
        for marker in ('M1_AFE_STATE', 'M1_AFE_FLOW', 'M1_AFE_CTL'):
            match = re.search(r'"(' + marker + r' [^"\n]+)"', afe)
            self.assertIsNotNone(match)
            # Reserve 60 bytes for the ESP log prefix within the parser's
            # 256-character line cap, including INT32_MIN return codes.
            worst = match.group(1).replace('%u', '4294967295').replace('%d', '-2147483648')
            self.assertLessEqual(len(worst) + 60, 256)
        self.assertIn('"m1_afe_diag", 4096, m1_diag_, 1, nullptr)', afe)
        self.assertIn('ReleaseM1AfeDiagState(state);\n                vTaskDelete(nullptr);', afe)
        self.assertNotIn('xSemaphoreTake', afe)
        self.assertNotIn('portMAX_DELAY', afe)
        self.assertNotIn('esp_timer_', afe)
        self.assertIn('std::memory_order_relaxed', afe)
        self.assertIn('M1AfeDiagState* m1_diag_', header)
        self.assertIn('std::atomic<uint32_t> feed_chunks{0}', afe)
        self.assertIn('std::atomic<uint32_t> fetch_other{0}', afe)
        self.assertIn('if (result == nullptr || result->ret_value == ESP_FAIL)', afe)
        self.assertIn('} else if (result->ret_value == ESP_OK) {', afe)
        self.assertIn('m1_diag_->fetch_other.fetch_add(1, std::memory_order_relaxed)', afe)
        self.assertIn('const int rc = afe_iface_->enable_wakenet(afe_data_);', afe)
        self.assertIn('const int rc = afe_iface_->disable_wakenet(afe_data_);', afe)
        self.assertIn('m1_diag_->wakenet_enable_rc.store(rc, std::memory_order_relaxed);', afe)
        self.assertIn('m1_diag_->wakenet_disable_rc.store(rc, std::memory_order_relaxed);', afe)
        report = afe.split('void AfeAudioEngine::ReportM1AfeStats', 1)[1].split(
            'void AfeAudioEngine::ProcessingTask()', 1)[0]
        for name in ('wakenet_enable_total', 'wakenet_disable_total',
                     'wakenet_enable_rc', 'wakenet_disable_rc'):
            self.assertIn(f'state->{name}.load(std::memory_order_relaxed)', report)
            self.assertNotIn(f'state->{name}.exchange(', report)
        self.assertIn('M1_AFE_CTL on_total=%u on_rc=%d off_total=%u off_rc=%d', report)
        self.assertNotIn('wake_applied_seq', afe)
        self.assertNotIn('wake_desired_seq', afe)
        self.assertNotIn('M1_AFE_STATE', (ordinary / 'main/audio/engines/afe_audio_engine.cc').read_text(
            encoding='utf-8'))
        self.assertNotIn('M1_AFE_CTL', (ordinary / 'main/audio/engines/afe_audio_engine.cc').read_text(
            encoding='utf-8'))
        staged_afe = {relative: (diagnostic / relative).read_bytes()
                      for relative in M1_AFE_DIAG_PATCHES}
        patch_m1_afe_runtime_diagnostics(diagnostic)
        self.assertEqual(staged_afe, {relative: (diagnostic / relative).read_bytes()
                                      for relative in M1_AFE_DIAG_PATCHES})
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

    def test_control_result_and_total_follow_each_afe_call(self):
        afe = (self.stage_variant('m1', True) /
               'main/audio/engines/afe_audio_engine.cc').read_text(encoding='utf-8')
        # A request may race with the event-bit snapshot or WakeNet may auto-disable.
        # Only completed AFE calls and their raw return codes may be reported;
        # no desired/applied association is sound across those interleavings.
        self.assertNotIn('wake_applied_seq', afe)
        self.assertNotIn('wake_desired_seq', afe)
        for verb in ('enable', 'disable'):
            with self.subTest(verb=verb):
                call = f'const int rc = afe_iface_->{verb}_wakenet(afe_data_);'
                result = f'm1_diag_->wakenet_{verb}_rc.store(rc, std::memory_order_relaxed);'
                total = (f'm1_diag_->wakenet_{verb}_total.fetch_add('
                         '1, std::memory_order_relaxed);')
                self.assertEqual(afe.count(call), 1)
                self.assertEqual(afe.count(result), 1)
                self.assertEqual(afe.count(total), 1)
                self.assertLess(afe.index(call), afe.index(result))
                self.assertLess(afe.index(result), afe.index(total))

    def test_afe_patch_idempotent_and_drift_refuses_all_writes(self):
        destination = Path(self.temp.name) / 'afe-only'
        for relative in M1_AFE_DIAG_PATCHES:
            path = destination / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(self.source[relative], encoding='utf-8')
        patch_m1_afe_runtime_diagnostics(destination)
        once = {relative: (destination / relative).read_bytes()
                for relative in M1_AFE_DIAG_PATCHES}
        patch_m1_afe_runtime_diagnostics(destination)
        self.assertEqual(once, {relative: (destination / relative).read_bytes()
                                for relative in M1_AFE_DIAG_PATCHES})

        source_path = destination / 'main/audio/engines/afe_audio_engine.cc'
        source_path.write_text(self.source['main/audio/engines/afe_audio_engine.cc'].replace(
            'AFE fetch failed:', 'AFE fetch changed:'), encoding='utf-8')
        header_path = destination / 'main/audio/engines/afe_audio_engine.h'
        header_path.write_text(self.source['main/audio/engines/afe_audio_engine.h'],
                               encoding='utf-8')
        before = {relative: (destination / relative).read_bytes()
                  for relative in M1_AFE_DIAG_PATCHES}
        with self.assertRaisesRegex(ValueError, 'anchor drift'):
            patch_m1_afe_runtime_diagnostics(destination)
        self.assertEqual(before, {relative: (destination / relative).read_bytes()
                                  for relative in M1_AFE_DIAG_PATCHES})

        source_path.write_text(self.source['main/audio/engines/afe_audio_engine.cc'] +
                               '\n' + M1_AFE_DIAG_PATCHES[
                                   'main/audio/engines/afe_audio_engine.cc'][0][0],
                               encoding='utf-8')
        before = {relative: (destination / relative).read_bytes()
                  for relative in M1_AFE_DIAG_PATCHES}
        with self.assertRaisesRegex(ValueError, 'anchor drift'):
            patch_m1_afe_runtime_diagnostics(destination)
        self.assertEqual(before, {relative: (destination / relative).read_bytes()
                                  for relative in M1_AFE_DIAG_PATCHES})

        source_path.write_text(self.source['main/audio/engines/afe_audio_engine.cc'],
                               encoding='utf-8')
        header_path.write_bytes(once['main/audio/engines/afe_audio_engine.h'])
        with self.assertRaisesRegex(ValueError, 'Partial AFE diagnostic patch'):
            patch_m1_afe_runtime_diagnostics(destination)

    def test_afe_reporter_owns_state_without_destructor_wait(self):
        diagnostic = self.stage_variant('m1', True)
        afe = (diagnostic / 'main/audio/engines/afe_audio_engine.cc').read_text(
            encoding='utf-8')
        self.assertIn('std::atomic<uint32_t> references{1};', afe)
        self.assertIn('m1_diag_->references.fetch_add(1, std::memory_order_relaxed);', afe)
        self.assertIn('if (created != pdPASS) {\n'
                      '            ReleaseM1AfeDiagState(m1_diag_);  // Task was not created.\n'
                      '            ReleaseM1AfeDiagState(m1_diag_);  // Engine reference.\n'
                      '            m1_diag_ = nullptr;', afe)
        self.assertIn('m1_diag_->stop.store(true, std::memory_order_release);\n'
                      '        ReleaseM1AfeDiagState(m1_diag_);\n'
                      '        m1_diag_ = nullptr;', afe)
        reporter = afe.split('const BaseType_t created = xTaskCreate(', 1)[1].split(
            '"m1_afe_diag"', 1)[0]
        self.assertIn('AfeAudioEngine::ReportM1AfeStats(state);', reporter)
        self.assertIn('ReleaseM1AfeDiagState(state);\n                vTaskDelete(nullptr);',
                      reporter)
        self.assertNotIn('engine->', reporter)
        self.assertNotIn('event_group_', reporter)
        self.assertNotIn('portMAX_DELAY', afe)

if __name__ == '__main__':
    unittest.main()
