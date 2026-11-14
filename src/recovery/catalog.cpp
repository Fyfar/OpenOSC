#include "openosc/recovery/catalog.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/update/image_file.hpp"
#include <cstdio>
#include <cstring>

namespace openosc {
bool ImageCatalog::load(unsigned page) {
    count_ = total_ = 0;
    page_ = page;
    DIR directory{};
    if (f_opendir(&directory, storage::Layout::Update) != FR_OK) return false; // Only here: no root fallback.
    FILINFO info{};
    FRESULT result;
    while ((result = f_readdir(&directory, &info)) == FR_OK && info.fname[0]) {
        if ((info.fattrib & (AM_DIR | AM_HID | AM_SYS))
            || ImageFile::kind(info.fname) == ImageFile::Kind::Unsupported) continue;
        if (total_ >= page * PageSize && count_ < PageSize)
            std::strcpy(names_[count_++], info.fname);
        ++total_;
    }
    FRESULT closed = f_closedir(&directory);
    return result == FR_OK && closed == FR_OK;
}
bool ImageCatalog::path(unsigned index, char* output, unsigned size) const {
    if (index >= count_) return false;
    int length = std::snprintf(output, size, "%s/%s", storage::Layout::Update, name(index));
    return length > 0 && unsigned(length) < size;
}
}
