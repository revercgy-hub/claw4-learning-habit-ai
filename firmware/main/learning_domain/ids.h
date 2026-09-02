// claw4/firmware/main/learning_domain/ids.h
// ID value types for the learning domain.
// Portable C++17. No LVGL / Wi-Fi / GPIO / ESP-IDF / FreeRTOS / BSP dependencies.
#pragma once

#include <string>

namespace claw4 {
namespace domain {

// Lightweight ID value types. In MVP all IDs are UUID strings issued or
// generated per ARCHITECTURE.md §5; the wrappers keep domain code readable
// and make future migration to typed IDs safe.

struct TaskId {
  std::string value;

  bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const TaskId& a, const TaskId& b) noexcept {
    return a.value == b.value;
  }
  friend bool operator!=(const TaskId& a, const TaskId& b) noexcept {
    return a.value != b.value;
  }
};

struct SessionId {
  std::string value;
  bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const SessionId& a, const SessionId& b) noexcept {
    return a.value == b.value;
  }
  friend bool operator!=(const SessionId& a, const SessionId& b) noexcept {
    return a.value != b.value;
  }
};

struct ChildId {
  std::string value;
  bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const ChildId& a, const ChildId& b) noexcept {
    return a.value == b.value;
  }
  friend bool operator!=(const ChildId& a, const ChildId& b) noexcept {
    return a.value != b.value;
  }
};

struct DeviceId {
  std::string value;
  bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const DeviceId& a, const DeviceId& b) noexcept {
    return a.value == b.value;
  }
  friend bool operator!=(const DeviceId& a, const DeviceId& b) noexcept {
    return a.value != b.value;
  }
};

struct EventId {
  std::string value;
  bool empty() const noexcept { return value.empty(); }
  friend bool operator==(const EventId& a, const EventId& b) noexcept {
    return a.value == b.value;
  }
  friend bool operator!=(const EventId& a, const EventId& b) noexcept {
    return a.value != b.value;
  }
};

}  // namespace domain
}  // namespace claw4
