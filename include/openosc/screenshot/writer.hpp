#pragma once
#include "openosc/screenshot/bmp.hpp"
#include "openosc/storage/filesystem.hpp"

namespace openosc {
class ScreenshotWriter {
public:
    explicit ScreenshotWriter(Filesystem& fs) : fs_(fs) {}
    bool start(const uint16_t* frame);
    void service();
    bool busy() const { return open_; }
    bool succeeded() const { return complete_; }
    unsigned percent() const { return position_ * 100ULL / BmpImage::FileBytes; }
    const char* error() const { return error_; }
    const char* path() const { return final_; }
private:
    bool fail(const char* message);
    Filesystem& fs_;
    BmpImage image_{nullptr};
    FIL file_{};
    bool open_ = false, complete_ = false;
    uint32_t position_ = 0, nextId_ = 1;
    const char* error_ = nullptr;
    char temporary_[40]{}, final_[40]{};
    alignas(4) uint8_t buffer_[4096];
};
}
