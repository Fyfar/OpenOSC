#include "openosc/ui/application_view.hpp"

namespace openosc {
void ApplicationView::images(const ui::ImageList &images) {
    menuFrame("SCREENSHOTS");
    imageBrowser_.list(menu_,*theme_,images);
}
void ApplicationView::imagePreview(const char *name,const uint16_t *pixels, bool fullSize) {
    lv_obj_set_hidden(toast_,true); toastActive_=false;
    menuFrame("IMAGE");
    lv_obj_set_pos(menu_,0,fullSize ? 0 : 20);
    lv_obj_set_size(menu_,320,fullSize ? 240 : 220);
    lv_obj_set_style_border_width(menu_,fullSize ? 0 : 1,0);
    imageBrowser_.preview(menu_,*theme_,name,pixels,fullSize);
}
} // namespace openosc
