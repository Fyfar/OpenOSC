#pragma once
#include "openosc/acquisition/frame.hpp"
#include "openosc/ui/scope/options.hpp"

namespace openosc::plot {
struct Viewport {
    int64_t startQ16 = 0;
    uint64_t spanQ16 = 0;
    uint16_t width = 0, height = 0;
    bool calculate(const scope::FrameMetadata &frame, const Options &options, unsigned w, unsigned h);
    int64_t position(unsigned x) const;
    int sampleX(uint32_t index) const;
    // Every record point lands in a column (the window spans at least a point per column).
    bool aggregates() const { return spanQ16 >= uint64_t(width - 1) * 65536; }
    // Column x's record points [first, last) of a record of `samples` (aggregating windows).
    void column(unsigned x, uint32_t samples, uint32_t &first, uint32_t &last) const;
    static int64_t floorSample(int64_t positionQ16) { // The record point at or before a Q16 position.
        return positionQ16 >= 0 ? positionQ16 / 65536 : -((-positionQ16 + 65535) / 65536);
    }
    bool operator==(const Viewport &o) const {
        return startQ16 == o.startQ16 && spanQ16 == o.spanQ16 && width == o.width && height == o.height;
    }
};
struct Axis {
    int32_t zeroQ8 = 0, codesPerDivisionQ8 = 8192;
    int16_t origin = 0;
    int16_t basePermille = 500; // Where the analog zero of this record puts 0 V, without the digital offset.
    int32_t factor = 0;
    bool configure(const scope::ChannelMetadata &metadata, const ChannelOptions &options, unsigned height);
    // A record's basePermille: a calibrated record's codes are compensated for its analog zero, so
    // its line is placed here; raw codes already carry it (500). The digital position that centres
    // this record's zero is then 1000 - basePermille.
    static int16_t base(const scope::ChannelMetadata &metadata);
    int16_t pixel(int32_t codeQ8) const;
    double code(int pixel) const;
};
} // namespace openosc::plot
