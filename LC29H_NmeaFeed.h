#pragma once

#include <stddef.h>
#include <stdint.h>

// Feed complete NMEA lines into a third-party parser without depending on it.
// TinyGPS++ and clones expose bool encode(char). This header does not include them.

namespace LC29H_NmeaFeed {

typedef void (*NmeaByteFn)(uint8_t byte, void* user);

inline size_t feedNmeaBytes(const uint8_t* data, size_t len, NmeaByteFn fn, void* user) {
    if (data == nullptr || fn == nullptr || len == 0) {
        return 0;
    }
    for (size_t i = 0; i < len; ++i) {
        fn(data[i], user);
    }
    return len;
}

inline size_t feedNmeaBytes(const char* line, NmeaByteFn fn, void* user) {
    if (line == nullptr || fn == nullptr) {
        return 0;
    }
    size_t n = 0;
    for (const char* p = line; *p != '\0'; ++p) {
        fn(static_cast<uint8_t>(*p), user);
        ++n;
    }
    fn(static_cast<uint8_t>('\r'), user);
    fn(static_cast<uint8_t>('\n'), user);
    return n + 2;
}

// gps.encode(char) is the TinyGPS++ contract. CR/LF are appended so a sentence
// that arrived without them still closes.
template <typename Gps>
size_t feedTinyGpsPlus(Gps& gps, const char* line) {
    if (line == nullptr) {
        return 0;
    }
    size_t n = 0;
    for (const char* p = line; *p != '\0'; ++p) {
        gps.encode(*p);
        ++n;
    }
    gps.encode('\r');
    gps.encode('\n');
    return n + 2;
}

}  // namespace LC29H_NmeaFeed
