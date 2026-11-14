#include "openosc/update/image_file.hpp"
#include <cstring>

namespace openosc {
ImageFile::Kind ImageFile::kind(const char* path) {
    if (!path) return Kind::Unsupported;
    if (const char* slash = std::strrchr(path, '/')) path = slash + 1;
    const size_t length = std::strlen(path);
    if (length < 5 || path[length - 4] != '.') return Kind::Unsupported;
    char extension[4]{};
    for (unsigned i = 0; i < 3; ++i) {
        char c = path[length - 3 + i];
        extension[i] = c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
    }
    if (!std::strcmp(extension, "OSC")) return Kind::Package;
    if (!std::strcmp(extension, "BIN")) return Kind::Binary;
    return Kind::Unsupported;
}
ImageError ImageFile::decode(ImageHeader& header, uint32_t prefixBytes, uint32_t fileBytes,
                             Kind kind, uint32_t& payloadOffset) {
    payloadOffset = sizeof header;
    if (kind == Kind::Unsupported) return ImageError::Header;
    const bool packaged = prefixBytes >= sizeof ImageMagic
        && !std::memcmp(header.magic, ImageMagic, sizeof ImageMagic);
    if (packaged || kind == Kind::Package) {
        if (prefixBytes != sizeof header) return ImageError::Header;
        return ImageValidator::header(header, fileBytes);
    }
    if (fileBytes < 8 + sizeof(ImageTrailer) || fileBytes > layout::MaxPayload) return ImageError::Range;
    header = {};
    std::memcpy(header.magic, ImageMagic, sizeof ImageMagic);
    header.format = 1;
    header.headerBytes = sizeof header;
    header.boardId = layout::BoardId;
    header.loadAddress = layout::VectorBase;
    header.payloadBytes = fileBytes;
    std::strcpy(header.version, "RAW BIN");
    payloadOffset = 0;
    return ImageError::None;
}
}
