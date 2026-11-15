#include "openosc/ui/application_view.hpp"

namespace openosc {
void ApplicationView::menuFrame(const char *title, bool instrument, bool retained) {
    host_.overlay(instrument);
    lv_obj_set_pos(menu_, 8, instrument ? 62 : 22);
    lv_obj_set_size(menu_, 304, instrument ? 170 : 210);
    lv_obj_set_style_bg_color(menu_, lv_color_hex(shown().panel), 0);
    lv_obj_set_hidden(menu_, false);
    lv_obj_set_style_border_width(menu_,1,0);
    if (!retained || !menuRetained_) {
        lv_obj_clean(menu_); menuContent_.reset(); choicePopup_.reset(); draftPreviewObject_=nullptr;
    }
    menuRetained_=retained;
    menuContent_.begin();
    if (draftPreviewObject_) lv_obj_set_hidden(draftPreviewObject_,true);
    menuAccent_ = instrument ? shown().elements[unsigned(mode_)][unsigned(ElementId::Parameters)].color : shown().accent;
    auto *heading = retained ? menuContent_.text(menu_,title,10,4) : label(menu_, title, 10, 4);
    lv_obj_set_size(heading, 238, 22);
    lv_obj_set_style_text_font(heading, font(14), 0);
    lv_obj_set_style_text_color(heading, lv_color_hex(menuAccent_), 0);
}
void ApplicationView::settings(const ui::SettingsMenu &menu, const ui::SettingsProperties &rows, const ui::PropertyNavigator &navigator,
                               const char *persistence) {
    menuFrame(menu.title(),false,true);
    const int top=ui::SettingsView(menu_, shown(), &menuContent_).menu(menu, rows, navigator, persistence, menuAccent_);
    choice(rows, navigator, top - 4);
}
void ApplicationView::information(const ui::SettingsMenu &menu, const DeviceInfo &info) {
    menuFrame(menu.title());
    ui::SettingsView(menu_, shown()).information(menu, info);
}
} // namespace openosc
