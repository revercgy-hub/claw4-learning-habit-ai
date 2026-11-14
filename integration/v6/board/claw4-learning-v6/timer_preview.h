#pragma once

#include <cstdint>
#include <lvgl.h>

void Claw4TimerPreviewSetupUI(lv_obj_t* parent);
void Claw4TimerPreviewStart();
// Call only after the fixed LAN OTA response has validated server_time.
void Claw4TimerPreviewAcceptTime(int64_t epoch_ms);
#ifdef CLAW4_LEARNING_PREVIEW
#include <optional>
std::optional<int64_t> Claw4TimerPreviewTrustedTime();
bool Claw4TimerPreviewIsRinging();
#endif
