#pragma once
#include "lvgl.h"
#include "openosc/generator/waveform.hpp"

namespace openosc::ui {
class GeneratorPreview {
public:
    lv_obj_t *create(lv_obj_t *parent);
    void update(const generator::Settings &settings, uint32_t color);

private:
    static void draw(lv_event_t *event);
    lv_obj_t *object_ = nullptr;
    uint16_t samples_[96]{};
    generator::Shape shape_ = generator::Shape::Sine;
    uint16_t duty_ = 0;
    uint32_t color_ = 0;
    bool valid_ = false;
};
} // namespace openosc::ui
