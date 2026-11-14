#pragma once
#include "openosc/update/image_file.hpp"
#include "openosc/display/display.hpp"
#include "ff.h"

namespace openosc {
class UpdateOperation {
public:
    explicit UpdateOperation(Display& screen) : screen_(screen) {}
    bool inspect(const char* path);
    bool install(const char* path);
    bool installed(bool progress = false);
    const ImageHeader& image() const { return image_; }
    bool rawBinary() const { return payloadOffset_ == 0; }
    const char* error() const { return error_; }
private:
    bool validateFile(FIL& file);
    bool fail(const char* message);
    Display& screen_;
    ImageHeader image_{};
    uint32_t payloadOffset_ = sizeof(ImageHeader);
    ImageFile::Kind fileKind_ = ImageFile::Kind::Unsupported;
    bool inspected_ = false;
    const char* error_ = nullptr;
    alignas(16) uint8_t buffer_[4096];
};
}

