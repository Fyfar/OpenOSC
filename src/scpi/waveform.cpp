#include "openosc/scpi/engine.hpp"
#include "openosc/data/decimal.hpp"
#include "openosc/acquisition/record_depths.hpp"
#include <cstdio>
#include <cstring>

namespace openosc::scpi {
bool Engine::waveformCommand(const Command &c) {
    const auto match = [&](const char *pattern) { return Header::matches(c.header, pattern); };
    if (match("WAVeform:SOURce")) {
        if (c.query) reply("CHAN%u", waveChannel_ + 1);
        else { unsigned ch = 0; if (!Header::matches(c.argument, "CHANnel#", &ch) || ch < 1 || ch > 2) error(-224); else waveChannel_ = ch - 1; }
    } else if (match("WAVeform:FORMat") || match("WAVeform:MODE")) {
        const auto *value = match("WAVeform:FORMat") ? "BYTE" : "RAW";
        if (c.query) reply("%s", value); else if (!Header::word(c.argument, value)) error(-224);
    } else if (match("WAVeform:STARt") || match("WAVeform:STOP")) {
        auto &value = match("WAVeform:STARt") ? waveStart_ : waveStop_;
        if (c.query) { if (value) reply("%lu", (unsigned long)value); else { const auto *f = frame(); if (f) reply("%lu", (unsigned long)f->metadata().samples); else error(-230, true); } }
        else { double number; if (argument(c, number)) { if (number < 1 || number > scope::RecordDepth::Maximum || number != uint32_t(number)) error(-222); else value = uint32_t(number); } }
    } else if (match("WAVeform:SNAPshot")) {
        if (c.query || *c.argument) error(-108,c.query);
        else { backend_.releaseFrame(); preambleChannels_ = 0; }
    } else if (match("WAVeform:SEQuence") || match("WAVeform:TIMing")) {
        const auto *f = frame();
        if (!c.query) error(-109); else if (!f) error(-230,true);
        else reply("%lu", (unsigned long)(match("WAVeform:TIMing") ? f->metadata().timingVerified : f->metadata().sequence));
    } else if (match("WAVeform:PREamble") || match("WAVeform:DATA")) {
        if (!c.query) { error(-109); return true; }
        const bool preamble = match("WAVeform:PREamble");
        const bool repeated = preamble && (preambleChannels_ & (1U << waveChannel_));
        const auto *f = frame(repeated);
        if (repeated) preambleChannels_ = 0;
        if (!f) {
            // The sole preserved frame is briefly unavailable during its DMA copy.
            // Wait in the foreground dispatcher; USB, capture and local UI continue.
            if (!waitingWaveform_) { waitingWaveform_ = true; pendingPreamble_ = preamble; deadline_ = now_ + 1000; }
            else if (int32_t(now_ - deadline_) >= 0) { waitingWaveform_ = false; error(-230,true); }
            return true;
        }
        waitingWaveform_ = false;
        if (!f->metadata().channels[waveChannel_].enabled) { error(-230, true); return true; }
        const auto &m = f->metadata(); const auto &ch = m.channels[waveChannel_];
        if (!ch.calibrated || !m.samplesPerSecond.valid()) { error(-230, true); return true; }
        if (preamble) {
            preambleChannels_ |= 1U << waveChannel_;
            const double interval = double(m.samplesPerSecond.denominator) / m.samplesPerSecond.numerator;
            char increment[40], origin[40], gain[40], zero[40];
            if (!Decimal::number(interval,increment,sizeof increment) ||
                !Decimal::number(m.triggered ? -interval * m.triggerIndex : 0.,origin,sizeof origin) ||
                !Decimal::number(ch.voltsPerCode * ch.probeNumerator / ch.probeDenominator,gain,sizeof gain) ||
                !Decimal::number(ch.zeroCode,zero,sizeof zero)) error(-230,true);
            else reply("0,0,%lu,1,%s,%s,0,%s,%s,128", (unsigned long)m.samples,increment,origin,gain,zero);
        } else {
            const uint32_t end = waveStop_ ? waveStop_ : m.samples;
            if (waveStart_ > end || end > m.samples) { error(-222, true); return true; }
            binaryStart_ = waveStart_ - 1; binaryCount_ = end - binaryStart_; binaryAt_ = 0;
            char count[16]; std::snprintf(count, sizeof count, "%lu", (unsigned long)binaryCount_);
            reply("#%u%s", unsigned(std::strlen(count)), count);
            binary_ = true;
        }
    } else return false;
    return true;
}
} // namespace openosc::scpi
