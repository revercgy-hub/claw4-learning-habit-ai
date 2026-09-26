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

PREFLIGHT_OTA_URL = 'http://192.168.3.100:7443/xiaozhi/ota/'
PREFLIGHT_WS_URL = 'ws://192.168.3.100:7444/xiaozhi/v1/'

M1_ENDPOINT_PATCHES = {
    'main/ota.cc': (
        ('''    Settings settings("wifi", false);
    std::string url = settings.GetString("ota_url");
    if (url.empty()) {
        url = CONFIG_OTA_URL;
    }
    return url;''', '''    // M1 preflight never uses persisted wifi/ota_url.
    constexpr char kOtaUrl[] = "http://192.168.3.100:7443/xiaozhi/ota/";
    if (std::string(CONFIG_OTA_URL) != kOtaUrl) {
        ESP_LOGE(TAG, "M1 preflight OTA build URL mismatch");
        return {};
    }
    return kOtaUrl;'''),
        ('''    if (url.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");''', '''    if (url != "http://192.168.3.100:7443/xiaozhi/ota/") {
        ESP_LOGE(TAG, "M1 preflight OTA URL rejected");'''),
        ('''    has_activation_code_ = false;
    has_activation_challenge_ = false;''', '''    // Reject the response before any persistent protocol setting is changed.
    cJSON *preflight_ws = cJSON_GetObjectItem(root, "websocket");
    cJSON *preflight_url = cJSON_GetObjectItem(preflight_ws, "url");
    if (!cJSON_IsObject(preflight_ws) || !cJSON_IsString(preflight_url) ||
        std::string(preflight_url->valuestring) != "ws://192.168.3.100:7444/xiaozhi/v1/") {
        ESP_LOGE(TAG, "M1 preflight WebSocket URL rejected");
        cJSON_Delete(root);
        return std::unexpected(NetworkError::InvalidArgument());
    }
    has_activation_code_ = false;
    has_activation_challenge_ = false;'''),
        ('''    has_mqtt_config_ = false;
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (cJSON_IsObject(mqtt)) {
        Settings settings("mqtt", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (cJSON_IsString(item)) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            } else if (cJSON_IsNumber(item)) {
                if (settings.GetInt(item->string) != item->valueint) {
                    settings.SetInt(item->string, item->valueint);
                }
            }
        }
        has_mqtt_config_ = true;
    } else {
        ESP_LOGI(TAG, "No mqtt section found !");
    }''', '''    // Ignore any MQTT response and retain existing NVS bytes unchanged.
    has_mqtt_config_ = false;'''),
    ),
    'main/application.cc': (
        ('''    mcp_server.AddCommonTools();
    mcp_server.AddUserOnlyTools();''', '''    mcp_server.AddCommonTools();
    // M1 preflight excludes user-only tools with arbitrary HTTP/upgrade URLs.'''),
        ('''    // Check for new assets version
    CheckAssetsVersion();

    // Check for new firmware version
    CheckNewVersion();

    // Initialize the protocol
    InitializeProtocol();''', '''    // M1 preflight performs one fixed NAS check, without asset or firmware upgrades.
    auto check = ota_->CheckVersion();
    if (!check || !ota_->HasWebsocketConfig()) {
        last_error_message_ = "M1 preflight NAS WebSocket configuration rejected";
        xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR | MAIN_EVENT_ACTIVATION_DONE);
        return;
    }
    InitializeProtocol();'''),
        ('''    if (ota_->HasMqttConfig()) {
        protocol_ = std::make_unique<MqttProtocol>();
    } else if (ota_->HasWebsocketConfig()) {
        protocol_ = std::make_unique<WebsocketProtocol>();
    } else {
        ESP_LOGW(TAG, "No protocol specified in the OTA config, using MQTT");
        protocol_ = std::make_unique<MqttProtocol>();
    }''', '''    // M1 preflight has no MQTT fallback, even with persisted MQTT settings.
    if (!ota_->HasWebsocketConfig()) {
        last_error_message_ = "M1 preflight WebSocket configuration missing";
        xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR);
        return;
    }
    protocol_ = std::make_unique<WebsocketProtocol>();'''),
        ('''            Schedule([this, url = std::string(audio_url->valuestring),
                      subtitles = std::move(subtitles)]() mutable {
                StartNotification(std::move(url), std::move(subtitles));
            });''', '''            // M1 preflight forbids notify HTTP URLs; TTS packets still use WebSocket audio.
            ESP_LOGW(TAG, "M1 preflight ignored notify audio URL");'''),
        ('''bool Application::UpgradeFirmware(const std::string& url, const std::string& version) {
    auto& board = Board::GetInstance();''', '''bool Application::UpgradeFirmware(const std::string& url, const std::string& version) {
    // No firmware writes from any M1 preflight caller.
    (void)url;
    (void)version;
    ESP_LOGE(TAG, "M1 preflight firmware upgrade disabled");
    return false;
    auto& board = Board::GetInstance();'''),
    ),
    'main/protocols/websocket_protocol.cc': (
        ('''    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");''', '''    std::string url = settings.GetString("url");
    if (url != "ws://192.168.3.100:7444/xiaozhi/v1/") {
        ESP_LOGE(TAG, "M1 preflight WebSocket URL rejected");
        SetError(Lang::Strings::SERVER_NOT_CONNECTED, url);
        return false;
    }
    std::string token = settings.GetString("token");'''),
    ),
}


