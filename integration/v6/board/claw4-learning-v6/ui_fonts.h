#pragma once

#include <lvgl.h>

// Use these helpers while holding the display lock.
const lv_font_t* Claw4UiTextFont();
void Claw4UiSetTextSize(lv_obj_t* label, int scale = 256);
const lv_font_t* Claw4UiIconFont();
lv_obj_t* Claw4UiAddIcon(lv_obj_t* parent, const char* symbol, uint32_t color);
lv_obj_t* Claw4UiShortcut(lv_obj_t* parent, int x, const char* title, const char* symbol,
                          uint32_t background, uint32_t badge, lv_obj_t** title_label = nullptr);
