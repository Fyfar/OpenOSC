#include "openosc/ui/components/status_icons.hpp"

namespace openosc::ui {
namespace {
#include "status_icons.inc"
lv_image_dsc_t alphaImage(const uint8_t *alpha) {
    lv_image_dsc_t image{};
    image.header.magic = LV_IMAGE_HEADER_MAGIC;
    image.header.cf = LV_COLOR_FORMAT_A8;
    image.header.w = StatusIcons::Width;
    image.header.h = StatusIcons::Height;
    image.header.stride = StatusIcons::Width;
    image.data_size = StatusIcons::Width * StatusIcons::Height;
    image.data = alpha;
    return image;
}
const lv_image_dsc_t usbImage = alphaImage(usbIconAlpha), generatorImage = alphaImage(generatorIconAlpha);
lv_obj_t *icon(lv_obj_t *parent, const lv_image_dsc_t *image, int x, int y, uint32_t color) {
    auto *object = lv_image_create(parent);
    lv_image_set_src(object, image);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_style_image_recolor(object, lv_color_hex(color), 0); // An alpha map draws in this colour.
    lv_obj_set_style_image_recolor_opa(object, LV_OPA_COVER, 0);
    return object;
}
static_assert(sizeof usbIconAlpha == StatusIcons::Width * StatusIcons::Height &&
              sizeof generatorIconAlpha == StatusIcons::Width * StatusIcons::Height, "Icon size");
} // namespace
lv_obj_t *StatusIcons::usb(lv_obj_t *parent, int x, int y, uint32_t color) { return icon(parent, &usbImage, x, y, color); }
lv_obj_t *StatusIcons::generator(lv_obj_t *parent, int x, int y, uint32_t color) { return icon(parent, &generatorImage, x, y, color); }
} // namespace openosc::ui
