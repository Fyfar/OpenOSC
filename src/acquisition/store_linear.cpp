#include "openosc/acquisition/store.hpp"
#include "openosc/platform/board.hpp"

namespace openosc::scope {
// A Roll snapshot: the chronological history copied into the arena's snapshot bank, so the
// history can keep growing while readers hold the picture. It replaces the frame on show (the
// previous snapshot, or the last record or trial frame), which nobody reads at this point.
bool FrameStore::beginLinear(const FrameView &source,uint32_t now) {
    if (!writable() || !source.valid() || source.metadata().layout!=Layout::Linear ||
        source.metadata().samples>AcquisitionArena::RollWindow || source.bank(0)==AcquisitionArena::rollSnapshot() ||
        !linearPlan_.begin(source.metadata().origin,source.metadata().ring,source.metadata().samples))
        return false;
    if (hasFrame_ && region_!=AcquisitionArena::RollSnapshot) {
        if (!unpublish()) return false;
        arena_.release(region_); region_=0; hasFrame_=consumed_=false;
    }
    const bool reuse=hasFrame_;
    if (!reuse && !arena_.claim(AcquisitionArena::RollSnapshot)) return false;
    if (!unpublish() || !prepare(now)) { if (!reuse) arena_.release(AcquisitionArena::RollSnapshot); return false; }
    region_=AcquisitionArena::RollSnapshot; copySource_=source;
    if (startLinear()) return true;
    halt(); mailbox_.cancel(); arena_.release(region_); region_=0; return false;
}
bool FrameStore::startLinear() {
    LinearCopyPlan::Span span;
    if (!linearPlan_.next(span)) return false;
    DMA2_Stream2->PAR=reinterpret_cast<uintptr_t>(copySource_.bank(0)+span.source);
    DMA2_Stream2->M0AR=reinterpret_cast<uintptr_t>(AcquisitionArena::rollSnapshot()+span.destination);
    DMA2_Stream2->NDTR=span.count;
    __DMB(); DMA2_Stream2->CR|=DMA_SxCR_EN;
    return true;
}
} // namespace openosc::scope
