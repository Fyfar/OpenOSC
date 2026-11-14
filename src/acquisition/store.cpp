#include "openosc/acquisition/store.hpp"
#include "openosc/platform/board.hpp"
#include "openosc/platform/dma_stream.hpp"
#include "openosc/platform/interrupts.hpp"

namespace openosc::scope {
FrameStore *FrameStore::active_ = nullptr;
namespace {
constexpr uint32_t transferFlags = 0x3dU << 16, completeFlag = 0x20U << 16, errorFlags = 0x0dU << 16;
} // namespace
bool FrameStore::unpublish() {
    // A published frame nobody has read yet is withdrawn; a read one stays until released.
    if (mailbox_.state() == FrameMailbox::State::Capturing) mailbox_.cancel();
    if (mailbox_.state() == FrameMailbox::State::Ready) {
        FrameLease pending;
        if (mailbox_.acquire(pending)) pending.release();
    }
    return mailbox_.state() == FrameMailbox::State::Idle;
}
bool FrameStore::adopt(const FrameView &frame, uint8_t region) {
    if (copying_ || !frame.valid() || !region || !unpublish()) return false;
    if (hasFrame_ && region_ != region) arena_.release(region_);
    latest_ = frame; region_ = region; hasFrame_ = true; consumed_ = inspected_ = false;
    if (!mailbox_.begin() || !mailbox_.publish(latest_)) { mailbox_.cancel(); status_ = DataStatus::InvalidArgument; return true; }
    status_ = DataStatus::Ok;
    return true;
}
bool FrameStore::drop() {
    if (mailbox_.state() == FrameMailbox::State::Leased) return false;
    if (copying_ && !halt()) return false;
    if (!unpublish()) return false;
    if (region_) arena_.release(region_);
    region_ = 0; hasFrame_ = consumed_ = false;
    return true;
}
bool FrameStore::detach(FrameView &frame, uint8_t &region) {
    if (copying_ || !hasFrame_ || !unpublish()) return false;
    frame = latest_; region = region_;
    region_ = 0; hasFrame_ = consumed_ = false;
    return true;
}
bool FrameStore::prepare(uint32_t now) {
    if (!resources_.acquire(1U << 10, peripheral_))
        return false;
    if (!mailbox_.begin()) {
        peripheral_.release();
        return false;
    }
    hasFrame_ = false;
    flags_ = 0;
    active_ = this;
    __HAL_RCC_DMA2_CLK_ENABLE();
    DMA2_Stream2->CR = 0;
    DMA2->LIFCR = transferFlags;
    DMA2_Stream2->FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH_0 | DMA_SxFCR_FTH_1 | DMA_SxFCR_FEIE;
    DMA2_Stream2->CR = DMA_SxCR_DIR_1 | DMA_SxCR_PINC | DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 |
                       DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE | (2U << 16);
    Interrupts::attach(DMA2_Stream2_IRQn, interrupt);
    copying_ = true;
    deadline_ = now + 100;
    status_ = DataStatus::Busy;
    return true;
}
void FrameStore::interrupt() {
    const auto flags = DMA2->LISR & transferFlags;
    DMA2->LIFCR = transferFlags;
    if (!active_)
        return;
    if (flags & errorFlags) {
        active_->flags_ |= flags;
        return;
    }
    if (flags & completeFlag) {
        if (active_->startLinear()) return;
    }
    active_->flags_ |= flags;
}
bool FrameStore::halt() {
    // Detach first: the completion raised by the FIFO flush would chain the next
    // span from the interrupt and re-enable the stream being stopped.
    Interrupts::detach(DMA2_Stream2_IRQn);
    if (!DmaStream::disable(DMA2_Stream2)) {
        status_ = DataStatus::HardwareError;
        return false;
    }
    DMA2->LIFCR = transferFlags;
    __DMB();
    active_ = nullptr;
    peripheral_.release();
    copying_ = false;
    return true;
}
void FrameStore::service(uint32_t now) {
    if (!copying_)
        return;
    if ((flags_ & errorFlags) || (!(flags_&completeFlag) && int32_t(now - deadline_) >= 0)) {
        const bool timeout = !(flags_ & errorFlags);
        if (!halt())
            return;
        mailbox_.cancel();
        arena_.release(region_); region_ = 0;
        status_ = timeout ? DataStatus::Timeout : DataStatus::HardwareError;
        return;
    }
    if (!(flags_ & completeFlag))
        return;
    if (!halt())
        return;
    auto metadata=copySource_.metadata(); metadata.ring=metadata.samples; metadata.origin=0;
    const uint16_t *linearBanks[]={AcquisitionArena::rollSnapshot(),nullptr,nullptr,nullptr};
    latest_=FrameView(metadata,linearBanks,AcquisitionArena::RollWindow);
    hasFrame_ = mailbox_.publish(latest_); consumed_ = inspected_ = false;
    status_ = hasFrame_ ? DataStatus::Ok : DataStatus::InvalidArgument;
}
bool FrameStore::acquireLatest(FrameLease &lease, bool consume) {
    if (lease.valid())
        return false;
    if (mailbox_.acquire(lease)) { consumed_ = consumed_ || consume; return true; }
    if (!hasFrame_ || copying_ || !mailbox_.begin())
        return false;
    if (mailbox_.publish(latest_) && mailbox_.acquire(lease)) { consumed_ = consumed_ || consume; return true; }
    mailbox_.cancel();
    return false;
}
bool FrameStore::acquireUninspected(FrameLease &lease) {
    if (inspected_ || !acquireLatest(lease, false)) return false;
    inspected_ = true;
    return true;
}
} // namespace openosc::scope
