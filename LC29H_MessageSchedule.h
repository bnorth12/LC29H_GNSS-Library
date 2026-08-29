#pragma once

#include <LC29H_GNSS.h>

// Status NMEA schedules used by the examples. RTCM (base) and correction
// ingress (rover) are the mission streams and are not in these tables.
//
// PQTMCFGMSGRATE <Rate> is every N navigation epochs. At a 1 Hz fix, RATE 10
// is 10 s. At a 5 Hz rover fix (200 ms), RATE 50 is 10 s.
//
// Write these in setup, then SAVEPAR. CFGMSGRATE takes effect immediately;
// save makes it survive PAIR023.

namespace LC29H_MessageSchedule {

inline uint8_t rateForPeriodMs(uint32_t periodMs, uint32_t fixIntervalMs) {
    if (fixIntervalMs == 0) {
        fixIntervalMs = 1000;
    }
    uint32_t divisor = periodMs / fixIntervalMs;
    if (divisor < 1) {
        divisor = 1;
    }
    if (divisor > 200) {
        divisor = 200;
    }
    return static_cast<uint8_t>(divisor);
}

// Base station at 1 Hz nav. RTCM MSM7+1005 stay 1 Hz via enableRTCM (PAIR432/434).
// PAIR062 defaults GLL+VTG on with GGA/RMC/GSA/GSV — explicit RATE 0 so they stay off.
// GSV is one family (all talkers); there is no GPGSV-only rate in protocol v1.5.
//
//   Message          Period   RATE   Why
//   RMC              1 s      1      time (status)
//   GGA              1 s      1      position (status)
//   GST             10 s     10      accuracy (status)
//   GSA              8 s      8      DOP; not a factor of 10/20 so it misses GSV/SVIN
//   PQTMEPE         10 s     10      estimated error (status)
//   PQTMSVINSTATUS  10 s     10      survey-in Obs/MeanAcc (status)
//   GSV             20 s     20      satellites; bulky NMEA
//   GLL,VTG,GNS,GRS,ZDA off   0      default-on or unused status
inline bool applyBaseStatusRates(LC29H_GNSS& gnss) {
    bool ok = true;
    ok = gnss.setMessageRate("RMC", 1, 1) && ok;
    ok = gnss.setMessageRate("GGA", 1, 1) && ok;
    ok = gnss.setMessageRate("GST", 1, 10) && ok;
    ok = gnss.setMessageRate("GSA", 1, 8) && ok;
    ok = gnss.setMessageRate("PQTMEPE", 1, 10) && ok;
    ok = gnss.setMessageRate("PQTMSVINSTATUS", 1, 10) && ok;
    ok = gnss.setMessageRate("GSV", 1, 20) && ok;
    ok = gnss.setMessageRate("GLL", 1, 0) && ok;
    ok = gnss.setMessageRate("VTG", 1, 0) && ok;
    ok = gnss.setMessageRate("GNS", 1, 0) && ok;
    ok = gnss.setMessageRate("GRS", 1, 0) && ok;
    ok = gnss.setMessageRate("ZDA", 1, 0) && ok;
    return ok;
}

// Rover: ingest RTCM every loop (not in this table). Publish GGA (position)
// and RMC (time) every epoch for GIS mapping tools. VTG is speed/course.
// GST/GSA/ZDA about 1 Hz. GSV every 10 s. PQTM diagnostics off — GIS uses NMEA.
inline bool applyRoverGisRates(LC29H_GNSS& gnss, uint32_t fixIntervalMs) {
    const uint8_t oneHz = rateForPeriodMs(1000, fixIntervalMs);
    const uint8_t tenSec = rateForPeriodMs(10000, fixIntervalMs);
    bool ok = true;
    ok = gnss.setMessageRate("GGA", 1, 1) && ok;
    ok = gnss.setMessageRate("RMC", 1, 1) && ok;
    ok = gnss.setMessageRate("VTG", 1, 1) && ok;
    ok = gnss.setMessageRate("GST", 1, oneHz) && ok;
    ok = gnss.setMessageRate("GSA", 1, oneHz) && ok;
    ok = gnss.setMessageRate("ZDA", 1, oneHz) && ok;
    ok = gnss.setMessageRate("GSV", 1, tenSec) && ok;
    ok = gnss.setMessageRate("PQTMPVT", 1, 0) && ok;
    ok = gnss.setMessageRate("PQTMVEL", 1, 0) && ok;
    ok = gnss.setMessageRate("PQTMDOP", 1, 0) && ok;
    ok = gnss.setMessageRate("PQTMSTD", 1, 0) && ok;
    return ok;
}

}  // namespace LC29H_MessageSchedule
