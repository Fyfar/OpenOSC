#include "openosc/calibration/zero_file.hpp"
#include "openosc/storage/directories.hpp"
#include "openosc/storage/layout.hpp"
#include "openosc/calibration/zero_json.hpp"
#include "openosc/storage/json_files.hpp"
#include "openosc/calibration/profile_store.hpp"

namespace openosc::calibration {
bool ZeroFile::save(const ZeroProfile &values) {
    error_=nullptr;
    if (!values.valid() || values.factoryCrc!=factoryCrc_) { error_="Invalid zero correction"; return false; }
    if (!fs_.mounted() || storage::ensureDirectory(storage::Layout::Calibration) != FR_OK) { error_="Calibration disk unavailable"; return false; }
    char json[ZeroJson::MaxBytes+1];
    const size_t bytes=ZeroJson::encode(values,json,sizeof json);
    if (!bytes || !JsonFiles::write(Path,json,bytes)) { error_="Cannot save scope zero JSON"; return false; }
    values_=values; loaded_=true;
    return true;
}
bool ZeroFile::load(uint16_t factoryCrc) {
    loaded_=read(factoryCrc);
    return loaded_;
}
bool ZeroFile::read(uint16_t factoryCrc) {
    factoryCrc_=factoryCrc; error_=nullptr; repaired_=false;
    if (values_.factoryCrc!=factoryCrc) { values_={}; values_.factoryCrc=factoryCrc; }
    if (!fs_.mounted()) { error_="Calibration disk unavailable"; return false; }
    char json[ZeroJson::MaxBytes+1];
    ZeroProfile candidate; candidate.factoryCrc=factoryCrc;
    using Load=ProfileStore::Load;
    switch (ProfileStore::load(Path,Previous,json,sizeof json,repaired_,[&](const char *text,size_t bytes) {
                return ZeroJson::parse(text,bytes,factoryCrc,candidate); })) {
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
