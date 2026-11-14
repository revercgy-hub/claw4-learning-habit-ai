#pragma once

#include <lvgl.h>

// Use these helpers while holding the display lock.
const lv_font_t* Claw4UiTextFont();
void Claw4UiSetTextSize(lv_obj_t* label, int scale = 256);
