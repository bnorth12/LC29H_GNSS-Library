#pragma once

#include <LC29H_NmeaCompat.h>
#include <LC29H_GNSS.h>

// Identify LC29H variant at init (and again after a module swap).
// DA/EA/BA/BS accept different commands; detection must happen before SAVEPAR.

struct LC29H_ModuleIdentity {
    LC29H_NmeaCompat::ModuleFamily family = LC29H_NmeaCompat::ModuleFamily::Unknown;
    char verno[96] = {};
};

inline void LC29H_noteIdentityLine(LC29H_ModuleIdentity& id, const char* line) {
    const auto family = LC29H_NmeaCompat::familyFromLine(line);
    if (family == LC29H_NmeaCompat::ModuleFamily::Unknown || line == nullptr) {
        return;
    }
    id.family = family;
    strncpy(id.verno, line, sizeof(id.verno) - 1);
    id.verno[sizeof(id.verno) - 1] = '\0';
}

inline void LC29H_printIdentity(Stream* log, const LC29H_ModuleIdentity& id) {
    if (log == nullptr) {
        return;
    }
    log->print("Module family=");
    log->println(LC29H_NmeaCompat::familyName(id.family));
    if (id.verno[0] != '\0') {
        log->println(id.verno);
    }
}

// DA RTK is 1 Hz. Fitness nav (PAIR081,1 / nav mode 1) can stay DGPS with MSM7 on UART.
inline void LC29H_applyFamilyPolicy(LC29H_GNSS& gnss, LC29H_NmeaCompat::ModuleFamily family, bool roverRole) {
    if (family == LC29H_NmeaCompat::ModuleFamily::DA) {
        gnss.setFixRateMs(1000);
        if (roverRole) {
            gnss.setNavMode(0);
        }
    }
    if (family == LC29H_NmeaCompat::ModuleFamily::EA && roverRole) {
        gnss.setNavMode(0);
    }
}

inline bool LC29H_familyUsesPair023(LC29H_NmeaCompat::ModuleFamily family) {
    return family != LC29H_NmeaCompat::ModuleFamily::BS;
}

// Query PQTMVERNO and wait for a line. Uses readLine (AVR / no pump).
inline LC29H_ModuleIdentity LC29H_identifyModule(LC29H_GNSS& gnss, Stream* log, uint32_t waitMs = 800) {
    LC29H_ModuleIdentity id;
    if (log != nullptr) {
        log->println("Identifying module (PQTMVERNO)...");
    }
    gnss.queryVersion();
    const uint32_t start = millis();
    String line;
    while ((millis() - start) < waitMs) {
        if (gnss.readLine(line, 0)) {
            LC29H_noteIdentityLine(id, line.c_str());
            if (id.family != LC29H_NmeaCompat::ModuleFamily::Unknown) {
                break;
            }
        }
        delay(10);
    }
    LC29H_printIdentity(log, id);
    return id;
}
