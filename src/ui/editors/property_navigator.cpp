#include "openosc/ui/editors/property_navigator.hpp"
#include "openosc/platform/input.hpp"

namespace openosc::ui {
void PropertyNavigator::restore(PropertyList &list) const { list.cancelEdit(row_, saved_); }
bool PropertyNavigator::cancel(PropertyList &list) {
    const bool number = mode_ == Mode::Number;
    if (number) restore(list);
    mode_ = Mode::Browse;
    return number;
}
PropertyNavigator::Outcome PropertyNavigator::keys(uint32_t keys, PropertyList &list) {
    const unsigned count = list.count();
    if (!keys || !count) return Outcome::None;
    if (row_ >= count) row_ = count - 1;
    const auto kind = list.kind(row_);
    switch (mode_) {
    case Mode::Choice: {
        const unsigned options = list.options(row_);
        if (keys & (Keypad::F4 | Keypad::Left)) { mode_ = Mode::Browse; return Outcome::None; }
        if ((keys & Keypad::Up) && options) draft_ = (draft_ + options - 1) % options;
        if ((keys & Keypad::Down) && options) draft_ = (draft_ + 1) % options;
        if (keys & Keypad::Menu) {
            mode_ = Mode::Browse;
            if (draft_ == list.option(row_)) return Outcome::None;
            return list.choose(row_, draft_) ? Outcome::Changed : Outcome::Refused;
        }
        return Outcome::None;
    }
    case Mode::Number:
        if (keys & Keypad::F4) { restore(list); mode_ = Mode::Browse; return Outcome::Restored; }
        if (keys & Keypad::Menu) { mode_ = Mode::Browse; return list.edited(row_, saved_) ? Outcome::Changed : Outcome::None; }
        if (keys & (Keypad::Right | Keypad::Up)) { list.adjust(row_, 1); return Outcome::Stepped; }
        if (keys & (Keypad::Left | Keypad::Down)) { list.adjust(row_, -1); return Outcome::Stepped; }
        return Outcome::None;
    case Mode::Browse: break;
    }
    if (keys & (Keypad::F4 | Keypad::Left)) return Outcome::Back;
    if (keys & Keypad::Up) { row_ = (row_ + count - 1) % count; return Outcome::None; }
    if (keys & Keypad::Down) { row_ = (row_ + 1) % count; return Outcome::None; }
    if ((keys & Keypad::Right) && kind == PropertyKind::Page) return Outcome::Open;
    if (!(keys & Keypad::Menu)) return Outcome::None;
    switch (kind) {
    case PropertyKind::Choice:
        if (!list.options(row_)) return Outcome::None;
        // A value off the list (a legacy one) starts the draft on the first option; it stays as it
        // is until an option is taken.
        mode_ = Mode::Choice; draft_ = list.option(row_) < list.options(row_) ? list.option(row_) : 0;
        return Outcome::None;
    case PropertyKind::Toggle: return list.choose(row_, list.option(row_) ? 0 : 1) ? Outcome::Changed : Outcome::Refused;
    case PropertyKind::Number: mode_ = Mode::Number; saved_ = list.number(row_); list.beginEdit(row_); return Outcome::None;
    case PropertyKind::Action: return Outcome::Activate;
    case PropertyKind::Page: return Outcome::Open;
    case PropertyKind::Info: return Outcome::None;
    }
    return Outcome::None;
}
} // namespace openosc::ui
