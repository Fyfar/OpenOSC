#include "openosc/ui/scope/painter.hpp"

namespace openosc::plot {
Path::Path(lv_layer_t *layer, const lv_area_t &area, uint32_t color, uint8_t opacity, unsigned width,
           Statistics &statistics)
    : layer_(layer), area_(area), statistics_(statistics) {
    lv_draw_line_dsc_init(&descriptor_);
    descriptor_.color = lv_color_hex(color);
    descriptor_.opa = opacity;
    descriptor_.width = width;
    descriptor_.raw_end = true;
}
void Path::flush(bool keep) {
    if (count_ >= 2) {
        descriptor_.points = points_;
        descriptor_.point_cnt = count_;
        lv_draw_line(layer_, &descriptor_);
        ++statistics_.drawBatches;
    }
    if (keep && count_) {
        points_[0] = points_[count_ - 1];
        count_ = 1;
    } else
        count_ = 0;
}
void Path::vertex(int x, int y) {
    const lv_point_precise_t point = {lv_value_precise_t(area_.x1 + x), lv_value_precise_t(area_.y1 + y)};
    if (count_ && points_[count_ - 1].x == point.x && points_[count_ - 1].y == point.y)
        return;
    if (count_ == 64)
        flush(true);
    points_[count_++] = point;
}
void Path::gap() {
    if (!count_ || points_[count_ - 1].x == LV_DRAW_LINE_POINT_NONE)
        return;
    if (count_ == 64)
        flush(false);
    points_[count_++] = {LV_DRAW_LINE_POINT_NONE, LV_DRAW_LINE_POINT_NONE};
}
} // namespace openosc::plot
