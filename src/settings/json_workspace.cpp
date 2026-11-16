#include "openosc/settings/json_workspace.hpp"
#include <cstdint>

namespace openosc {
namespace {
#ifdef OPENOSC_APPLICATION
#define JSON_SCRATCH __attribute__((section(".settings_workspace")))
#else
#define JSON_SCRATCH
#endif
alignas(8) uint8_t documentStorage[JsonWorkspace::DocumentBytes] JSON_SCRATCH;
char textStorage[JsonWorkspace::TextBytes] JSON_SCRATCH;
}
char *JsonWorkspace::text() { return textStorage; }
void *JsonWorkspace::documentMemory() { return documentStorage; }
} // namespace openosc
