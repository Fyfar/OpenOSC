#pragma once
#include <cstdint>

namespace openosc {
// A bounded foreground service for drivers which synchronously wait for I/O.
class CooperativeService {
public:
    using Callback=void(*)(void *,uint32_t);
    static void install(Callback callback,void *context);
    static void remove(void *context);
    static void poll(uint32_t now);
};
} // namespace openosc
