#pragma once
#include <atomic>
#include <cstdint>

namespace openosc {
class PeripheralResources;
class PeripheralLease {
  public:
    ~PeripheralLease();
    PeripheralLease() = default;
    PeripheralLease(const PeripheralLease &) = delete;
    PeripheralLease &operator=(const PeripheralLease &) = delete;
    bool valid() const { return owner_ != nullptr; }
    void release();

  private:
    friend class PeripheralResources;
    PeripheralResources *owner_ = nullptr;
    uint32_t mask_ = 0;
};
class PeripheralResources {
  public:
    explicit PeripheralResources(uint32_t reserved=0) : claimed_(reserved) {}
    static constexpr uint32_t StorageDma=(1U<<11)|(1U<<12);
    static constexpr uint32_t ScopeDma = (1U << 0) | (1U << 1) | (1U << 8) | (1U << 9);
    // A split capture ring's pacing streams DMA1 S2/S3 and DMA2 S6 and their sink DMA1 S7, which is
    // never enabled (HistoryCopy). The lease also stands for what only HistoryCopy uses: MDMA channels
    // 0-3 and DMAMUX1 request generators 0-2. DMA2 S7 is no pacer: lane 3 copies on lane 1's trigger.
    static constexpr uint32_t ScopeHistory = (1U << 2) | (1U << 3) | (1U << 7) | (1U << 14);
    static constexpr uint32_t DmmDma = 1U << 0, GeneratorDma = 1U << 16;
    static constexpr uint32_t DmmSerialDma=1U<<13,OutputJack=1U<<24;
    static constexpr uint32_t ScopeCounters=(1U<<20)|(1U<<21)|(1U<<22);
    static constexpr uint32_t ScopeTrigger=1U<<23;
    bool acquire(uint32_t mask, PeripheralLease &lease);
    static_assert(!(ScopeHistory & (ScopeDma | StorageDma | DmmDma | GeneratorDma | DmmSerialDma | OutputJack | ScopeCounters | ScopeTrigger)),
                  "The pacing streams are no other user's");

  private:
    friend class PeripheralLease;
    std::atomic<uint32_t> claimed_{0};
};
} // namespace openosc
