#pragma once
#include "openosc/instrument/mode.hpp"
#include "openosc/measurement/dmm/function.hpp"
#include "openosc/measurement/dmm/ranges.hpp"
#include "openosc/data/status.hpp"

namespace openosc {
// The DMM range restore owed after entering the DMM (boot or MODE): its function is asked for first,
// then the saved Auto or manual range (MeterRangeMemory) is decided once by serviceInstruments.
// Owed from the entry's accepted request until that one decision (the selection confirmed and no DMM
// command running: the restore is sent if the reading shows the saved function, else dropped with the
// preferences kept), the selection failed (Timeout, HardwareError), the measurement is no longer the
// DMM (GEN over the DMM keeps it), or an explicit DMM command (keys, SCPI) superseded it - so it never
// waits without end and an old restore never lands on a later choice.
// pending() is what *OPC? waits for (InstrumentBackend::busy) between the selection and the restore,
// when no DMM command is running yet; ChainMs bounds the whole chain for its deadline.
class MeterRestore {
public:
    // Selection, then the range command (a key command, Auto/Manual, ends within 3 s, below it).
    static constexpr uint32_t ChainMs = dmm::FunctionSelector::DeadlineMs + dmm::RangeSelector::DeadlineMs;
    void arm(bool requested) { owed_ = requested; }
    void cancel() { owed_ = false; }
    bool owed() const { return owed_; }
    // Still owed and still able to happen: the DMM measures and its selection has not failed.
    bool pending(MeasurementMode measurement, DataStatus selection) const {
        return owed_ && measurement == MeasurementMode::Multimeter &&
               (selection == DataStatus::Busy || selection == DataStatus::Ok);
    }
    // serviceInstruments, each pass: drops a restore that can no longer happen. True once, when the
    // decision is due (the selection confirmed, no DMM command running); it is then taken (no longer
    // owed) whatever the reading shows.
    bool due(MeasurementMode measurement, DataStatus selection, bool commandRunning) {
        if (owed_ && !pending(measurement, selection)) owed_ = false;
        if (!owed_ || selection != DataStatus::Ok || commandRunning) return false;
        owed_ = false;
        return true;
    }

private:
    bool owed_ = false;
};
} // namespace openosc
