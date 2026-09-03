// claw4/firmware/tests/fakes/fake_learning_backend.h
// Deterministic LearningBackend fake for MCP host tests.
// The test controls the snapshot and the injected monotonic clock.
#pragma once

#include "mcp/learning_mcp_host.h"

namespace claw4 {
namespace mcp {

class FakeLearningBackend final : public LearningBackend {
 public:
  domain::DomainState state;
  int64_t now_monotonic_ms = 0;

  domain::DomainState snapshot() const override { return state; }
  int64_t nowMonotonicMs() const override { return now_monotonic_ms; }
};

}  // namespace mcp
}  // namespace claw4
