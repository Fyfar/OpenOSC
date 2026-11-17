#pragma once
#include "openosc/decode/csv_export.hpp"
#include "openosc/storage/filesystem.hpp"

namespace openosc {
// DECode:SAVE and the decoder menu's Save CSV (plan §9 stage 7): the held result's CSV into
// /openosc/DECODE/DECnnnnn.CSV, its directory created on first use. start() only queues the work;
// service(), a bounded step a main-loop pass, then: makes the directory (one step), looks for a free
// number (NamesPerService names a step: a directory full of earlier files never stalls the UI or the
// watchdog), creates DECnnnnn.TMP with FA_CREATE_NEW (an existing name is never overwritten; an
// unfinished .TMP keeps its number taken), writes Chunk bytes a step, then syncs, closes and renames:
// the final name appears only for a whole file. The CSV's size is not known before (no free-space
// bound is claimed): a volume that fills up, any write, sync or rename error, a cancel or a revoked
// hold (Off, a new source) closes and removes the temporary file, says why, and ends the export.
// The instance lives in the decoder workspace (ITCM: FatFs copies through its window, the NOR driver
// through its own DMA buffers).
class DecodeCsvFile {
public:
    static constexpr unsigned Chunk = 512, NamesPerService = 8, MaximumId = 99999;
    bool start(decoding::CsvExport &csv, uint32_t ticket, Filesystem &fs);
    void service();
    void cancel(const char *why = "DECODE CSV CANCELLED");
    bool busy() const { return phase_ != Phase::Idle; }
    bool succeeded() const { return complete_; }
    const char *error() const { return error_; }
    const char *path() const { return final_; } // The last file, once succeeded().
    uint32_t written() const { return written_; }
private:
    enum class Phase : uint8_t { Idle, Directory, Naming, Writing };
    bool fail(const char *message);
    void name();
    void write();
    decoding::CsvExport *csv_ = nullptr;
    FIL file_{};
    uint32_t ticket_ = 0, written_ = 0, id_ = 0;
    Phase phase_ = Phase::Idle;
    bool complete_ = false;
    const char *error_ = nullptr;
    char temporary_[40]{}, final_[40]{};
    uint8_t buffer_[Chunk];
};
DecodeCsvFile &decodeCsvFile(); // The instance in the decoder workspace.
} // namespace openosc
