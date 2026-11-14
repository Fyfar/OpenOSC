#pragma once
#include "openosc/ui/editors/property_list.hpp"
#include "openosc/platform/shutdown.hpp"
#include <cstdio>

namespace openosc::ui {
class Confirmation final : public PropertyList {
public:
    Confirmation(const char *title,const char *action,const char *notice=nullptr) : title_(title),action_(action),notice_(notice) {}
    explicit Confirmation(ShutdownAction action) : Confirmation(
        action==ShutdownAction::Recovery ? "OPEN RECOVERY?" : action==ShutdownAction::Restart ? "RESTART DEVICE?" : "POWER OFF?",
        action==ShutdownAction::Recovery ? "Open Recovery" : action==ShutdownAction::Restart ? "Save and restart" : "Power off") {}
    const char *title() const override { return title_; }
    const char *notice() const override { return notice_; }
    unsigned count() const override { return 2; }
    PropertyKind kind(unsigned) const override { return PropertyKind::Action; }
    void name(unsigned row, char *output, size_t capacity) const override {
        std::snprintf(output,capacity,"%s", row == 0 ? "Cancel" : action_);
    }
private:
    const char *title_,*action_,*notice_;
};
} // namespace openosc::ui
