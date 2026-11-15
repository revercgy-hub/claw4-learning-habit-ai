#pragma once

struct _lv_obj_t;
using lv_obj_t = _lv_obj_t;

enum class PreviewPage { Home, Timer, Learning };

// The alarm temporarily takes over the screen without changing the current page.
class PreviewPageSelection {
public:
    constexpr void Open(PreviewPage page) { if (!ringing_) page_ = page; }
    constexpr void Close(PreviewPage page) { if (!ringing_ && page_ == page) page_ = PreviewPage::Home; }
    constexpr void SetRinging(bool ringing) { ringing_ = ringing; }
    constexpr PreviewPage Visible() const { return ringing_ ? PreviewPage::Timer : page_; }

private:
    PreviewPage page_ = PreviewPage::Home;
    bool ringing_ = false;
};

// All page and widget operations run with the LVGL lock held.
void Claw4PreviewPagesSetup(lv_obj_t* layer);
lv_obj_t* Claw4PreviewPanelHost();
void Claw4PreviewRegister(PreviewPage page, lv_obj_t* panel);
void Claw4PreviewOpen(PreviewPage page);
void Claw4PreviewClose(PreviewPage page);
void Claw4PreviewSetRinging(bool ringing);
