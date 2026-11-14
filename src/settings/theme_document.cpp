#include "openosc/settings/theme_document.hpp"
#include <new>

namespace openosc {
namespace {
#ifdef OPENOSC_APPLICATION
#define THEME_SCRATCH __attribute__((section(".settings_workspace")))
#else
#define THEME_SCRATCH
#endif
alignas(ThemeDocument) uint8_t documentStorage[sizeof(ThemeDocument)] THEME_SCRATCH;
char inputStorage[ThemeScratch::InputCapacity] THEME_SCRATCH;
}
ThemeDocument &ThemeScratch::document() {
    // Reconstruct on every operation: SRAM4 is NOLOAD and need not be zeroed.
    return *new (documentStorage) ThemeDocument;
}
char *ThemeScratch::input() { return inputStorage; }
} // namespace openosc
