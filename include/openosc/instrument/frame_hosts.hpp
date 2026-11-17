#pragma once
#include <cstdint>

namespace openosc {
// The USB readers of scope records (one per SCPI session: USBTMC, CDC, the diagnostic mailbox). In
// a one-slot geometry (compact 64K, 512K) the next capture needs the memory of the record on show,
// and the view has usually seen it long before a host asks for it. A host that is reading (demand)
// and has not yet leased the accepted record on show is owed that record: its memory stays until
// the host takes it, the host goes quiet (DemandMs) or the record has waited HoldMs. Once taken,
// nothing waits: the host's own lease keeps the record while it reads, and its release frees it.
// Without a demanding host nothing changes. Foreground only.
class FrameHosts {
public:
    static constexpr unsigned Capacity = 3;
    static constexpr uint32_t DemandMs = 2000, HoldMs = 2000;
    static constexpr unsigned None = Capacity;
    // A slot for one session's backend; None when every slot is taken (that host is then never owed).
    unsigned attach() { return attached_ < Capacity ? attached_++ : None; }
    // The host asks for records: ACQ:FRAME?, RUN, SINGle, TFORce, a waveform read.
    void demand(unsigned host, uint32_t now) {
        if (host >= attached_) return;
        hosts_[host].active = true; hosts_[host].demandAt = now;
    }
    // The host leased the record `sequence`, or passed over it (RUN/SINGle re-arm): no longer owed to it.
    void taken(unsigned host, uint32_t sequence) {
        if (host < attached_) { hosts_[host].taken = sequence; hosts_[host].hasTaken = true; }
    }
    // RUN/SINGle from this host: it wants the record the new arm makes, not `shown` (the accepted record
    // on show, read or not), so that record never holds the new arm's capture for it (Root HIL
    // 2026-11-13: an unread Single record held RUN's first Normal capture for HoldMs). Other hosts keep theirs.
    void rearm(unsigned host, uint32_t shown, uint32_t now) { taken(host, shown); demand(host, now); }
    // The session ended (reset, disconnect, new session): it is owed nothing.
    void end(unsigned host) { if (host < attached_) hosts_[host] = {}; }
    // Whether a host asks for records now (demanded within DemandMs).
    bool demanding(uint32_t now) const {
        for (unsigned h = 0; h < attached_; ++h)
            if (hosts_[h].active && uint32_t(now - hosts_[h].demandAt) < DemandMs) return true;
        return false;
    }
    // Whether the accepted record `sequence` (accepted at `acceptedAt`) is still owed to a host.
    bool owed(uint32_t sequence, uint32_t acceptedAt, uint32_t now) const {
        if (uint32_t(now - acceptedAt) >= HoldMs) return false;
        for (unsigned h = 0; h < attached_; ++h) {
            const auto &host = hosts_[h];
            if (host.active && uint32_t(now - host.demandAt) < DemandMs && !(host.hasTaken && host.taken == sequence))
                return true;
        }
        return false;
    }

private:
    struct Host { uint32_t demandAt = 0, taken = 0; bool active = false, hasTaken = false; };
    Host hosts_[Capacity];
    unsigned attached_ = 0;
};
} // namespace openosc
