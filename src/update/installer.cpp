#include "openosc/update/update_operation.hpp"
#include "openosc/update/internal_flash.hpp"
#include "openosc/update/crc32.hpp"
#include "openosc/platform/board.hpp"
#include <cstring>

namespace openosc {
namespace {
class FileGuard {
public:
    explicit FileGuard(FIL& file) : file_(file) {}
    ~FileGuard() { f_close(&file_); }
private:
    FIL& file_;
};
class FlashGuard {
public:
    explicit FlashGuard(InternalFlash& flash) : flash_(flash) {}
    ~FlashGuard() { flash_.lock(); }
private:
    InternalFlash& flash_;
};
}

bool UpdateOperation::install(const char* path) {
    if (!inspected_) return fail("INSPECT IMAGE BEFORE INSTALLING");
    const ImageHeader approved = image_;
    const uint32_t approvedOffset = payloadOffset_;
    inspected_ = false;
    fileKind_ = ImageFile::kind(path);
    FIL file{};
    if (f_open(&file, path, FA_READ) != FR_OK) return fail("CANNOT OPEN IMAGE FILE");
    FileGuard fileGuard(file);
    if (!validateFile(file)) return false;
    if (approvedOffset != payloadOffset_ || std::memcmp(&approved, &image_, sizeof image_))
        return fail("IMAGE CHANGED. SELECT IT AGAIN.");
    if (f_lseek(&file, payloadOffset_) != FR_OK) return fail("CANNOT SEEK IMAGE FILE");
    InternalFlash flash;
    FlashGuard flashGuard(flash);
    uint32_t eraseBytes = (sizeof image_ + image_.payloadBytes + layout::SectorBytes - 1)
        & ~(layout::SectorBytes - 1);
    for (uint32_t offset = 0; offset < eraseBytes; offset += layout::SectorBytes) {
        screen_.progress("ERASING APPLICATION", offset, eraseBytes);
        if (!flash.eraseSector(layout::ApplicationBase + offset)) return fail(flash.error());
    }
    uint32_t position = 0;
    while (position < image_.payloadBytes) {
        uint32_t chunk = image_.payloadBytes - position;
        if (chunk > sizeof buffer_) chunk = sizeof buffer_;
        std::memset(buffer_, 0xff, sizeof buffer_);
        UINT count = 0;
        if (f_read(&file, buffer_, chunk, &count) != FR_OK || count != chunk)
            return fail("IMAGE READ FAILED AFTER ERASE");
        uint32_t padded = (chunk + 15) & ~15U;
        if (!flash.program(layout::VectorBase + position, buffer_, padded)) return fail(flash.error());
        position += chunk;
        screen_.progress("WRITING APPLICATION", position, image_.payloadBytes);
    }
    Crc32 written;
    for (uint32_t offset = 0; offset < image_.payloadBytes; ) {
        uint32_t chunk = image_.payloadBytes - offset;
        if (chunk > sizeof buffer_) chunk = sizeof buffer_;
        written.update(reinterpret_cast<const void*>(layout::VectorBase + offset), chunk);
        offset += chunk;
        screen_.progress("VERIFYING FLASH", offset, image_.payloadBytes);
    }
    if (written.value() != image_.payloadCrc) return fail("FULL FLASH CRC FAILED");
    // The validity marker is committed last. Interrupted updates have no valid header.
    auto* header = reinterpret_cast<const uint8_t*>(&image_);
    if (!flash.program(layout::ApplicationBase + 16, header + 16, sizeof image_ - 16)
        || !flash.program(layout::ApplicationBase, header, 16)) return fail(flash.error());
    if (std::memcmp(header, reinterpret_cast<const void*>(layout::ApplicationBase), sizeof image_))
        return fail("IMAGE HEADER READBACK FAILED");
    screen_.progress("UPDATE VERIFIED", image_.payloadBytes, image_.payloadBytes);
    error_ = nullptr;
    return true;
}
}

