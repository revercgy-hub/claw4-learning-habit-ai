// claw4/integration/metalio_claw4/host_glue/learning_app.h
// P17a — LearningApp lifecycle container over the transactional coordinator.
// Portable C++17, host-testable; the device shell (P17b/P18) instantiates one
// LearningApp with its NVS-backed storage + clock adapter and drives the same
// funnel the host tests already prove.
#pragma once

#include <vector>

#include "application/coordinator.h"
#include "interaction/dispatcher.h"
#include "learning_domain/reducer.h"
#include "mcp/learning_mcp_host.h"
#include "ports/clock_port.h"
#include "sync/outbox_storage.h"

#include "metalio_claw4/host_glue/coordinator_glue.h"

namespace claw4 {
namespace metalio {

class LearningApp {
 public:
  // storage/clock are owned by the caller and must outlive this app.
  LearningApp(sync::OutboxStorage& storage, ports::ClockPort& clock)
      : app_(storage, reducer_), ctx_(clock), sink_(app_, ctx_),
        dispatcher_(sink_), backend_(app_, clock), mcp_(dispatcher_, backend_) {}

  // --- lifecycle ---------------------------------------------------------
  void start() { running_ = true; }
  void stop() { running_ = false; }
  bool running() const { return running_; }

  // --- identity / id sources (device overrides the host-default sequential
  // factories so rebooted devices never reuse pre-reboot event/session ids;
  // CommandSinkGlue reads ctx per emit, so these take effect immediately). ---
  void setIdentity(claw4::domain::DeviceId device, claw4::domain::ChildId child) {
    ctx_.setIdentity(std::move(device), std::move(child));
  }
  void setEventIdFactory(std::function<claw4::domain::EventId()> f) {
    ctx_.setEventIdFactory(std::move(f));
  }
  void setSessionIdFactory(std::function<claw4::domain::SessionId()> f) {
    ctx_.setSessionIdFactory(std::move(f));
  }

  // --- domain entry ------------------------------------------------------
  bool applyTodaySnapshot(const std::vector<claw4::domain::Task>& server_tasks) {
    return app_.applyTodaySnapshot(server_tasks);
  }
  application::SyncOutcome runSyncOnce(application::SyncTransport& transport,
                                       const application::ReauthFn& reauth) {
    return app_.runSyncOnce(transport, reauth);
  }

  // --- funnel access (P14/P15 on the real coordinator) --------------------
  interaction::CommandDispatcher& dispatcher() { return dispatcher_; }
  mcp::LearningMcpHost& mcpHost() { return mcp_; }
  application::AppCoordinator& coordinator() { return app_; }
  const claw4::domain::DomainState& state() const { return app_.state(); }
  int pendingCount() const { return app_.pendingCount(); }
  int64_t lastAcked() const { return app_.lastAcked(); }
  bool authPaused() const { return app_.authPaused(); }
  void resetAuthPause() { app_.resetAuthPause(); }

 private:
  claw4::domain::DomainReducer reducer_;  // stateless pure reducer
  application::AppCoordinator app_;
  ContextBuilder ctx_;
  CommandSinkGlue sink_;
  interaction::CommandDispatcher dispatcher_;
  BackendGlue backend_;
  mcp::LearningMcpHost mcp_;
  bool running_ = false;
};

}  // namespace metalio
}  // namespace claw4
