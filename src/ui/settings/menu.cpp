#include "openosc/ui/settings/menu.hpp"

namespace openosc::ui {
void SettingsMenu::open() {
    page_ = SettingsPage::Root;
    for (auto &selected : selected_) selected = 0;
}
bool SettingsMenu::back() {
    if (page_ == SettingsPage::Root) return false;
    page_ = definition().parent;
    return true;
}
SettingsAction SettingsMenu::enter(unsigned row) {
    select(row);
    const auto item = entry(row);
    if (item.child != SettingsPage::Count) page_ = item.child;
    return item.action;
}
const SettingsEntry &SettingsMenu::entry(unsigned row) const {
    static constexpr SettingsEntry empty{"", SettingsAction::None};
    return row < count() ? definition().entries[row] : empty;
}
} // namespace openosc::ui
