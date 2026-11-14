#pragma once
#include "openosc/calibration/zero_calibration.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/storage/filesystem.hpp"

namespace openosc::calibration {
class ZeroFile final : public ZeroStore {
public:
    static constexpr const char *Path=storage::Layout::ZeroFile;
    static constexpr const char *Previous=storage::Layout::ZeroPrevious;
    explicit ZeroFile(Filesystem &fs) : fs_(fs) {}
    bool load(uint16_t factoryCrc);
    bool save(const ZeroProfile &values) override;
    const ZeroProfile &values() const { return values_; }
    const char *error() const { return error_; }
    bool repaired() const { return repaired_; }
    // values() are what the disk holds: the last load or a save succeeded (never after a failed read).
    bool loaded() const { return loaded_; }
    bool available() const override { return fs_.mounted(); }
private:
    bool read(uint16_t factoryCrc);
    Filesystem &fs_;
    ZeroProfile values_;
    uint16_t factoryCrc_=0;
    const char *error_=nullptr;
    bool repaired_=false,loaded_=false;
};
} // namespace openosc::calibration
