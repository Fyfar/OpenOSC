#pragma once
#include "openosc/screenshot/bitmap_layout.hpp"
#include "openosc/ui/components/elements.hpp"

namespace openosc::ui {
struct ImageList {
    static constexpr unsigned Rows=5;
    const char *names[Rows]{};
    unsigned visible=0,selected=0,first=0,total=0;
};
class ImageBrowserView {
public:
    void list(lv_obj_t *parent,const Theme &theme,const ImageList &images);
    void preview(lv_obj_t *parent,const Theme &theme,const char *name,const uint16_t *pixels, bool fullSize = false);
private:
    lv_image_dsc_t bitmap_{};
};
} // namespace openosc::ui
