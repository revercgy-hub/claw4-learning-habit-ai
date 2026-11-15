#include "preview_pages.h"
#include "sdkconfig.h"

#ifdef CONFIG_CLAW4_TIMER_PREVIEW
#include <lvgl.h>

namespace {
PreviewPageSelection selection;
lv_obj_t* backdrop;
lv_obj_t* timer_panel;
lv_obj_t* learning_panel;

void Show(lv_obj_t* object, bool visible) {
    if (!object) return;
    if (visible && lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    if (!visible && !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

void Render() {
    const auto page = selection.Visible();
    const bool show_timer = page == PreviewPage::Timer && timer_panel;
    const bool show_learning = page == PreviewPage::Learning && learning_panel;
    Show(timer_panel, show_timer);
    Show(learning_panel, show_learning);
    Show(backdrop, show_timer || show_learning);
}
}  // namespace

void Claw4PreviewPagesSetup(lv_obj_t* layer) {
    backdrop = lv_obj_create(layer);
    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, lv_pct(100), lv_pct(100));
    lv_obj_center(backdrop);
    lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x142E40), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_60, LV_PART_MAIN);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* Claw4PreviewPanelHost() { return backdrop; }

void Claw4PreviewRegister(PreviewPage page, lv_obj_t* panel) {
    if (page == PreviewPage::Timer) timer_panel = panel;
    if (page == PreviewPage::Learning) learning_panel = panel;
    lv_obj_move_foreground(backdrop);
    Render();
}

void Claw4PreviewOpen(PreviewPage page) { selection.Open(page); Render(); }
void Claw4PreviewClose(PreviewPage page) { selection.Close(page); Render(); }
void Claw4PreviewSetRinging(bool ringing) { selection.SetRinging(ringing); Render(); }
#endif
