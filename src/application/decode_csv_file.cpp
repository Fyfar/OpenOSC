#include "openosc/application/decode_csv_file.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/layout.hpp"
#include <cstdio>
#include <new>

namespace openosc {
namespace {
alignas(DecodeCsvFile) __attribute__((section(".decode_workspace"))) unsigned char fileStorage[sizeof(DecodeCsvFile)];
DecodeCsvFile *instance = nullptr;
} // namespace
DecodeCsvFile &decodeCsvFile() {
    if (!instance) instance = new (fileStorage) DecodeCsvFile;
    return *instance;
}
bool DecodeCsvFile::fail(const char *message) {
    if (phase_ == Phase::Writing) { f_close(&file_); f_unlink(temporary_); }
    phase_ = Phase::Idle; complete_ = false; error_ = message;
    if (csv_) csv_->end(ticket_);
    csv_ = nullptr; ticket_ = 0;
    return false;
}
bool DecodeCsvFile::start(decoding::CsvExport &csv, uint32_t ticket, Filesystem &fs) {
    if (busy()) { csv.end(ticket); error_ = "DECODE CSV BUSY"; return false; }
    csv_ = &csv; ticket_ = ticket; complete_ = false; error_ = nullptr; written_ = 0; id_ = 1; final_[0] = 0;
    if (!fs.mounted() || !fs.refreshFree()) return fail("STORAGE NOT AVAILABLE");
    if (!fs.freeBytes()) return fail("NOT ENOUGH FREE SPACE");
    phase_ = Phase::Directory;
    return true;
}
void DecodeCsvFile::cancel(const char *why) { if (busy()) fail(why); }
void DecodeCsvFile::service() {
    switch (phase_) {
    case Phase::Directory:
        if (storage::ensureDirectory(storage::Layout::Decode) != FR_OK) { fail("CANNOT CREATE DECODE DIRECTORY"); return; }
        phase_ = Phase::Naming;
        return;
    case Phase::Naming: name(); return;
    case Phase::Writing: write(); return;
    case Phase::Idle: return;
    }
}
void DecodeCsvFile::name() {
    for (unsigned tried = 0; tried < NamesPerService; ++tried, ++id_) {
        if (id_ > MaximumId) { fail("DECODE FILE NAME LIMIT REACHED"); return; }
        const unsigned long id = id_;
        std::snprintf(final_, sizeof final_, "%s/DEC%05lu.CSV", storage::Layout::Decode, id);
        std::snprintf(temporary_, sizeof temporary_, "%s/DEC%05lu.TMP", storage::Layout::Decode, id);
        FILINFO info{};
        FRESULT result = f_stat(final_, &info);
        if (result == FR_OK) continue;
        if (result != FR_NO_FILE) { fail("CANNOT CHECK DECODE FILE NAME"); return; }
        result = f_open(&file_, temporary_, FA_WRITE | FA_CREATE_NEW);
        if (result == FR_EXIST) continue; // An unfinished one of before: its number stays taken.
        if (result == FR_DENIED) { fail("DECODE CSV DISK FULL"); return; }
        if (result != FR_OK) { fail("CANNOT CREATE DECODE FILE"); return; }
        phase_ = Phase::Writing;
        return;
    }
}
void DecodeCsvFile::write() {
    const size_t chunk = csv_->read(ticket_, buffer_, sizeof buffer_);
    if (!chunk) {
        if (!csv_->done(ticket_)) { fail("DECODE CSV CANCELLED"); return; } // Off, a new source: revoked.
        const FRESULT synced = f_sync(&file_), closed = f_close(&file_);
        phase_ = Phase::Idle;
        if (synced != FR_OK || closed != FR_OK) { f_unlink(temporary_); fail("DECODE CSV SYNC FAILED"); return; }
        if (f_rename(temporary_, final_) != FR_OK) { f_unlink(temporary_); fail("DECODE CSV FINALIZE FAILED"); return; }
        csv_->end(ticket_); csv_ = nullptr; ticket_ = 0;
        complete_ = true;
        return;
    }
    UINT done = 0;
    const FRESULT result = f_write(&file_, buffer_, UINT(chunk), &done);
    if (result != FR_OK) { fail("DECODE CSV WRITE FAILED"); return; }
    if (done != chunk) { fail("DECODE CSV DISK FULL"); return; } // FatFs: no cluster left.
    written_ += done;
}
} // namespace openosc
