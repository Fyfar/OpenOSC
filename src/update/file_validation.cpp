#include "openosc/update/update_operation.hpp"
#include "openosc/update/crc32.hpp"
#include "openosc/platform/board.hpp"
#include <cstring>

namespace openosc {
bool UpdateOperation::fail(const char* message) { error_ = message; return false; }
bool UpdateOperation::validateFile(FIL& file) {
    UINT count = 0;
    const uint32_t bytes = f_size(&file);
    if (bytes > sizeof image_ + layout::MaxPayload) return fail("IMAGE FILE IS TOO LARGE");
    image_ = {};
    if (f_read(&file, &image_, sizeof image_, &count) != FR_OK)
        return fail("CANNOT READ IMAGE FILE");
    ImageError result = ImageFile::decode(image_, count, bytes, fileKind_, payloadOffset_);
    if (result != ImageError::None) return fail(ImageValidator::message(result));
    ImageTrailer trailer{};
    if (rawBinary()) {
        if (f_lseek(&file, bytes - sizeof trailer) != FR_OK
            || f_read(&file, &trailer, sizeof trailer, &count) != FR_OK || count != sizeof trailer)
            return fail("CANNOT READ BOARD MARKER");
        result = ImageValidator::trailer(trailer, bytes);
        if (result != ImageError::None) return fail(ImageValidator::message(result));
    }
    if (f_lseek(&file, payloadOffset_) != FR_OK) return fail("CANNOT SEEK IMAGE PAYLOAD");
    Crc32 crc;
    uint32_t position = 0;
    while (position < image_.payloadBytes) {
        uint32_t chunk = image_.payloadBytes - position;
        if (chunk > sizeof buffer_) chunk = sizeof buffer_;
        // A raw BIN carries its program CRC, so stop exactly where the program ends.
        if (rawBinary() && position < trailer.programBytes && chunk > trailer.programBytes - position)
            chunk = trailer.programBytes - position;
        if (f_read(&file, buffer_, chunk, &count) != FR_OK || count != chunk)
            return fail("IMAGE FILE IS INCOMPLETE");
        if (!position) {
            uint32_t words[2];
            std::memcpy(words, buffer_, sizeof words);
            result = ImageValidator::vectors(image_, words[0], words[1]);
            if (result != ImageError::None) return fail(ImageValidator::message(result));
        }
        crc.update(buffer_, count);
        position += count;
        if (rawBinary() && position == trailer.programBytes && crc.value() != trailer.programCrc)
            return fail("FULL FILE CRC FAILED");
        screen_.progress("CHECKING FILE", position, image_.payloadBytes);
    }
    if (rawBinary()) {
        image_.payloadCrc = crc.value();
        image_.headerCrc = ImageValidator::headerCrc(image_);
    } else if (crc.value() != image_.payloadCrc) return fail("FULL FILE CRC FAILED");
    error_ = nullptr;
    return true;
}
bool UpdateOperation::inspect(const char* path) {
    inspected_ = false;
    fileKind_ = ImageFile::kind(path);
    if (fileKind_ == ImageFile::Kind::Unsupported) return fail("UNSUPPORTED IMAGE FILE EXTENSION");
    FIL file{};
    if (f_open(&file, path, FA_READ) != FR_OK) return fail("CANNOT OPEN IMAGE FILE");
    bool result = validateFile(file);
    if (f_close(&file) != FR_OK) return fail("CANNOT CLOSE IMAGE FILE");
    inspected_ = result;
    return result;
}
bool UpdateOperation::installed(bool progress) {
    inspected_ = false;
    payloadOffset_ = sizeof image_;
    std::memcpy(&image_, reinterpret_cast<const void*>(layout::ApplicationBase), sizeof image_);
    ImageError result = ImageValidator::header(image_, sizeof image_ + image_.payloadBytes);
    if (result != ImageError::None) return fail(ImageValidator::message(result));
    const auto* words = reinterpret_cast<const uint32_t*>(layout::VectorBase);
    result = ImageValidator::vectors(image_, words[0], words[1]);
    if (result != ImageError::None) return fail(ImageValidator::message(result));
    Crc32 crc;
    for (uint32_t offset = 0; offset < image_.payloadBytes; ) {
        uint32_t chunk = image_.payloadBytes - offset;
        if (chunk > 4096) chunk = 4096;
        crc.update(reinterpret_cast<const void*>(layout::VectorBase + offset), chunk);
        offset += chunk;
        if (progress) screen_.progress("CHECKING INSTALLED IMAGE", offset, image_.payloadBytes);
        Board::service();
    }
    if (crc.value() != image_.payloadCrc) return fail("INSTALLED IMAGE CRC FAILED");
    error_ = nullptr;
    return true;
}
}
