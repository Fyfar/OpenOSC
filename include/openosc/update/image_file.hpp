#pragma once
#include "openosc/update/image.hpp"

namespace openosc {
class ImageFile {
public:
    enum class Kind { Unsupported, Package, Binary };
    static Kind kind(const char* path);
    // prefix contains up to sizeof(ImageHeader) bytes read from the file.
    static ImageError decode(ImageHeader& prefix, uint32_t prefixBytes, uint32_t fileBytes,
                             Kind kind, uint32_t& payloadOffset);
};
}
