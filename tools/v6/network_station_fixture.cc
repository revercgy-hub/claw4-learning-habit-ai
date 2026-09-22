// Host shim for the actual generated HandleScanResult / StartConnect methods.
// The runner inserts those method bodies; no alternative algorithm is tested.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include "saved_network_policy.h"
#define TAG "fixture"
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_ERROR_CHECK(x) assert((x) == 0)
constexpr int ESP_OK = 0, MAX_RECONNECT_COUNT = 5, WIFI_IF_STA = 0;
constexpr int WIFI_ALL_CHANNEL_SCAN = 1, WIFI_FAST_SCAN = 2, WIFI_CONNECT_AP_BY_SIGNAL = 1;
using esp_err_t = int;
struct wifi_ap_record_t { uint8_t ssid[33]{}, bssid[6]{}; int rssi=0, primary=0, authmode=0; };
struct wifi_config_t {
    struct { uint8_t ssid[32]{}, password[64]{}, bssid[6]{};
        int channel=0, scan_method=0, sort_method=0, failure_retry_cnt=0, listen_interval=0;
        bool bssid_set=false; } sta;
};
struct Saved { std::string ssid, password; };
using SsidItem = Saved;
void bzero(void* p, std::size_t n) { std::memset(p, 0, n); }
struct WifiApRecord { std::string ssid, password; int channel, authmode; uint8_t bssid[6]; bool direct=false; };
struct SsidManager {
    std::vector<Saved> saved;
    static SsidManager& GetInstance() { static SsidManager s; return s; }
    std::vector<Saved> GetSsidList() { return saved; }
};
static std::vector<wifi_ap_record_t> aps;
static std::vector<wifi_config_t> configs;
static int scan_error=0, connect_error=0, connects=0, timers=0;
int esp_wifi_scan_get_ap_num(uint16_t* n) { *n=aps.size(); return scan_error; }
int esp_wifi_scan_get_ap_records(uint16_t*, wifi_ap_record_t* out) { std::copy(aps.begin(),aps.end(),out); return scan_error; }
void esp_wifi_clear_ap_list() {}
void esp_timer_start_once(int, int) { ++timers; }
int esp_wifi_set_config(int, wifi_config_t* c) { configs.push_back(*c); return 0; }
int esp_wifi_connect() { ++connects; return connect_error; }
struct WifiStation {
    std::vector<WifiApRecord> connect_queue_;
    bool last_scan_used_saved_channels_=false, use_saved_channels_scan_=false;
    uint8_t remember_bssid_=0, failure_retry_cnt_=3;
    int timer_handle_=0, scan_current_interval_microseconds_=10000000, reconnect_count_=0, scans=0, backoffs=0;
    std::string ssid_, password_;
    std::function<void(const std::string&)> on_connect_;
    void StartScan() { ++scans; }
    void UpdateScanInterval() { ++backoffs; }
    void HandleScanResult();
    void StartConnect();
};
// INSERT_PRODUCTION_METHODS
void reset() {
    aps.clear(); configs.clear(); connects=timers=scan_error=connect_error=0;
    SsidManager::GetInstance().saved={{"hidden", "password"}, {"two", "second"}, {"three", "third"}, {"four", "fourth"}};
}
int main() {
    reset(); WifiStation full;
    full.HandleScanResult();
    assert(connects==1 && full.connect_queue_.size()==2);
    assert(configs[0].sta.channel==0 && !configs[0].sta.bssid_set);
    assert(configs[0].sta.scan_method==WIFI_ALL_CHANNEL_SCAN && configs[0].sta.failure_retry_cnt==0);
    assert(full.reconnect_count_==MAX_RECONNECT_COUNT);
    reset(); WifiStation partial; partial.last_scan_used_saved_channels_=true;
    partial.HandleScanResult(); assert(connects==0 && partial.scans==1 && timers==0);
    reset(); WifiStation pinned; pinned.remember_bssid_=1;
    pinned.HandleScanResult(); assert(connects==0 && timers==1);
    reset(); WifiStation failed; connect_error=9;
    failed.HandleScanResult(); assert(connects==3 && timers==1 && failed.backoffs==1);
    reset(); WifiStation bad_scan; scan_error=7;
    bad_scan.HandleScanResult(); assert(connects==0 && timers==1);
    reset(); WifiStation visible;
    wifi_ap_record_t ap{}; std::memcpy(ap.ssid,"hidden",6); ap.primary=11; aps.push_back(ap);
    visible.HandleScanResult();
    assert(connects==1 && visible.connect_queue_.empty());
    assert(configs[0].sta.channel==11 && configs[0].sta.scan_method==WIFI_FAST_SCAN);
    assert(visible.reconnect_count_==0 && configs[0].sta.failure_retry_cnt==3);
    reset(); WifiStation empty; SsidManager::GetInstance().saved.clear();
    empty.HandleScanResult(); assert(connects==0 && timers==1);
}
