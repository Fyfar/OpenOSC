#include "openosc/instrument/data_access.hpp"

namespace openosc {
bool DataAccess::serialMeter(bool enabled) {
    if (!enabled) return serial_.stop();
    if (!active_ || stopping_ || measurement_!=MeasurementMode::Multimeter) return false;
    return serial_.start(meter_.latest().sequence);
}
} // namespace openosc