def patch_m1_endpoints(destination):
    for relative, edits in M1_ENDPOINT_PATCHES.items():
        path = destination / relative
        for old, new in edits:
            replace_once(path, old, new)


def replace_once(path, old, new):
    source = path.read_text(encoding='utf-8')
    if source.count(old) != 1:
        raise ValueError(f'Upstream anchor drift: {path}: {old[:60]}')
    path.write_text(source.replace(old, new), encoding='utf-8', newline='\n')


def m1_input_diagnostics_kconfig(enabled):
    """Keep the M1 opt-in independent of the M0 local diagnostic app."""
    default = 'y' if enabled else 'n'
    return ('\nconfig CLAW4_M1_INPUT_DIAGNOSTICS\n'
            '    bool "Claw4 M1 input-only diagnostic counters"\n'
            '    depends on BOARD_TYPE_CLAW4_LEARNING_V6 && !CLAW4_M0_DIAGNOSTICS\n'
            f'    default {default}\n')


def stage(upstream, destination, variant='m0', m1_input_diagnostics=False):
    if variant not in ('m0', 'm1'):
        raise ValueError(f'Unknown build variant: {variant}')
    if m1_input_diagnostics and variant != 'm1':
        raise ValueError('M1 input diagnostics require the M1 variant')
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
        patch_m1_endpoints(destination)
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
        if variant == 'm1':
            output.write(m1_input_diagnostics_kconfig(m1_input_diagnostics))
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
                'endpoint_policy': 'm1_fixed_nas_ota_ws_no_upgrade' if variant == 'm1' else 'upstream',
                'component_lock_sha256': hashlib.sha256((destination / 'dependencies.lock').read_bytes()).hexdigest()
                    if variant == 'm1' else None}
    if variant == 'm1':
        manifest['m1_input_diagnostics'] = m1_input_diagnostics
        manifest['staged_kconfig_sha256'] = hashlib.sha256(
            (destination / 'main/Kconfig.projbuild').read_bytes()).hexdigest()
    (destination / 'v6-stage-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--variant', choices=('m0', 'm1'), default='m0')
    parser.add_argument('--m1-input-diagnostics', action='store_true',
                        help='Enable M1 mic level/read-failure counters in this staged candidate')
    args = parser.parse_args()
    stage(args.upstream.resolve(), args.destination.resolve(), args.variant,
          args.m1_input_diagnostics)
