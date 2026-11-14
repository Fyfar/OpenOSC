#include "openosc/platform/resources.hpp"
#include "openosc/platform/critical_section.hpp"

namespace openosc {
PeripheralLease::~PeripheralLease() { release(); }
void PeripheralLease::release() {
    if (owner_) {
        CriticalSection guard;
        owner_->claimed_.store(owner_->claimed_.load(std::memory_order_relaxed) & ~mask_,
                               std::memory_order_release);
        owner_ = nullptr;
        mask_ = 0;
    }
}
bool PeripheralResources::acquire(uint32_t mask, PeripheralLease &lease) {
    CriticalSection guard;
    if (!mask || lease.valid())
        return false;
    const uint32_t expected = claimed_.load(std::memory_order_acquire);
    if (expected & mask)
        return false;
    claimed_.store(expected | mask, std::memory_order_release);
    lease.owner_ = this;
    lease.mask_ = mask;
    return true;
}
} // namespace openosc
