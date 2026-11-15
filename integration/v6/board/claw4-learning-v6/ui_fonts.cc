#include "ui_fonts.h"
#include "sdkconfig.h"

#ifdef CONFIG_CLAW4_TIMER_PREVIEW
#include "cbin_font.h"
#include <cstdint>
#include <material_symbols.h>

LV_FONT_DECLARE(font_material_symbols_30_4);
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

const lv_font_t* Claw4UiIconFont() { return &font_material_symbols_30_4; }

lv_obj_t* Claw4UiAddIcon(lv_obj_t* parent, const char* symbol, uint32_t color) {
    auto* icon = lv_label_create(parent);
    lv_obj_set_style_text_font(icon, Claw4UiIconFont(), LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(icon, symbol);
    return icon;
}

lv_obj_t* Claw4UiShortcut(lv_obj_t* parent, int x, const char* title, const char* symbol,
                          uint32_t background, uint32_t badge, lv_obj_t** title_label) {
    auto* button = lv_button_create(parent);
    lv_obj_set_pos(button, x, 100);
    lv_obj_set_size(button, 324, 82);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 25, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);

    auto* icon = Claw4UiAddIcon(button, symbol, 0x234355);
    lv_obj_set_size(icon, 56, 54);
    lv_obj_set_style_bg_color(icon, lv_color_hex(badge), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(icon, 18, LV_PART_MAIN);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_top(icon, 10, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 14, 0);

    auto* label = lv_label_create(button);
    Claw4UiSetTextSize(label, 282);
    lv_obj_set_style_text_color(label, lv_color_hex(0x203D4D), LV_PART_MAIN);
    lv_label_set_text(label, title);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 84, 0);
    lv_obj_align(Claw4UiAddIcon(button, MATERIAL_SYMBOLS_ARROW_FORWARD, 0x426473), LV_ALIGN_RIGHT_MID, -16, 0);
    if (title_label) *title_label = label;
    return button;
}
#endif
