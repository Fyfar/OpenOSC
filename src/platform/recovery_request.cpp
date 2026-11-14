#include "openosc/platform/recovery_request.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/handoff.hpp"
#include "openosc/update/layout.hpp"

namespace openosc {
bool RecoveryRequest::take() {
    auto* request = reinterpret_cast<volatile uint32_t*>(Address);
    if (request[0] != Magic || request[1] != ~Magic) return false;
    request[0] = request[1] = 0;
    __DSB();
    return true;
}
[[noreturn]] void RecoveryRequest::restart() {
    auto* request = reinterpret_cast<volatile uint32_t*>(Address);
    request[0] = Magic; request[1] = ~Magic;
    __DSB();
    // Return to our loader directly: the factory's reset startup may clear
    // the retained request before our loader gets to inspect it.
    Handoff::jump(layout::LoaderBase);
}
}
