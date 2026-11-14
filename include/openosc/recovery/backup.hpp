#pragma once
#include "openosc/display/display.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/storage/filesystem.hpp"

namespace openosc {
class FactoryBackup {
public:
    static constexpr const char* Path = storage::Layout::FactoryBackup;
    FactoryBackup(Display& screen, Filesystem& fs) : screen_(screen), fs_(fs) {}
    bool exists() const;
    bool create(bool overwrite);
    uint32_t crc() const { return crc_; }
    const char* error() const { return error_; }
private:
    bool copy();
    bool verify();
    bool metadata();
    bool fail(const char* message);
    Display& screen_;
    Filesystem& fs_;
    uint32_t crc_ = 0;
    const char* error_ = nullptr;
    alignas(4) uint8_t buffer_[4096];
};
}

