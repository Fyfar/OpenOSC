#include "openosc/update/image.hpp"
#include "openosc/update/crc32.hpp"
#include <cstring>

namespace openosc {
uint32_t ImageValidator::headerCrc(const ImageHeader& image) {
    Crc32 crc;
    uint32_t zero = 0;
    crc.update(&image, offsetof(ImageHeader, headerCrc));
    crc.update(&zero, sizeof zero);
    crc.update(image.reserved, sizeof image.reserved);
    return crc.value();
}
ImageError ImageValidator::header(const ImageHeader& image, uint32_t fileBytes) {
    if (std::memcmp(image.magic, ImageMagic, sizeof ImageMagic) || image.format != 1
        || image.headerBytes != sizeof image || image.version[31]) return ImageError::Header;
    if (image.boardId != layout::BoardId) return ImageError::Board;
    if (image.loadAddress != layout::VectorBase || image.payloadBytes < 8
        || image.payloadBytes > layout::MaxPayload
        || fileBytes != sizeof image + image.payloadBytes) return ImageError::Range;
    if (image.headerCrc != headerCrc(image)) return ImageError::HeaderCrc;
    return ImageError::None;
}
ImageError ImageValidator::vectors(const ImageHeader& image, uint32_t sp, uint32_t pc) {
    // OpenOSC application ABI: main stack in DTCM, 8-byte aligned.
    if ((sp & 7) || sp <= 0x20000000 || sp > 0x2001ff00) return ImageError::Vectors;
    if (!(pc & 1) || (pc & ~1U) < image.loadAddress + 8
        || (pc & ~1U) >= image.loadAddress + image.payloadBytes) return ImageError::Vectors;
    return ImageError::None;
}
ImageError ImageValidator::trailer(const ImageTrailer& trailer, uint32_t fileBytes) {
    if (std::memcmp(trailer.magic, TrailerMagic, sizeof TrailerMagic)
        || trailer.trailerCrc != Crc32::compute(&trailer, offsetof(ImageTrailer, trailerCrc)))
        return ImageError::Marker;
    if (trailer.boardId != layout::BoardId) return ImageError::Board;
    if (trailer.loadAddress != layout::VectorBase || fileBytes < sizeof trailer
        || trailer.programBytes != fileBytes - sizeof trailer) return ImageError::Range;
    return ImageError::None;
}
const char* ImageValidator::message(ImageError error) {
    switch (error) {
    case ImageError::None: return nullptr;
    case ImageError::Header: return "INVALID OPENOSC IMAGE HEADER";
    case ImageError::Board: return "IMAGE IS FOR A DIFFERENT BOARD";
    case ImageError::Range: return "IMAGE SIZE OR ADDRESS IS INVALID";
    case ImageError::HeaderCrc: return "IMAGE HEADER CRC FAILED";
    case ImageError::Vectors: return "INVALID STACK OR RESET VECTOR";
    case ImageError::Marker: return "BIN HAS NO BOARD MARKER";
    }
    return "IMAGE ERROR";
}
}

