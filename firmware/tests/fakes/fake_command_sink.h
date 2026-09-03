// claw4/firmware/tests/fakes/fake_command_sink.h
// Deterministic CommandSink fake for interaction/MCP host tests.
// Records every emitted IntentRequest and returns a scripted IntentResult.
#pragma once

#include <vector>

#include "interaction/dispatcher.h"

namespace claw4 {
namespace interaction {

class FakeCommandSink final : public CommandSink {
 public:
  explicit FakeCommandSink(domain::IntentResult result = domain::IntentResult::Accepted)
      : result_(result) {}

  domain::IntentResult emit(const domain::IntentRequest& intent) override {
    emitted_.push_back(intent);
    ++emit_count_;
    return result_;
  }

  const std::vector<domain::IntentRequest>& emitted() const { return emitted_; }
  int emit_count() const { return emit_count_; }
  void set_result(domain::IntentResult result) { result_ = result; }

 private:
  domain::IntentResult result_;
  int emit_count_ = 0;
  std::vector<domain::IntentRequest> emitted_;
};

}  // namespace interaction
}  // namespace claw4
