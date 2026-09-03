// claw4 P18 L0 Learning App Shell — Learning screen (device build tree).
// Minimal first-firmware scope (WB-LEARNING-V4-NEXT 阶段 F / P18):
//   Home -> Learning -> Mock Task -> Start -> Back
// NO backend / NVS / voice / STT / MCP / AI in this screen. State is UI-local
// (l0 mock) by design; the host-verified domain funnel (firmware/main +
// integration/metalio_claw4/host_glue) lands from P17d/L1 onward.
#include "learning_screen/learning_screen.h"

#include "esp_log.h"
#include "home_screen/home_screen.h"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_30_4);

namespace {

constexpr const char* TAG = "LearningScreen";
constexpr int kPanelW = 720;
constexpr int kPanelH = 720;

struct Ui {
  lv_obj_t* status_label = nullptr;
  bool running = false;
  bool done = false;
};
Ui s_ui;

// Back: recreate Home exactly like other apps (calculator OnSwipeBack pattern).
void GoHome() {
  lv_obj_t* old_scr = lv_screen_active();
  lv_obj_t* home = HomeScreen::Create();
  lv_screen_load(home);
  if (old_scr != nullptr && old_scr != home) {
    lv_obj_delete_async(old_scr);
  }
}

void OnStartClick(lv_event_t* /*event*/) {
  if (s_ui.status_label == nullptr) return;
  if (!s_ui.running) {
    s_ui.running = true;
    lv_label_set_text(s_ui.status_label, "专注中（L0 Mock，UI 状态）");
  } else if (!s_ui.done) {
    s_ui.done = true;
    lv_label_set_text(s_ui.status_label, "已完成（L0 Mock）");
  }
}

lv_obj_t* MakeButton(lv_obj_t* parent, int x, int y, int w, int h,
                     const char* text,
                     lv_event_cb_t handler) {
  lv_obj_t* btn = lv_obj_create(parent);
  screen_strip_obj_chrome(btn);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(btn, handler, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &font_puhui_30_4, 0);
  lv_obj_center(label);
  return btn;
}

}  // namespace

void LearningScreen::LifecycleCallback(screen_lifecycle_event_t event) {
  if (event == SCREEN_LIFECYCLE_LOAD) {
    ESP_LOGI(TAG, "load: learning_screen");
  } else {
    ESP_LOGI(TAG, "unload: learning_screen");
  }
}

lv_obj_t* LearningScreen::Create() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  s_ui.running = false;
  s_ui.done = false;
  screen_strip_obj_chrome(scr);
  lv_obj_set_size(scr, kPanelW, kPanelH);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E1116), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Header.
  lv_obj_t* header = lv_obj_create(scr);
  screen_strip_obj_chrome(header);
  lv_obj_set_size(header, kPanelW, 90);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "学习  (L0 App Shell)");
  lv_obj_set_style_text_font(title, &font_puhui_30_4, 0);
  lv_obj_center(title);

  // Mock task card.
  lv_obj_t* card = lv_obj_create(scr);
  screen_strip_obj_chrome(card);
  lv_obj_set_size(card, kPanelW - 80, 160);
  lv_obj_set_pos(card, 40, 140);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* mock = lv_label_create(card);
  lv_label_set_text(mock, "今日任务（Mock）\n口算练习  约 20 分钟");
  lv_obj_set_style_text_font(mock, &font_puhui_30_4, 0);
  lv_obj_center(mock);

  // Start toggle.
  MakeButton(scr, 220, 360, 280, 110, "开始", OnStartClick);

  // Status line.
  s_ui.status_label = lv_label_create(scr);
  lv_label_set_text(s_ui.status_label, "未开始");
  lv_obj_set_style_text_font(s_ui.status_label, &font_puhui_20_4, 0);
  lv_obj_set_pos(s_ui.status_label, 40, 540);

  // Back to Home.
  MakeButton(scr, 40, 620, 200, 70, "返回", [](lv_event_t*) { GoHome(); });

  return scr;
}
