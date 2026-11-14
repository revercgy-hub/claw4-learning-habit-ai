#include "ui_fonts.h"
#include "sdkconfig.h"

#ifdef CONFIG_CLAW4_TIMER_PREVIEW
#include "cbin_font.h"
#include <cstdint>

extern const uint8_t kChineseFont[] asm("_binary_font_noto_sans_common_20_4_bin_start");

const lv_font_t* Claw4UiTextFont() {
    static const auto* font = cbin_font_create(const_cast<uint8_t*>(kChineseFont));
    return font ? font : LV_FONT_DEFAULT;
}

void Claw4UiSetTextSize(lv_obj_t* label, int scale) {
    lv_obj_set_style_text_font(label, Claw4UiTextFont(), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(label, lv_pct(50), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(label, lv_pct(50), LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(label, scale, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(label, scale, LV_PART_MAIN);
}
#endif
