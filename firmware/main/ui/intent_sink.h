// claw4/firmware/main/ui/intent_sink.h
// IntentSink: the only way the UI communicates with the domain.
// Portable C++17. MUST NOT include LVGL; MUST NOT mutate business state.
// Contract source: ARCHITECTURE.md §3.5 / I4.
#pragma once

#include "learning_domain/intents.h"

namespace claw4 {
namespace ui {

// The UI translates gestures into intents and pushes them through this sink.
// The domain layer validates and applies them; the UI never calls business
// methods or writes server data directly (ARCHITECTURE.md §3.5 / I4).
class IntentSink {
 public:
  virtual ~IntentSink() = default;
  virtual claw4::domain::IntentResult emit(const claw4::domain::IntentRequest& intent) = 0;
};

}  // namespace ui
}  // namespace claw4
