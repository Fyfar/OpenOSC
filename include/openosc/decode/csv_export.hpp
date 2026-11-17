#pragma once
#include "openosc/decode/export/csv_stream.hpp"
#include "openosc/decode/settings.hpp"

namespace openosc::decoding {
// The one export of a decoded result at a time (plan §9 stage 7; docs/SCOPE_PROTOCOL_DECODERS_CSV.md):
// it holds the published ResultSet immutably (ResultSets::acquire) and streams it as CSV to its owner,
// a USB SCPI session (DECode:DATA?) or the local file (DECode:SAVE, the menu). Lives in the decoder
// workspace (ITCM) with the job: a reader keeps only the ticket begin() gave. Non-blocking: every call
// does a bounded amount. The hold ends with end(), cancel(), or when the job revokes it (Off, a new
// source): the next read() then gives 0 with done() false (cancelled), and nothing of the set is read
// after that. Each read() checks its ticket and the hold before touching the set.
class CsvExport {
public:
    enum class Owner : uint8_t { None, Usb, File };
    enum class Refusal : uint8_t { None, Busy, NoResult, Format };
    static constexpr uint32_t MeasureChunk = 4096; // Bytes measure() generates a call.
    // Holds the published result (Done, with events or not) and begins its CSV. 0: refused (refusal()).
    uint32_t begin(Owner owner, ResultSets &sets, const Settings &settings, const char *firmware);
    // The file's total size for a definite block: generated once more in chunks (any split gives the
    // same bytes), then begun again on the same held set. 1: known (total), 0: not yet, -1: cancelled.
    int measure(uint32_t ticket, uint32_t &total);
    size_t read(uint32_t ticket, uint8_t *output, size_t capacity);
    bool done(uint32_t ticket) const { return ticket && ticket == ticket_ && stream_.done(); }
    void end(uint32_t ticket); // Only the owner's ticket ends it: the stream first, then the hold.
    void cancel();             // Whatever owns it.
    bool active() const { return ticket_ != 0; }
    Owner owner() const { return owner_; }
    Refusal refusal() const { return refusal_; }
    const ResultSet *set() const { return sets_ && ticket_ ? sets_->leased(lease_) : nullptr; }
    // The file's context: time from the trigger when the record has one, each source line's role and
    // channel, and its threshold only when the settings in force are those the set was decoded with.
    static exporting::CsvContext context(const ResultSet &set, const Settings &settings, const char *firmware);
private:
    bool held();
    exporting::DecodeCsvStream stream_;
    exporting::CsvContext context_;
    ResultSets *sets_ = nullptr;
    ResultLease lease_;
    uint32_t ticket_ = 0, nextTicket_ = 0, measured_ = 0;
    Owner owner_ = Owner::None;
    Refusal refusal_ = Refusal::None;
    bool counting_ = false; // measure(): the first pass, its bytes only counted.
};
} // namespace openosc::decoding
