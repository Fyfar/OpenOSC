#pragma once
#include <cstdio>

namespace openosc {
struct PreferencesResult {
    bool mode = true, scope = true, generator = true, meter = true;
    bool meterRequested = false;
    bool ok() const { return mode && scope && generator && meter; }
    void text(char *output, size_t capacity) const {
        std::snprintf(output, capacity, "Mode %s; Scope %s; GEN %s; DMM %s", mode ? "OK" : "FAIL",
            scope ? "OK" : "FAIL", generator ? "OK" : "FAIL",
            meterRequested ? meter ? "pending" : "FAIL" : "N/A");
    }
};
}
