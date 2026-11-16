#include "openosc/platform/stack_guard.hpp"
#include "openosc/platform/watchdog.hpp"

#if defined(OPENOSC_APPLICATION) && (defined(__arm__) || defined(__thumb__))
extern "C" uint32_t _ebss; // application.ld: the end of .bss, the lowest address the stack may reach.
#endif

namespace openosc {
namespace {
volatile uint32_t *guard = nullptr;
}
void StackGuard::arm(volatile uint32_t *words) {
    guard = words;
    for (size_t i = 0; i < Words; ++i) guard[i] = Pattern;
}
#if defined(OPENOSC_APPLICATION) && (defined(__arm__) || defined(__thumb__))
void StackGuard::arm() { arm(&_ebss); }
#endif
size_t StackGuard::intact() {
    if (!guard) return Words;
    size_t count = 0;
    while (count < Words && guard[count] == Pattern) ++count;
    return count;
}
void StackGuard::check() {
    // Any word gone: the stack reached the guard. It arrives from above (the highest word first), so
    // the record's detail is how many of the lowest words survived (0: it went past the guard).
    if (!guard) return;
    const size_t survived = intact();
    if (survived == Words) return;
    Watchdog::stackOverflow(uint32_t(survived));
}
} // namespace openosc
