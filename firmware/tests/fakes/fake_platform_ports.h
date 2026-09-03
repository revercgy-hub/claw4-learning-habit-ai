// claw4/firmware/tests/fakes/fake_platform_ports.h
// Deterministic fakes for the 8 Platform Ports (WB-LEARNING-V4 P16).
// Header-only, for host tests only. State is fully injectable and every port
// records call counters so tests can assert the interaction happened.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ports/clock_port.h"
#include "ports/mcp_registration_port.h"
#include "ports/network_port.h"
#include "ports/power_port.h"
#include "ports/storage_port.h"
#include "ports/stt_port.h"
#include "ports/ui_port.h"
#include "ports/voice_session_port.h"

namespace claw4 {
namespace ports {

class FakeClockPort final : public ClockPort {
 public:
  int64_t epoch = 1756718530;
  int64_t mono = 0;
  bool synced = true;

  int64_t epochSeconds() override { return epoch; }
  int64_t monotonicMs() override { return mono; }
  bool isTimeSynced() override { return synced; }
};

class FakeStoragePort final : public StoragePort {
 public:
  std::map<std::string, std::string> store;
  bool fail_writes = false;
  int write_calls = 0;
  int erase_calls = 0;

  std::optional<std::string> read(const std::string& key) override {
    const auto it = store.find(key);
    if (it == store.end()) return std::nullopt;
    return it->second;
  }
  bool write(const std::string& key, const std::string& value) override {
    ++write_calls;
    if (fail_writes) return false;
    store[key] = value;
    return true;
  }
  bool erase(const std::string& key) override {
    ++erase_calls;
    return store.erase(key) > 0;
  }
  bool contains(const std::string& key) override {
    return store.find(key) != store.end();
  }
};

class FakeNetworkPort final : public NetworkPort {
 public:
  NetworkStatus net;
  bool next_post_ok = false;
  std::string next_response;
  int post_calls = 0;
  std::string last_url;
  std::string last_body;
  int64_t last_timeout_ms = 0;

  NetworkStatus status() override { return net; }
  bool postJson(const std::string& url, const std::string& body,
                int64_t timeout_ms, std::string& response) override {
    ++post_calls;
    last_url = url;
    last_body = body;
    last_timeout_ms = timeout_ms;
    response = next_response;
    return next_post_ok;
  }
};

class FakeUiPort final : public UiPort {
 public:
  claw4::ui::Screen last_screen = claw4::ui::Screen::Home;
  std::vector<std::string> toasts;
  int confirm_calls = 0;
  std::string last_confirm_title;
  std::optional<std::function<void(bool)>> pending_confirm;

  void showScreen(claw4::ui::Screen screen) override { last_screen = screen; }
  void showConfirm(const std::string& title,
                   std::function<void(bool)> on_confirm) override {
    ++confirm_calls;
    last_confirm_title = title;
    pending_confirm = std::move(on_confirm);
  }
  void toast(const std::string& text) override { toasts.push_back(text); }

  // Test helper: simulate the physical choice.
  void resolveConfirm(bool choice) {
    if (pending_confirm) {
      auto cb = *pending_confirm;
      pending_confirm.reset();
      cb(choice);
    }
  }
};

class FakeVoiceSessionPort final : public VoiceSessionPort {
 public:
  bool acquired = false;
  bool begin_result = true;
  int begin_calls = 0;
  int end_calls = 0;

  bool beginSession() override {
    ++begin_calls;
    if (!begin_result) return false;
    acquired = true;
    return true;
  }
  void endSession() override {
    ++end_calls;
    acquired = false;
  }
};

class FakeSttPort final : public SttPort {
 public:
  bool enabled = false;
  int set_enabled_calls = 0;
  ResultCallback callback;

  void setEnabled(bool value) override {
    ++set_enabled_calls;
    enabled = value;
  }
  void setResultCallback(ResultCallback value) override { callback = std::move(value); }

  // Test helper: deliver a recognition result.
  void emitResult(const std::string& text) {
    if (callback) callback(text);
  }
};

class FakeMcpRegistrationPort final : public McpRegistrationPort {
 public:
  std::map<std::string, ToolHandler> tools;

  bool registerTool(const std::string& name, ToolHandler handler) override {
    if (tools.find(name) != tools.end()) return false;
    tools[name] = std::move(handler);
    return true;
  }
  std::vector<std::string> registeredNames() override {
    std::vector<std::string> names;
    names.reserve(tools.size());
    for (const auto& kv : tools) names.push_back(kv.first);
    return names;
  }
};

class FakePowerPort final : public PowerPort {
 public:
  PowerInfo power;
  bool awake = false;

  PowerInfo info() override { return power; }
  void stayAwake(bool keep_awake) override { awake = keep_awake; }
};

}  // namespace ports
}  // namespace claw4
