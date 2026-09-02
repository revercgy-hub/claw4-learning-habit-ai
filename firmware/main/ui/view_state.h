// claw4/firmware/main/ui/view_state.h
// Read-only view state consumed by the UI layer.
// Portable C++17. MUST NOT include LVGL or any hardware/network headers.
// Contract source: ARCHITECTURE.md §3.5 / §8.1.
#pragma once

#include <cstdint>
#include "learning_domain/domain_state.h"

namespace claw4 {
namespace ui {

// Which screen is active. UI is a presentation layer: it only renders state
// snapshots and emits intents (ARCHITECTURE.md §8.1).
enum class Screen : uint8_t {
  Home = 0,
  Focus,
  Done,
  Offline,
};

// Immutable snapshot handed to the renderer. No setters — the UI cannot
// mutate business truth.
struct ViewState {
  Screen screen = Screen::Home;
  claw4::domain::DomainState domain;  // includes the cached task snapshots
};

}  // namespace ui
}  // namespace claw4
