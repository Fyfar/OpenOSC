#include "openosc/screenshot/bitmap_layout.hpp"
#include "openosc/data/binary.hpp"

namespace openosc::screenshot {
bool BitmapLayout::parse(const uint8_t *header,size_t bytes,uint64_t fileBytes,BitmapLayout &output) {
    if (!header || bytes<54 || header[0]!='B' || header[1]!='M') return false;
    const auto word=[&](unsigned at) { return Binary::readLe(header+at,2); };
    const auto dword=[&](unsigned at) { return Binary::readLe(header+at,4); };
    const uint32_t dib=dword(14), offset=dword(10), compression=dword(30);
    const uint32_t height=dword(22);
    if ((dib!=40 && dib!=52 && dib!=56 && dib!=108 && dib!=124) ||
        dword(18)!=Width || (height!=Height && height!=uint32_t(-int32_t(Height))) || word(26)!=1)
        return false;
    const unsigned bits=word(28);
    if (bits==16) {
        if (compression!=3 || bytes<66 || offset<66 || dword(54)!=0xf800 ||
            dword(58)!=0x07e0 || dword(62)!=0x001f) return false;
    } else if (bits!=24 || compression) return false;
    const uint32_t stride=((Width*bits+31)/32)*4;
    if (offset<14+dib || uint64_t(offset)+uint64_t(stride)*Height>fileBytes) return false;
    // Stock BMPs declare two extra bytes; the actual payload extent is authoritative.
    output={offset,stride,uint16_t(bits),height!=Height};
    return true;
}
void BitmapLayout::previewRow(const uint8_t *source,uint16_t *target, bool fullSize) const {
    const unsigned width=fullSize ? Width : PreviewWidth;
    for (unsigned x=0;x<width;++x) {
        const unsigned input=x*Width/width;
        if (bits==16) target[x]=uint16_t(Binary::readLe(source+input*2,2));
        else {
            const auto *pixel=source+input*3;
            target[x]=uint16_t((pixel[2]>>3)<<11)|uint16_t((pixel[1]>>2)<<5)|(pixel[0]>>3);
        }
    }
}
} // namespace openosc::screenshot
