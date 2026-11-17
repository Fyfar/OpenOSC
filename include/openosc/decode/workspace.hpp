#pragma once
#include "openosc/decode/job.hpp"
#include "openosc/decode/csv_export.hpp"

namespace openosc::decoding {
// The decoder's one job (its lease and both result sets), in ITCM (.decode_workspace, plan §5.4):
// outside DTCM and the AXI/AHB capture memory. The CPU and MDMA reach ITCM, DMA1/2 and DMA2D do
// not; the decoder itself uses only the CPU. Reset_Handler zeroes the section (ECC); the job is
// constructed in it on the first call. Native builds place it in an ordinary section.
DecodeJob &workspaceJob();
CsvExport &workspaceExport(); // The one CSV export (its stream and the hold of a result).
} // namespace openosc::decoding
