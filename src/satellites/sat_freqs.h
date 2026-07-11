/*
 * VAIL SUMMIT - Satellite Frequency Reference
 * Curated uplink/downlink info for popular amateur birds, keyed by NORAD id.
 * Static reference data only (frequencies drift rarely; TLEs handle orbits).
 */

#ifndef SAT_FREQS_H
#define SAT_FREQS_H

#include <Arduino.h>

struct SatFreqInfo {
    uint32_t norad;
    const char* uplink;     // MHz text, "" if none/receive-only
    const char* downlink;   // MHz text
    const char* mode;       // FM / Linear / SSTV etc.
    const char* note;       // one-liner (tones, event info)
};

static const SatFreqInfo satFreqTable[] = {
    // ISS - the SSTV workhorse. SSTV + voice repeater + APRS all listed.
    { 25544, "145.990 (PL 67.0)", "437.800 / SSTV 145.800", "FM",
      "ARISS SSTV is event-based - check ariss.org for active events" },
    { 27607, "145.850 (PL 67.0)", "436.795", "FM",
      "SO-50: 74.4 Hz tone for 10s arms the repeater timer" },
    { 43017, "435.250 (PL 67.0)", "145.960", "FM",
      "AO-91 (RadFxSat): daylight passes only - battery limits" },
    { 43678, "437.500 (PL 141.3)", "145.900", "FM",
      "PO-101 (Diwata-2): active on scheduled weekends" },
    { 44909, "145.935-145.995", "435.610-435.670", "Linear (inverting)",
      "RS-44: CW beacon 435.605, very workable high orbit" },
    { 24278, "145.900-146.000", "435.800-435.900", "Linear (inverting)",
      "FO-29: switched on over Japan on schedule" },
    {  7530, "432.125-432.175", "145.975-145.925", "Linear (inverting)",
      "AO-7 Mode B: 50 years old and still going (illuminated passes)" },
    { 39444, "435.150-435.130", "145.950-145.970", "Linear (inverting)",
      "AO-73 (FUNcube-1): transponder on in eclipse" },
    { 43770, "145.965-145.935", "435.640-435.670", "Linear (inverting)",
      "JO-97 (JY1SAT): FUNcube family" },
    { 40967, "145.985 (PL 67.0)", "435.180", "FM",
      "AO-85 (Fox-1A): status uncertain - battery degraded" },
    { 43137, "435.350 (PL 67.0)", "145.920", "FM",
      "AO-92 (Fox-1D): 24h L-band/U-band schedule varies" },
    { 47311, "145.970", "436.400", "FM",
      "TEVEL series: eight identical FM birds, no tone required" },
};
#define SAT_FREQ_TABLE_SIZE (sizeof(satFreqTable) / sizeof(satFreqTable[0]))

// Returns frequency info for a NORAD id, or NULL if not in the table
const SatFreqInfo* lookupSatFreqs(uint32_t norad) {
    for (size_t i = 0; i < SAT_FREQ_TABLE_SIZE; i++) {
        if (satFreqTable[i].norad == norad) return &satFreqTable[i];
    }
    return NULL;
}

#endif // SAT_FREQS_H
