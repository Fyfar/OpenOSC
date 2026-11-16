#include "openosc/calibration/zero_file.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/calibration/zero_json.hpp"
#include "openosc/storage/json_files.hpp"
#include "openosc/calibration/profile_store.hpp"
#include "openosc/settings/json_workspace.hpp"

namespace openosc::calibration {
bool ZeroFile::save(const ZeroProfile &values) {
    error_=nullptr;
    if (!values.valid() || values.factoryCrc!=factoryCrc_) { error_="Invalid zero correction"; return false; }
    if (!fs_.mounted() || storage::ensureDirectory(storage::Layout::Calibration) != FR_OK) { error_="Calibration disk unavailable"; return false; }
    JsonWorkspace::Text json; // Off the stack (JsonWorkspace).
    const size_t bytes=json ? ZeroJson::encode(values,json.data(),ZeroJson::MaxBytes+1) : 0;
    if (!bytes || !JsonFiles::write(Path,json.data(),bytes)) { error_="Cannot save scope zero JSON"; return false; }
    values_=values; loaded_=true;
    return true;
}
bool ZeroFile::load(uint16_t factoryCrc) {
    // A busy workspace is unreadable now: the profile in force (and loaded()), the file and its
    // names stay; the same load passes once the workspace is idle.
    if (!JsonWorkspace::idle()) { error_="Cannot read scope zero JSON"; repaired_=false; return false; }
    loaded_=read(factoryCrc);
    return loaded_;
}
bool ZeroFile::read(uint16_t factoryCrc) {
    factoryCrc_=factoryCrc; error_=nullptr; repaired_=false;
    if (values_.factoryCrc!=factoryCrc) { values_={}; values_.factoryCrc=factoryCrc; }
    if (!fs_.mounted()) { error_="Calibration disk unavailable"; return false; }
    static_assert(ZeroJson::MaxBytes+1<=JsonWorkspace::TextCapacity);
    ZeroProfile candidate; candidate.factoryCrc=factoryCrc;
    using Load=ProfileStore::Load;
    Load load;
    {   // The text is held only while the file is read: a restoring save() takes it again.
        JsonWorkspace::Text json;
        if (!json) { error_="Cannot read scope zero JSON"; return false; }
        load=ProfileStore::load(Path,Previous,json.data(),ZeroJson::MaxBytes+1,repaired_,[&](const char *text,size_t bytes) {
            return ZeroJson::parse(text,bytes,factoryCrc,candidate); });
    }
    switch (load) {
    case Load::Current: values_=candidate; return true;
    case Load::Previous:
        // Recovery after a power loss between the two renames. Keep the
        // verified previous profile usable even if restoring its name fails.
        values_=candidate;
        if (!save(candidate)) error_="Zero loaded; file restore failed";
        return true;
    case Load::StorageError: error_="Cannot read scope zero JSON"; return false;
    case Load::ArchiveError: error_="Cannot preserve invalid zero JSON"; return false;
    case Load::Repaired:
        candidate={}; candidate.factoryCrc=factoryCrc;
        return save(candidate);
    case Load::Missing: break;
    }
    values_={}; values_.factoryCrc=factoryCrc; // An absent optional file means the original factory zero.
    return true;
}
} // namespace openosc::calibration
