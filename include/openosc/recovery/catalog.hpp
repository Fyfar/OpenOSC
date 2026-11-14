#pragma once
#include "ff.h"
#include <cstdint>

namespace openosc {
class ImageCatalog {
public:
    static constexpr unsigned PageSize = 5;
    bool load(unsigned page);
    unsigned count() const { return count_; }
    unsigned total() const { return total_; }
    unsigned page() const { return page_; }
    const char* name(unsigned index) const { return index < count_ ? names_[index] : ""; }
    bool path(unsigned index, char* output, unsigned size) const;
private:
    unsigned count_ = 0, total_ = 0, page_ = 0;
    char names_[PageSize][FF_MAX_LFN + 1]{};
};
}
