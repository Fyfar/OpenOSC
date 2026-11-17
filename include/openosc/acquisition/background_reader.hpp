#pragma once

namespace openosc::scope {
// A foreground reader that keeps a FrameLease across loop passes (the protocol decoder's job).
// DataAccess revokes it, as it does its readout lease, before anything that needs that memory or a
// new capture: revoke() releases the lease at once and the reader never reads that frame again.
class BackgroundReader {
public:
    virtual void revoke() = 0;
protected:
    ~BackgroundReader() = default;
};
} // namespace openosc::scope
