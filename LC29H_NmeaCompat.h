#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// Host-side NMEA helpers for phone GIS apps (SW Maps Generic NMEA).
// LC29H(DA) does not fill GGA DiffAge/DiffStation and often has no GST.
// Estimated error is $PQTMEPE; SW Maps reads $GNGST instead.

namespace LC29H_NmeaCompat {

enum class ModuleFamily : uint8_t { Unknown, DA, EA, BA, BS, Other };

inline ModuleFamily familyFromLine(const char* line) {
    if (line == nullptr) {
        return ModuleFamily::Unknown;
    }
    if (strstr(line, "LC29HDA") != nullptr) {
        return ModuleFamily::DA;
    }
    if (strstr(line, "LC29HEA") != nullptr) {
        return ModuleFamily::EA;
    }
    if (strstr(line, "LC29HBA") != nullptr) {
        return ModuleFamily::BA;
    }
    if (strstr(line, "LC29HBS") != nullptr) {
        return ModuleFamily::BS;
    }
    if (strstr(line, "LC29H") != nullptr) {
        return ModuleFamily::Other;
    }
    return ModuleFamily::Unknown;
}

inline const char* familyName(ModuleFamily family) {
    switch (family) {
    case ModuleFamily::DA:
        return "DA";
    case ModuleFamily::EA:
        return "EA";
    case ModuleFamily::BA:
        return "BA";
    case ModuleFamily::BS:
        return "BS";
    case ModuleFamily::Other:
        return "other";
    default:
        return "unknown";
    }
}

inline const char* utcFromGga(const char* gga, char* out, size_t outSz) {
    if (out == nullptr || outSz < 8) {
        return "000000.00";
    }
    out[0] = '0';
    out[1] = '\0';
    if (gga == nullptr) {
        strncpy(out, "000000.00", outSz - 1);
        out[outSz - 1] = '\0';
        return out;
    }
    const char* a = strchr(gga, ',');
    if (a == nullptr) {
        strncpy(out, "000000.00", outSz - 1);
        out[outSz - 1] = '\0';
        return out;
    }
    const char* b = strchr(a + 1, ',');
    if (b == nullptr || b <= a + 1) {
        strncpy(out, "000000.00", outSz - 1);
        out[outSz - 1] = '\0';
        return out;
    }
    size_t n = static_cast<size_t>(b - (a + 1));
    if (n >= outSz) {
        n = outSz - 1;
    }
    memcpy(out, a + 1, n);
    out[n] = '\0';
    return out;
}

inline void nmeaFinish(char* bodyAndOut, size_t cap) {
    uint8_t cs = 0;
    const char* p = bodyAndOut;
    if (*p == '$') {
        ++p;
    }
    for (; *p != '\0' && *p != '*'; ++p) {
        cs ^= static_cast<uint8_t>(*p);
    }
    char tail[8];
    snprintf(tail, sizeof(tail), "*%02X", cs);
    strncat(bodyAndOut, tail, cap - strlen(bodyAndOut) - 1);
}

// Builds $GNGST from $PQTMEPE north/east/down/2D. Returns false if parse fails.
inline bool gstFromPqtmepe(const char* epe, const char* gga, char* out, size_t outSz) {
    if (epe == nullptr || out == nullptr || outSz < 40) {
        return false;
    }
    float north = 0;
    float east = 0;
    float down = 0;
    float e2d = 0;
    float e3d = 0;
    if (sscanf(epe, "$PQTMEPE,%*f,%f,%f,%f,%f,%f", &north, &east, &down, &e2d, &e3d) < 4 &&
        sscanf(epe, "$PQTMEPE,%*d,%f,%f,%f,%f,%f", &north, &east, &down, &e2d, &e3d) < 4) {
        return false;
    }
    char utc[16];
    utcFromGga(gga, utc, sizeof(utc));
    const float smin = (north < east) ? north : east;
    snprintf(
        out,
        outSz,
        "$GNGST,%s,%.3f,%.3f,%.3f,0.0,%.3f,%.3f,%.3f",
        utc,
        e2d,
        e2d,
        smin,
        north,
        east,
        down);
    nmeaFinish(out, outSz);
    return true;
}

}  // namespace LC29H_NmeaCompat
