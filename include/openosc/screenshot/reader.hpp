#pragma once
#include "openosc/screenshot/bitmap_layout.hpp"
#include "openosc/storage/filesystem.hpp"

namespace openosc::screenshot {
class ImageReader {
public:
    explicit ImageReader(Filesystem &fs) : fs_(fs) {}
    ~ImageReader() { cancel(); }
    bool start(const char *path,uint16_t *preview, bool fullSize = false);
    void service();
    void cancel();
    bool busy() const { return open_; }
    bool ready() const { return ready_; }
    unsigned percent() const { return row_*100/height_; }
    const char *error() const { return error_; }
private:
    bool fail(const char *message);
    Filesystem &fs_;
    FIL file_{};
    BitmapLayout layout_;
    uint16_t *preview_=nullptr;
    unsigned row_=0, width_=BitmapLayout::PreviewWidth, height_=BitmapLayout::PreviewHeight;
    bool open_=false,ready_=false;
    const char *error_=nullptr;
    uint8_t rowData_[BitmapLayout::Width*3];
};
uint16_t *previewMemory();
} // namespace openosc::screenshot
