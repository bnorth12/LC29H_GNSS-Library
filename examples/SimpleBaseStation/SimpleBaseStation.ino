#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>
#include <lc29h_board.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <LC29H_HostPump.h>
#endif

// Simple survey-base bring-up. Not a full NTRIP/Wi-Fi app.
//
// 1) AccLimit 15 m. Matching live SVIN is adopted (no PAIR023, Obs kept).
// 2) Otherwise CFGSVIN + SAVEPAR + PAIR023. PAIR003 sleep is not enough.
// 3) Status NMEA schedule: GGA/RMC 1 s, GST/GSA/EPE 5 s, GSV/SVIN 10 s.
//    RTCM MSM7+1005 stay 1 Hz (mission). NMEA is status.
// 4) Drain mixed NMEA+RTCM every loop via processTo (RTCM dest required).
//
// SAVEPAR / module flash: with LC29H_CFG_SAVE=1 this sketch writes rates/mode
// into the module. That leaves a non-default state — restore the base-station
// message-rate set afterward (see esp32s3-lc29h-base-station#12).
//
// ESP32-S3: Tools → USB CDC On Boot = "CDC" for native USB console (COM12).
// Pins: lc29h_board.h (defaults RX=17 TX=18; override with -D).

#if !defined(ARDUINO_ARCH_ESP32)
#include <SoftwareSerial.h>
#endif

namespace {
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kGnssBaud = LC29H_BOARD_GNSS_BAUD;

#if defined(ARDUINO_ARCH_ESP32)
constexpr int kGnssRxPin = LC29H_BOARD_GNSS_RX_PIN;
constexpr int kGnssTxPin = LC29H_BOARD_GNSS_TX_PIN;
HardwareSerial& gnssPort = Serial1;
#else
constexpr int kGnssRxPin = 4;
constexpr int kGnssTxPin = 3;
SoftwareSerial gnssPort(kGnssRxPin, kGnssTxPin);
#endif

class DiscardStream : public Stream {
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t write(uint8_t) override { return 1; }
    size_t write(const uint8_t* /*buf*/, size_t size) override { return size; }
};

LC29H_GNSS gnss(gnssPort, &Serial);
#if defined(ARDUINO_ARCH_ESP32)
LC29H_UartPump::Pump uartPump;
#endif
DiscardStream discardRtcm;
LC29H_GNSS::BridgeState bridgeState;
LC29H_GNSS::BridgeStats bridgeStats;
LC29H_GNSS::BridgeNmeaFilter nmeaFilter;
bool exampleEnabled = false;

const char* profileStatusName(LC29H_GNSS::ProfileStatus status) {
    switch (status) {
    case LC29H_GNSS::ProfileStatus::Success:
        return "Success";
    case LC29H_GNSS::ProfileStatus::CommandFailed:
        return "CommandFailed";
    case LC29H_GNSS::ProfileStatus::SaveFailed:
        return "SaveFailed";
    case LC29H_GNSS::ProfileStatus::VerifyFailed:
        return "VerifyFailed";
    default:
        return "Unknown";
    }
}
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

#if defined(ARDUINO_ARCH_ESP32)
    // Bring-up needs a blocking read timeout. setTimeout(0) only after success.
    LC29H_beginEsp32GnssUart(gnssPort, kGnssBaud, kGnssRxPin, kGnssTxPin);
    uartPump.setPriorities(LC29H_UartPump::baseStationPriorities());
#else
    gnssPort.begin(kGnssBaud);
#endif

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("SimpleBaseStation example");
    Serial.print("GNSS UART RX=");
    Serial.print(kGnssRxPin);
    Serial.print(" TX=");
    Serial.println(kGnssTxPin);
    Serial.println("RTCM 1 Hz mission. Adopt live SVIN if MinDur matches.");
    Serial.println("NOTE: SAVEPAR may leave the module non-default; restore BS rates after HIL.");

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("HIL_EXAMPLE=SimpleBaseStation STATUS=DISABLED reason=no_lc29hconfig");
        return;
    }

    
#if defined(ARDUINO_ARCH_ESP32)
    // Settle + drain before PQTM identity (live NMEA after prior sketch / BS rates).
    delay(300);
    while (gnssPort.available() > 0) { (void)gnssPort.read(); }
#endif

    LC29H_BringUpResult bringUp;
    if (!LC29H_bringUp(gnss, bringUp, &Serial)) {
        Serial.print("Bring-up failed, status=");
        Serial.println(profileStatusName(bringUp.profile.status));
        Serial.print("HIL_EXAMPLE=SimpleBaseStation STATUS=FAIL bringup=");
        Serial.println(profileStatusName(bringUp.profile.status));
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    gnssPort.setTimeout(0);  // non-blocking drain budget for loop pump
#endif

    exampleEnabled = true;
    gnss.queryVersion();
    gnss.queryReceiverMode();
    gnss.querySurveyIn();
    Serial.println("Base survey ready. Watch $PQTMSVINSTATUS Valid=1 Obs counting.");
    Serial.println("HIL_EXAMPLE=SimpleBaseStation STATUS=READY");
}

void loop() {
#if !defined(ARDUINO_AVR_MEGA2560)
    gnss.processSerialCommands();
#endif
    if (!exampleEnabled) {
        return;
    }

#if defined(ARDUINO_ARCH_ESP32)
    LC29H_HostPump::tick(gnssPort, uartPump);
    // RTCM dest must be non-null so mission frames are drained (DiscardStream OK).
    LC29H_HostPump::processTo(uartPump, &discardRtcm, &Serial);
    static uint32_t lastHealthMs = 0;
    if (millis() - lastHealthMs >= 5000) {
        lastHealthMs = millis();
        const LC29H_UartPump::PumpHealth health = LC29H_HostPump::snapshotHealth(uartPump);
        Serial.print("pump frames=");
        Serial.print(health.frames);
        Serial.print(" mailboxOverwrites=");
        Serial.print(health.drops);
        Serial.print(" usbCdcBackpressureDrops=");
        Serial.print(LC29H_HostPump::txDropCount());
        Serial.print(" high=");
        Serial.println(health.drainHighWater);
    }
#else
    gnss.forwardBridgeAvailable(
        discardRtcm,
        bridgeState,
        LC29H_GNSS::BridgeMode::RtcmAndNmeaAllowlist,
        nmeaFilter,
        false,
        &bridgeStats,
        0,
        &Serial,
        nullptr);
#endif
}