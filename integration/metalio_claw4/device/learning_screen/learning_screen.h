// claw4 P18 L0 Learning App Shell — Learning screen (device build tree).
// Minimal first-firmware scope (WB-LEARNING-V4-NEXT 阶段 F / P18):
//   Home -> Learning -> Mock Task -> Start -> Back
// NO backend / NVS / voice / STT / MCP / AI. Mock state is UI-local.
#pragma once

#include "lvgl.h"
#include "screen_util.h"

class LearningScreen {
 public:
  static lv_obj_t* Create();
  static void LifecycleCallback(screen_lifecycle_event_t event);
};
