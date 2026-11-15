#pragma once
#include "openosc/ui/components/color_swatch.hpp"
#include "openosc/ui/components/elements.hpp"

namespace openosc::ui {
// Hardware keys edit the model; these native LVGL controls display its state.
class MenuRow {
public:
    MenuRow() = default;
    void update(lv_obj_t *parent, int y, int height, const char *name, const char *value,
                bool selected, const Theme &theme, uint32_t accent, int x = 6, int width = 290);
    void visible(bool value) { if (root_) lv_obj_set_hidden(root_, !value); }
    MenuRow(lv_obj_t *parent, int y, int height, const char *name, const char *value,
            bool selected, const Theme &theme, uint32_t accent);
    void toggle(bool checked);
    void slider(unsigned value, unsigned minimum, unsigned maximum);
    // A colour sample before the name (an option of a colour list) or before the value (a colour
    // row; left of its slider when it has one), framed in the row's text colour. update() hides both.
    void nameSwatch(uint32_t rgb);
    void valueSwatch(uint32_t rgb);

private:
    lv_obj_t *root_ = nullptr, *name_ = nullptr, *value_ = nullptr;
    lv_obj_t *toggle_ = nullptr, *slider_ = nullptr;
    ColorSwatch nameSwatch_, valueSwatch_;
    uint32_t accent_, ink_ = 0;
    int height_ = 0, width_ = 0, valueWidth_ = 0;
};
} // namespace openosc::ui
