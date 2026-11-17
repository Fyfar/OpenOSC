#include "openosc/scpi/engine.hpp"
#include <cstdio>

// DECode: (docs/SCPI.md): read-only state and the decoded result. The result names its record by its
// full key (generation, sequence, timestamp, origin, metadata fingerprint) so a host never takes the
// newest WAV record for the decoded one. DATA? sends the CSV (docs/SCOPE_PROTOCOL_DECODERS_CSV.md) as
// a definite block; SAVE writes it to /openosc/DECODE.
namespace openosc::scpi {
namespace {
void decimal(uint64_t value, char *out, size_t capacity) { // nano printf has no %llu.
    char digits[24]; size_t n = 0;
    do { digits[n++] = char('0' + value % 10); value /= 10; } while (value && n < sizeof digits);
    size_t i = 0;
    while (n && i + 1 < capacity) out[i++] = digits[--n];
    out[i] = 0;
}
} // namespace
bool Engine::decodeCommand(const Command &c) {
    const auto match = [&](const char *pattern) { return Header::matches(c.header, pattern); };
    if (!match("DECode:STATe") && !match("DECode:PROTocol") && !match("DECode:RESult") && !match("DECode:DATA") &&
        !match("DECode:SAVE")) return false;
    DecodeReport r;
    if (!backend_.decodeReport(r)) { error(-113, c.query); return true; }
    if (match("DECode:SAVE")) {
        if (c.query) reply("\"%s\"", backend_.decodeSaved());
        else if (*c.argument) error(-108);
        else if (const int16_t e = backend_.decodeSave()) error(e);
        return true;
    }
    if (!c.query) { error(-113); return true; } // Read-only here: the decoder is switched on its own menu.
    if (match("DECode:STATe")) reply("%u", unsigned(r.enabled));
    else if (match("DECode:PROTocol")) reply("%s", r.enabled ? r.protocol : "OFF");
    else if (match("DECode:RESult")) {
        if (!r.result) { error(-230, true); return true; }
        char time[24]; decimal(r.timestampUs, time, sizeof time);
        reply("%s,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%s,%lu,0x%08lX,0x%08lX,%lu,%lu,%lu", r.status, r.protocol,
              (unsigned long)r.events, (unsigned long)r.lost, (unsigned long)r.dropped, (unsigned long)r.overflows,
              (unsigned long)r.clipped, (unsigned long)r.processed, (unsigned long)r.samples, (unsigned long)r.generation,
              (unsigned long)r.sequence, time, (unsigned long)r.origin, (unsigned long)r.fingerprint, (unsigned long)r.settingsHash,
              (unsigned long)r.fsNumerator, (unsigned long)r.fsDenominator, (unsigned long)r.revision);
    } else { // DATA?
        endDecode();
        int16_t refused = 0;
        decodeTicket_ = backend_.decodeExport(refused);
        if (!decodeTicket_) { error(refused ? refused : -230, true); return true; }
        waitingDecode_ = true; deadline_ = now_ + 3000;
    }
    return true;
}
} // namespace openosc::scpi
