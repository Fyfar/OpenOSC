#pragma once
#include "ff.h"
#include <cstdint>

namespace openosc {
class BlockDevice;
class Filesystem;
namespace fat {
class Formatter {
public:
    enum class State { Idle,Filesystem,Directories,Verify,Complete,Failed };
    Formatter(BlockDevice &disk,openosc::Filesystem &fs) : disk_(disk),fs_(fs) {}
    bool begin(); // Preparation only. The first service call starts changing the volume.
    void service();
    bool busy() const { return state_==State::Filesystem || state_==State::Directories || state_==State::Verify; }
    State state() const { return state_; }
    const char *phase() const;
    unsigned completedSteps() const;
    FRESULT result() const { return result_; }
    const char *error() const { return error_; }
    // The factory boot sets this label in the root entry on every start (f_setlabel("0:FLASH")); the
    // same name in the BPB keeps both labels equal across factory boots.
    static constexpr const char *VolumeLabel="FLASH";
    static constexpr UINT EraseBytes=4096;                          // NOR erase sector.
    static constexpr UINT ClusterBytes[]={4096,8192,16384,32768};   // Tried in order, all erase-aligned.
private:
    bool fail(const char *error,FRESULT result);
    bool bootLabel();
    bool writeVerified(uint32_t sector,uint8_t *data);
    BlockDevice &disk_;
    openosc::Filesystem &fs_;
    State state_=State::Idle;
    FRESULT result_=FR_OK;
    const char *error_=nullptr;
    uint32_t sectors_=0,sectorBytes_=0;
};
} // namespace fat
} // namespace openosc
