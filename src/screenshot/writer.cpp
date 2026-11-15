#include "openosc/screenshot/writer.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/layout.hpp"
#include <cstdio>

namespace openosc {
bool ScreenshotWriter::fail(const char* message) {
    if (open_) f_close(&file_);
    open_ = false; complete_ = false; error_ = message;
    return false;
}
bool ScreenshotWriter::start(const uint16_t* frame) {
    if (open_) return false;
    complete_ = false; error_ = nullptr; position_ = 0;
    if (!frame || !fs_.mounted() || !fs_.refreshFree()) return fail("STORAGE NOT AVAILABLE");
    if (fs_.freeBytes() < BmpImage::FileBytes + 8192) return fail("NOT ENOUGH FREE SPACE");
    FRESULT result = storage::ensureDirectory(storage::Layout::Screenshots);
    if (result != FR_OK) return fail("CANNOT CREATE SCREENSHOTS DIRECTORY");
    for (; nextId_ <= 99999; ++nextId_) {
        std::snprintf(final_, sizeof final_, "%s/SCR%05lu.BMP", storage::Layout::Screenshots, static_cast<unsigned long>(nextId_));
        std::snprintf(temporary_, sizeof temporary_, "%s/SCR%05lu.TMP", storage::Layout::Screenshots, static_cast<unsigned long>(nextId_));
        FILINFO info{};
        result = f_stat(final_, &info);
        if (result == FR_OK) continue;
        if (result != FR_NO_FILE) return fail("CANNOT CHECK SCREENSHOT NAME");
        result = f_open(&file_, temporary_, FA_WRITE | FA_CREATE_NEW);
        if (result == FR_EXIST) continue;
        if (result != FR_OK) return fail("CANNOT CREATE SCREENSHOT FILE");
        open_ = true; ++nextId_;
        image_ = BmpImage(frame);
        return true;
    }
    return fail("SCREENSHOT NAME LIMIT REACHED");
}
void ScreenshotWriter::cancel() {
    if (!open_) return;
    f_close(&file_);
    f_unlink(temporary_);
    open_ = false; complete_ = false; error_ = "SCREENSHOT CANCELLED";
}
void ScreenshotWriter::service() {
    if (!open_) return;
    const size_t chunk = image_.read(position_, buffer_, sizeof buffer_);
    UINT written = 0;
    if (f_write(&file_, buffer_, chunk, &written) != FR_OK || written != chunk) {
        fail("SCREENSHOT WRITE FAILED"); return;
    }
    position_ += written;
    if (position_ != BmpImage::FileBytes) return;
    const FRESULT synced = f_sync(&file_), closed = f_close(&file_);
    open_ = false;
    if (synced != FR_OK || closed != FR_OK) { fail("SCREENSHOT SYNC FAILED"); return; }
    if (f_rename(temporary_, final_) != FR_OK) { fail("SCREENSHOT FINALIZE FAILED"); return; }
    complete_ = true;
}
}
