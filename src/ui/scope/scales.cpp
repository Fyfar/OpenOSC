#include "openosc/ui/scope/display_scale.hpp"
#include "openosc/ui/scope/format.hpp"
#include "openosc/ui/scope/painter.hpp"
#include <cstdio>

namespace openosc::plot {
void Painter::scales() {
    const auto &o = model_.options();
    const auto &view = model_.viewport();
    const auto &m = model_.metadata();
    if (!o.showScale || o.cursors.time || o.cursors.voltage || view.width < 160 || view.height < 60)
        return;
    char value[32], label[48];
    for (unsigned c = 0; c < 2; ++c)
        if (m.channels[c].enabled && o.channels[c].visible) {
            const auto scale = DisplayScale::channel(m.channels[c], o.channels[c]);
            if (Format::quantity(scale.perDivision, scale.unit, value, sizeof value)) {
                std::snprintf(label, sizeof label, "%u %s %s", c + 1, DisplayScale::coupling(m.channels[c]), value);
                text(label, 4 + c * 90, view.height - 14, palette_.channels[c], 90);
            }
        }
    if ((o.mode == Mode::Time || o.mode == Mode::SpectrumOverlay) &&
        Format::quantity(DisplayScale::secondsPerDivision(view, m), "s", value, sizeof value)) {
        std::snprintf(label, sizeof label, "%s/div", value);
        text(label, view.width - 98, view.height - 14, palette_.cursor, 96);
    }
}
} // namespace openosc::plot
