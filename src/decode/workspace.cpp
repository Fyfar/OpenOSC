#include "openosc/decode/workspace.hpp"
#include <new>

namespace openosc::decoding {
namespace {
// Raw storage, constructed on first use (the controller's construction, after Reset_Handler zeroed
// ITCM): no static constructor or destructor (the firmware registers none: no __cxa_atexit).
alignas(DecodeJob) __attribute__((section(".decode_workspace"))) unsigned char storage[sizeof(DecodeJob)];
DecodeJob *job = nullptr;
alignas(CsvExport) __attribute__((section(".decode_workspace"))) unsigned char exportStorage[sizeof(CsvExport)];
CsvExport *csv = nullptr;
} // namespace
DecodeJob &workspaceJob() {
    if (!job) job = new (storage) DecodeJob;
    return *job;
}
CsvExport &workspaceExport() {
    if (!csv) csv = new (exportStorage) CsvExport;
    return *csv;
}
} // namespace openosc::decoding
