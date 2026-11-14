#include "openosc/ui/screens/scope_screen.hpp"

namespace openosc::ui {
void ScopeScreen::counters(const scope::CounterSnapshot &snapshot) {
    if (countersValid_ && counters_.sequence==snapshot.sequence && counters_.enabled==snapshot.enabled) return;
    counters_=snapshot; countersValid_=true;
    if (host_.theme().layout!=LayoutPreset::Legacy) {
        if (!valid_)
            for (const auto &channel : measurements_.values) for (auto kind : channel)
                if (kind==unsigned(scope::Measurement::CounterFrequency)) host_.elements().text(ElementId::Measurements,"");
        host_.scopeFooter().counters(counters_);
    }
}
} // namespace openosc::ui
