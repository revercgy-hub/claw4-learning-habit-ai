// claw4/firmware/main/ports/ui_port.h
// UiPort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// Rendering/dialog boundary for the app shell. LVGL itself belongs to the P17
// device adapter; this port only carries semantic actions + confirmation
// callbacks so host tests never depend on pixels. Portable C++17.
#pragma once

#include <functional>
#include <string>

#include "ui/view_state.h"

namespace claw4 {
namespace ports {

class UiPort {
 public:
  virtual ~UiPort() = default;
  // Navigate the shell to a screen (Home / Focus / Done / Offline).
  virtual void showScreen(claw4::ui::Screen screen) = 0;
  // Modal confirmation (e.g. "AI 请求完成任务"): `on_confirm(bool)` is called
  // exactly once with the child/parent's physical choice.
  virtual void showConfirm(const std::string& title,
                           std::function<void(bool)> on_confirm) = 0;
  virtual void toast(const std::string& text) = 0;
};

}  // namespace ports
}  // namespace claw4
