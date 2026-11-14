#pragma once
#include "openosc/ui/editors/property_list.hpp"
#include "openosc/calibration/zero_operation.hpp"

namespace openosc::ui {
class ZeroProgress final : public PropertyList {
public:
    ZeroProgress(const calibration::ZeroProcedure &process,calibration::ZeroOperation::Phase phase) : process_(process),phase_(phase) {}
    const char *title() const override { return process_.title(); }
    const char *notice() const override { return process_.requirement(); }
    unsigned count() const override { return 4; }
    PropertyKind kind(unsigned row) const override { return row==3 ? PropertyKind::Action : PropertyKind::Info; }
    void name(unsigned row,char *output,size_t capacity) const override;
    void value(unsigned row,char *output,size_t capacity) const override;
private:
    const calibration::ZeroProcedure &process_;
    calibration::ZeroOperation::Phase phase_;
};
} // namespace openosc::ui
