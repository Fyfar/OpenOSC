#include "openosc/ui/scope/model.hpp"
#include "openosc/ui/scope/column_cache.hpp"

namespace openosc::plot {
Storage &applicationStorage() {
    alignas(32) static Storage storage __attribute__((section(".scope_plot")));
    static_assert(sizeof(Storage) < 128 * 1024, "Scope plot exceeds its AXI memory budget");
    return storage;
}
ColumnCache &applicationColumns() {
    static ColumnCache columns; // DTCM: the reduction writes it column by column.
    return columns;
}
scope::SpectrumStorage &applicationSpectrum() {
    // Zero-wait DTCM: the transform would otherwise spend tens of milliseconds in
    // non-cached AXI SRAM, stalling the main loop and the masters that share that bus.
    static scope::SpectrumStorage spectrum;
    return spectrum;
}
} // namespace openosc::plot
