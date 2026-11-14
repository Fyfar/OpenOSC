#pragma once
#include "openosc/ui/editors/property_list.hpp"
#include "openosc/ui/scope/controls.hpp"

namespace openosc::ui {
// DISPLAY: the rows of the plot controls' section (Section first, a choice of sections). Each field
// is a draft of the view options until it is committed (PropertyNavigator::Changed), then the owner
// applies it. `fine` steps a number by one (a single press) or by its coarse step (a held key).
class DisplayProperties final : public PropertyList {
public:
    explicit DisplayProperties(plot::Controls &controls, bool fine = true) : controls_(controls), fine_(fine) {}
    const char *title() const override { return "DISPLAY"; }
    // The control row (plot::Controls::Row) of an index.
    unsigned row(unsigned index) const { return controls_.row(index); }
    void value(unsigned index, char *output, size_t capacity) const override {
        if (index < count()) controls_.value(row(index), output, capacity);
        else PropertyList::value(index, output, capacity);
    }
    unsigned options(unsigned index) const override { return index < count() ? controls_.choices(row(index)) : 0; }
    unsigned option(unsigned index) const override { return index < count() ? controls_.choice(row(index)) : 0; }
    void optionName(unsigned index, unsigned option, char *output, size_t capacity) const override {
        controls_.choiceName(row(index), option, output, capacity);
    }
    bool choose(unsigned index, unsigned option) override {
        if (index == 0 && option < controls_.choices(plot::Controls::Section)) { controls_.group = plot::Controls::Group(option); return true; }
        return PropertyList::choose(index, option);
    }
    void adjust(unsigned index, int direction) override { if (index < count()) controls_.adjust(row(index), direction, fine_); }
    int32_t number(unsigned index) const override { return index < count() ? controls_.number(row(index)) : 0; }
    void setNumber(unsigned index, int32_t value) override { if (index < count()) controls_.setNumber(row(index), value); }
    unsigned count() const override { return controls_.count(); }
    PropertyKind kind(unsigned index) const override { return controls_.kind(row(index)); }
    void name(unsigned index, char *output, size_t capacity) const override { controls_.name(row(index), output, capacity); }
private:
    plot::Controls &controls_;
    bool fine_;
};
} // namespace openosc::ui
