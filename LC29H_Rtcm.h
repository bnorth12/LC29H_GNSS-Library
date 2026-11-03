#pragma once

#include <Arduino.h>
#include <LC29H_GNSS.h>

// Optional peer. Absent TinyRTCM3 must still compile this header.
#if !defined(LC29H_HAS_TINYRTCM3)
#  if defined(__has_include)
#    if __has_include(<TinyRTCM3.h>)
#      include <TinyRTCM3.h>
#      define LC29H_HAS_TINYRTCM3 1
#    else
#      define LC29H_HAS_TINYRTCM3 0
#    endif
#  else
#    define LC29H_HAS_TINYRTCM3 0
#  endif
#endif

// RTCM3 frame assembly and CRC-24Q. Use on BLE/UART ingress before writeRaw()
// so a partial notify cannot poison the GNSS UART.
// Assembler and Counters are a one-release shim. Per-type fields are copied
// from tinyrtcm3::StreamStats when that header is present (an owned stats
// object is used until setStreamStats). Without TinyRTCM3 the old 1005/1006
// MSM4/MSM7 increments stay inside this assembler. framesOk / crcFails /
// lastType remain the ingress CRC guard.

namespace LC29H_Rtcm {

// CRC-24Q over preamble + reserved/length + payload (not the 3 CRC bytes).
inline uint32_t crc24q(const uint8_t* data, size_t len) {
    uint32_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint32_t>(data[i]) << 16;
        for (int bit = 0; bit < 8; ++bit) {
            crc <<= 1;
            if ((crc & 0x1000000UL) != 0) {
                crc ^= 0x1864CFBUL;
            }
        }
    }
    return crc & 0xFFFFFFUL;
}

inline uint16_t messageType(const uint8_t* frame, size_t len) {
    if (frame == nullptr || len < 5) {
        return 0;
    }
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(frame[3]) << 4) | (static_cast<uint16_t>(frame[4]) >> 4));
}

struct [[deprecated("one-release shim; use tinyrtcm3::StreamStats")]] Counters {
    uint32_t framesOk = 0;
    uint32_t crcFails = 0;
    // Kept so older sketches still compile. Not updated in 0.3.0.
    // Per-type counts live on tinyrtcm3::StreamStats when LC29H_HAS_TINYRTCM3.
    uint32_t msg1005 = 0;
    uint32_t msg1006 = 0;
    uint32_t msm4 = 0;
    uint32_t msm7 = 0;
    uint16_t lastType = 0;
};

class [[deprecated("one-release shim; use tinyrtcm3::StreamStats")]] Assembler {
public:
    void reset() {
        _len = 0;
        _expected = 0;
        _phase = 0;
    }

#if LC29H_HAS_TINYRTCM3
    void setStreamStats(tinyrtcm3::StreamStats* stats) { _stats = stats; }
    // 0 skips lastSeen / gap / rate inside TinyRTCM3.
    void setNowMs(uint32_t nowMs) { _nowMs = nowMs; }
#endif

    void feed(uint8_t b, LC29H_GNSS& dest, LC29H_GNSS::RawIngressStats* stats, Counters& counters) {
        if (_phase == 0) {
            if (b != 0xD3) {
                return;
            }
            _buf[0] = b;
            _len = 1;
            _phase = 1;
            return;
        }
        if (_len >= sizeof(_buf)) {
            ++counters.crcFails;
            reset();
            return;
        }
        _buf[_len++] = b;
        if (_phase == 1) {
            _phase = 2;
            return;
        }
        if (_phase == 2) {
            const size_t payload =
                ((static_cast<size_t>(_buf[1]) & 0x03U) << 8U) | static_cast<size_t>(_buf[2]);
            _expected = payload + 6U;
            if (_expected < 6U || _expected > sizeof(_buf)) {
                ++counters.crcFails;
                reset();
                return;
            }
            _phase = 3;
            return;
        }
        if (_phase == 3 && _len >= _expected) {
            accept(dest, stats, counters);
        }
    }

private:
    uint8_t _buf[1100];
    size_t _len = 0;
    size_t _expected = 0;
    uint8_t _phase = 0;

    void accept(LC29H_GNSS& dest, LC29H_GNSS::RawIngressStats* stats, Counters& counters) {
        if (_expected < 6 || _expected > sizeof(_buf)) {
            ++counters.crcFails;
            reset();
            return;
        }
        const uint32_t got =
            (static_cast<uint32_t>(_buf[_expected - 3]) << 16) |
            (static_cast<uint32_t>(_buf[_expected - 2]) << 8) |
            static_cast<uint32_t>(_buf[_expected - 1]);
        if (crc24q(_buf, _expected - 3) != got) {
            ++counters.crcFails;
            reset();
            return;
        }
        const size_t written = dest.writeRaw(_buf, _expected);
        if (stats != nullptr) {
            stats->bytesRead += _expected;
            stats->bytesWritten += written;
            if (written < _expected) {
                stats->shortWrites += 1;
            }
        }
        ++counters.framesOk;
        counters.lastType = messageType(_buf, _expected);
        noteType(counters, counters.lastType, _expected);
        reset();
    }

    void noteType(Counters& counters, uint16_t type, size_t frameLen) {
#if LC29H_HAS_TINYRTCM3
        tinyrtcm3::StreamStats* stats = (_stats != nullptr) ? _stats : &_owned;
        tinyrtcm3::statsOnCrcPass(stats, type, frameLen, _nowMs);
        counters.msg1005 = stats->type1005;
        counters.msg1006 = stats->type1006;
        counters.msm4 = stats->typeMsm4;
        counters.msm7 = stats->typeMsm7;
#else
        (void)frameLen;
        if (type == 1005) {
            ++counters.msg1005;
        } else if (type == 1006) {
            ++counters.msg1006;
        } else if (type >= 1071 && type <= 1127 && (type % 10) == 4) {
            ++counters.msm4;
        } else if (type >= 1071 && type <= 1127 && (type % 10) == 7) {
            ++counters.msm7;
        }
#endif
    }

#if LC29H_HAS_TINYRTCM3
    tinyrtcm3::StreamStats* _stats = nullptr;
    tinyrtcm3::StreamStats _owned;
    uint32_t _nowMs = 0;
#endif
};

}  // namespace LC29H_Rtcm
