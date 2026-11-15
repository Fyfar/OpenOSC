#pragma once
#include "openosc/storage/json_profile.hpp"

namespace openosc::calibration {
// The calibration files read their current, else previous, profile through the shared JsonProfile.
using ProfileStore = JsonProfile;
} // namespace openosc::calibration
