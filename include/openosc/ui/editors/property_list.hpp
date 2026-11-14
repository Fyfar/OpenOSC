#pragma once
#include <cstddef>
#include <cstdint>
#include "openosc/generator/waveform.hpp"

namespace openosc::ui {
// What a row is, so that one navigator and one renderer serve every menu (PropertyNavigator):
// Choice: one of options() named values, picked in a list; Toggle: Off/On; Number: stepped with
// adjust(); Action: does something (or asks first); Page: opens a child list; Info: shows a value.
enum class PropertyKind : uint8_t { Choice, Toggle, Number, Action, Page, Info };
class PropertyList {
public:
    virtual ~PropertyList() = default;
    virtual const char *title() const = 0;
    virtual const char *notice() const { return nullptr; }
    virtual const generator::Settings *preview() const { return nullptr; }
    virtual unsigned count() const = 0;
    virtual PropertyKind kind(unsigned row) const = 0;
    virtual void name(unsigned row, char *output, size_t capacity) const = 0;
    // The row's current (draft) value; empty for actions and pages.
    virtual void value(unsigned row, char *output, size_t capacity) const;
    // Choice: how many options, which one is current, their names. Toggle: two, Off and On.
    virtual unsigned options(unsigned row) const { return kind(row) == PropertyKind::Toggle ? 2 : 0; }
    virtual unsigned option(unsigned) const { return 0; }
    virtual void optionName(unsigned row, unsigned index, char *output, size_t capacity) const;
    // Makes option `index` current; false when the list refuses it (notice() says why). By
    // default by stepping adjust() towards it.
    virtual bool choose(unsigned row, unsigned index);
    // Number (and the default choose): one step up or down.
    virtual void adjust(unsigned, int) {}
    // Number: its value, and setting it back to one it had (cancel); the slider range where it has one.
    virtual int32_t number(unsigned) const { return 0; }
    virtual void setNumber(unsigned, int32_t) {}
    virtual bool range(unsigned, int32_t &, int32_t &) const { return false; }
    // A Number edit finished with MENU changed the field (Changed, else nothing to commit): by
    // default its number differs from the one it began with.
    virtual bool edited(unsigned row, int32_t saved) const { return number(row) != saved; }
    // A Number edit begins, and is cancelled (F4, MODE): by default the number it began with is set
    // back; a list whose steps change other values too (GEN: frequency, level, codes) keeps all of
    // them at beginEdit and restores them at cancelEdit.
    virtual void beginEdit(unsigned) {}
    virtual void cancelEdit(unsigned row, int32_t saved) { setNumber(row, saved); }
};
} // namespace openosc::ui
