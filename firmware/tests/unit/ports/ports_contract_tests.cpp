// claw4/firmware/tests/unit/ports/ports_contract_tests.cpp
// Host contract tests for the 8 Platform Ports + deterministic fakes
// (WB-LEARNING-V4 P16).
//
// Compiles, links and RUNS natively on Windows. Explicit case/assert counter,
// non-zero exit on failure. Compilation itself is part of the gate: the 4b3
// include scan in verify-host-cpp-tests.ps1 proves none of interaction/mcp/
// ports includes LVGL / ESP-IDF / FreeRTOS / Metalio.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "fakes/fake_platform_ports.h"
#include "ports/clock_port.h"
#include "ports/mcp_registration_port.h"
#include "ports/network_port.h"
#include "ports/power_port.h"
#include "ports/storage_port.h"
#include "ports/stt_port.h"
#include "ports/ui_port.h"
#include "ports/voice_session_port.h"

using namespace claw4::ports;

// ---------------------------------------------------------------------------
// tiny test harness
// ---------------------------------------------------------------------------
static int g_cases = 0;
static int g_fail = 0;

#define CASE(name)                                   \
  do {                                               \
    ++g_cases;                                       \
    if (!run_case_##name()) {                        \
      std::printf("FAIL: %s\n", #name);              \
      ++g_fail;                                      \
    }                                                \
  } while (0)

#define CHECK(cond)                                  \
  do {                                               \
    if (!(cond)) {                                   \
      std::printf("  assert fail: %s (line %d)\n",   \
                  #cond, __LINE__);                  \
      return false;                                  \
    }                                                \
  } while (0)

static bool run_case_clock_port_roundtrip() {
  FakeClockPort p;
  p.epoch = 1756718530;
  p.mono = 42;
  p.synced = true;
  CHECK(p.epochSeconds() == 1756718530);
  CHECK(p.monotonicMs() == 42);
  CHECK(p.isTimeSynced());
  p.synced = false;
  CHECK(!p.isTimeSynced());
  return true;
}

static bool run_case_storage_port_roundtrip_and_failure() {
  FakeStoragePort p;
  CHECK(!p.contains("k"));
  CHECK(p.write("k", "v1"));
  CHECK(p.contains("k"));
  CHECK(p.read("k") == std::optional<std::string>("v1"));
  CHECK(p.write("k", "v2"));  // overwrite
  CHECK(p.read("k") == std::optional<std::string>("v2"));

  p.fail_writes = true;
  const int before = p.write_calls;
  CHECK(!p.write("k", "v3"));
  CHECK(p.write_calls == before + 1);
  CHECK(p.read("k") == std::optional<std::string>("v2"));  // untouched

  p.fail_writes = false;
  CHECK(p.erase("k"));
  CHECK(!p.contains("k"));
  CHECK(!p.erase("k"));  // missing key
  CHECK(!p.read("k").has_value());
  return true;
}

static bool run_case_network_port_roundtrip() {
  FakeNetworkPort p;
  p.net.online = true;
  p.net.ssid = "home-5g";
  CHECK(p.status().online);
  CHECK(p.status().ssid == "home-5g");

  p.next_post_ok = true;
  p.next_response = R"({"ok":1})";
  std::string resp;
  CHECK(p.postJson("https://example/api", "{}", 3000, resp));
  CHECK(resp == R"({"ok":1})");
  CHECK(p.post_calls == 1);
  CHECK(p.last_url == "https://example/api");
  CHECK(p.last_timeout_ms == 3000);

  p.next_post_ok = false;  // transport failure injection
  CHECK(!p.postJson("https://example/api", "{}", 3000, resp));
  CHECK(p.post_calls == 2);
  return true;
}

static bool run_case_ui_port_roundtrip() {
  FakeUiPort p;
  p.showScreen(claw4::ui::Screen::Focus);
  CHECK(p.last_screen == claw4::ui::Screen::Focus);

  bool choice = false;
  p.showConfirm("确认完成?", [&choice](bool ok) { choice = ok; });
  CHECK(p.confirm_calls == 1);
  CHECK(p.last_confirm_title == "确认完成?");
  p.resolveConfirm(true);
  CHECK(choice);

  p.toast("已保存");
  CHECK(p.toasts.size() == 1);
  CHECK(p.toasts[0] == "已保存");
  return true;
}

static bool run_case_voice_session_port_roundtrip() {
  FakeVoiceSessionPort p;
  p.begin_result = false;
  CHECK(!p.beginSession());
  CHECK(!p.acquired);

  p.begin_result = true;
  CHECK(p.beginSession());
  CHECK(p.acquired);
  p.endSession();
  CHECK(!p.acquired);
  CHECK(p.begin_calls == 2);
  CHECK(p.end_calls == 1);
  return true;
}

static bool run_case_stt_port_roundtrip() {
  FakeSttPort p;
  std::string heard;
  p.setResultCallback([&heard](const std::string& text) { heard = text; });
  p.setEnabled(true);
  CHECK(p.enabled);
  p.emitResult("开始学习");
  CHECK(heard == "开始学习");
  p.setEnabled(false);
  CHECK(!p.enabled);
  return true;
}

static bool run_case_mcp_registration_port_roundtrip() {
  FakeMcpRegistrationPort p;
  CHECK(p.registerTool("learning.get_today_tasks",
                       [](const std::string&) { return "[]"; }));
  CHECK(p.registerTool("learning.start_task",
                       [](const std::string&) { return "{}"; }));
  CHECK(!p.registerTool("learning.start_task",  // duplicate rejected
                        [](const std::string&) { return "{}"; }));
  const auto names = p.registeredNames();
  CHECK(names.size() == 2);
  bool found = false;
  for (const auto& n : names) {
    if (n == "learning.start_task") found = true;
  }
  CHECK(found);
  return true;
}

static bool run_case_power_port_roundtrip() {
  FakePowerPort p;
  p.power.battery_percent = 87;
  p.power.charging = true;
  CHECK(p.info().battery_percent == 87);
  CHECK(p.info().charging);
  p.stayAwake(true);
  CHECK(p.awake);
  p.stayAwake(false);
  CHECK(!p.awake);
  return true;
}

static bool run_case_all() {
  CASE(clock_port_roundtrip);
  CASE(storage_port_roundtrip_and_failure);
  CASE(network_port_roundtrip);
  CASE(ui_port_roundtrip);
  CASE(voice_session_port_roundtrip);
  CASE(stt_port_roundtrip);
  CASE(mcp_registration_port_roundtrip);
  CASE(power_port_roundtrip);
  return g_fail == 0;
}

int main() {
  std::printf("== WB-LEARNING-V4 P16 platform ports contract tests ==\n");
  const bool ok = run_case_all();
  std::printf("cases=%d failures=%d\n", g_cases, g_fail);
  return ok ? 0 : 1;
}
