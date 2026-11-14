#pragma once
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

private:
    lv_obj_t *root_ = nullptr, *name_ = nullptr, *value_ = nullptr;
    lv_obj_t *toggle_ = nullptr, *slider_ = nullptr;
    uint32_t accent_;
};
} // namespace openosc::ui
