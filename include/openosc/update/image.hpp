#pragma once
#include "openosc/update/layout.hpp"
#include <cstddef>

namespace openosc {
inline constexpr uint8_t ImageMagic[] = {'O','P','E','N','O','S','C','1'};
struct ImageHeader {
    uint8_t magic[8];
    uint32_t format;
    uint32_t headerBytes;
    uint32_t boardId;
    uint32_t loadAddress;
    uint32_t payloadBytes;
    uint32_t payloadCrc;
    char version[32];
    uint32_t headerCrc;
    uint8_t reserved[layout::HeaderBytes - 68];
};
static_assert(sizeof(ImageHeader) == layout::HeaderBytes);
static_assert(offsetof(ImageHeader, headerCrc) == 64);

inline constexpr uint8_t TrailerMagic[] = {'O','S','C','T','R','L','R','1'};
// Appended after the linked program in every application BIN, so a BIN names its
// board without moving a single byte of the program.
struct ImageTrailer {
    uint8_t magic[8];
    uint32_t boardId;
    uint32_t loadAddress;
    uint32_t programBytes;
    uint32_t programCrc;
    uint32_t trailerCrc;
};
static_assert(sizeof(ImageTrailer) == 28);

enum class ImageError {
    None, Header, Board, Range, HeaderCrc, Vectors, Marker
};
class ImageValidator {
public:
    static ImageError header(const ImageHeader& image, uint32_t fileBytes);
    static ImageError vectors(const ImageHeader& image, uint32_t sp, uint32_t pc);
    static ImageError trailer(const ImageTrailer& trailer, uint32_t fileBytes);
    static const char* message(ImageError error);
    static uint32_t headerCrc(const ImageHeader& image);
};
}

