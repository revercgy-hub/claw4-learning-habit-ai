// claw4/integration/metalio_claw4/host_glue/coordinator_glue.h
// P17a — Host/device-shared production glue over AppCoordinator
// (WB-LEARNING-V4-NEXT 阶段 E). Portable C++17, host-testable: mirrors the
// host-funnel acceptance pattern (CoordSink/CoordBackend) as REAL glue so the
// device shell reuses it verbatim instead of re-implementing wiring.
//
// No Metalio / ESP-IDF / LVGL headers. The concrete OutboxStorage and
// ports::ClockPort are injected (host: fakes/std::chrono; device: NVS +
// RTC/esp_timer adapters from P17d).
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "application/coordinator.h"
#include "interaction/dispatcher.h"
#include "learning_domain/domain_state.h"
#include "learning_domain/ids.h"
#include "learning_domain/intents.h"
#include "learning_domain/reducer.h"
#include "mcp/learning_mcp_host.h"
#include "ports/clock_port.h"
#include "sync/outbox_storage.h"

namespace claw4 {
namespace metalio {

// Builds a deterministic ReducerContext from a ClockPort + injected identity.
// Event/session id sources default to sequential values (same reuse semantics
// as the host tests); a device adapter overrides them with its own generator.
class ContextBuilder {
 public:
  explicit ContextBuilder(ports::ClockPort& clock) : clock_(clock) {}

  void setIdentity(claw4::domain::DeviceId device, claw4::domain::ChildId child) {
    device_ = std::move(device);
    child_ = std::move(child);
  }
  void setEventIdFactory(std::function<claw4::domain::EventId()> f) {
    event_factory_ = std::move(f);
  }
  void setSessionIdFactory(std::function<claw4::domain::SessionId()> f) {
    session_factory_ = std::move(f);
  }

  claw4::domain::ReducerContext make() {
    claw4::domain::ReducerContext c;
    c.device_id = device_;
    c.child_id = child_;
    c.now_epoch = clock_.epochSeconds();
    c.monotonic_ms = clock_.monotonicMs();
    c.make_event_id = event_factory_ ? event_factory_
                                     : [this]() {
                                         return claw4::domain::EventId{
                                             "ev-" + std::to_string(++event_seq_)};
                                       };
    c.make_session_id = session_factory_ ? session_factory_
                                          : [this]() {
                                              return claw4::domain::SessionId{
                                                  "sess-" +
                                                  std::to_string(++session_seq_)};
                                            };
    return c;
  }

 private:
  ports::ClockPort& clock_;
  claw4::domain::DeviceId device_;
  claw4::domain::ChildId child_;
  std::function<claw4::domain::EventId()> event_factory_;
  std::function<claw4::domain::SessionId()> session_factory_;
  int event_seq_ = 0;
  int session_seq_ = 0;
};

// interaction::CommandSink implementation over AppCoordinator (single funnel
// exit used by P14 CommandDispatcher / P15 MCP host on the device too).
class CommandSinkGlue final : public interaction::CommandSink {
 public:
  CommandSinkGlue(application::AppCoordinator& app, ContextBuilder& ctx)
      : app_(app), ctx_(ctx) {}

  claw4::domain::IntentResult emit(
      const claw4::domain::IntentRequest& intent) override {
    return app_.dispatchIntent(intent, ctx_.make()).intent_result;
  }

 private:
  application::AppCoordinator& app_;
  ContextBuilder& ctx_;
};

// mcp::LearningBackend implementation over the coordinator's committed state
// (read-only projections for the 8 learning.* query tools).
class BackendGlue final : public mcp::LearningBackend {
 public:
  BackendGlue(application::AppCoordinator& app, ports::ClockPort& clock)
      : app_(app), clock_(clock) {}

  claw4::domain::DomainState snapshot() const override { return app_.state(); }
  int64_t nowMonotonicMs() const override { return clock_.monotonicMs(); }

 private:
  application::AppCoordinator& app_;
  ports::ClockPort& clock_;
};

}  // namespace metalio
}  // namespace claw4
