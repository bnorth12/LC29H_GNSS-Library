#include <LC29H_GNSS.h>
#include <LC29H_ProjectConfig.h>

// Dual USB-C bring-up: native USB is the Arduino console, CH340 Serial0 is a
// raw GNSS UART for QGNSS. One host at a time. Auto-apply is off by default so
// QGNSS owns module config. Loop: host bytes into GNSS, GNSS bytes out to host.
//
// Minimum verified hardware:
// - ESP32-S3 target (compiled with esp32:esp32:esp32s3)
// - Designed for dual USB-C ESP32-S3 dev boards (native USB + CH340 USB-UART)

#if !defined(ARDUINO_ARCH_ESP32)
#error "ESP32UsbUartBridge.ino is intended for ESP32 targets only."
#endif

namespace {
constexpr uint32_t kConsoleBaud = 115200;
constexpr uint32_t kGnssBaud = LC29H_CFG_ESP32_GNSS_BAUD;
constexpr uint32_t kUsbUartBaud = LC29H_CFG_ESP32_USB_UART_BRIDGE_BAUD;
constexpr uint32_t kStatusIntervalMs = 1000;

constexpr int kGnssRxPin = LC29H_CFG_ESP32_GNSS_RX_PIN;
constexpr int kGnssTxPin = LC29H_CFG_ESP32_GNSS_TX_PIN;

HardwareSerial& gnssPort = Serial1;
HardwareSerial& usbUartPort = Serial0;

LC29H_GNSS gnss(gnssPort, &Serial);
LC29H_GNSS::RawIngressStats ingressStats;
uint32_t bytesFromGnss = 0;
uint32_t lastStatusMs = 0;
bool bridgeEnabled = false;
uint32_t startupChecksPassed = 0;
uint32_t startupChecksFailed = 0;

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

void reportCheck(const char* name, bool ok) {
    if (ok) {
        ++startupChecksPassed;
        Serial.print("[check] ");
        Serial.print(name);
        Serial.println(" = OK");
    } else {
        ++startupChecksFailed;
        Serial.print("[check] ");
        Serial.print(name);
        Serial.println(" = FAIL");
    }
}

void runStartupSanityChecks() {
    Serial.println("Running startup module checks...");

    String versionLine;
    const bool versionOk = gnss.queryAndWaitLine("PQTMVERNO", "PQTMVERNO", versionLine, 2000);
    reportCheck("PQTMVERNO response", versionOk);
    if (versionOk) {
        Serial.print("  ");
        Serial.println(versionLine);
    }

    uint8_t mode = 0;
    const bool modeOk = gnss.getReceiverMode(mode, 2000);
    reportCheck("Receiver mode query", modeOk);
    if (modeOk) {
        Serial.print("  mode=");
        Serial.println(mode);
    }

    uint32_t baud = 0;
    const bool baudOk = gnss.getBaudRate(baud, 1, 2000);
    reportCheck("UART1 baud query", baudOk);
    if (baudOk) {
        Serial.print("  uart1_baud=");
        Serial.println(baud);
    }

    Serial.print("Startup checks passed=");
    Serial.print(startupChecksPassed);
    Serial.print(", failed=");
    Serial.println(startupChecksFailed);
}
}

void setup() {
    Serial.begin(kConsoleBaud);
    delay(250);

    LC29H_beginEsp32GnssUart(gnssPort, kGnssBaud, kGnssRxPin, kGnssTxPin);
    usbUartPort.begin(kUsbUartBaud);

    gnss.attachConsole(Serial);
    gnss.setRecoveryPolicy(LC29H_projectRecoveryPolicy());

    Serial.println();
    Serial.println("ESP32UsbUartBridge example");
    Serial.println("Native USB Serial is the console; the CH340-backed USB-C exposes Serial0 for raw GNSS/QGNSS traffic.");
    Serial.println("Connect the GNSS module to Serial1 pins and use the second USB-C port as the host-side UART bridge.");
    Serial.println("Expected workflow: run either QGNSS on CH340 or Arduino Serial console commands, not both at once.");
    Serial.println("One GNSS UART reader. Survey-in on DA needs AccLimit 15, SAVEPAR, then PAIR023.");
    Serial.println("Quick test: open this console on native USB, then open QGNSS on the CH340 COM port at the configured bridge baud.");
    Serial.println("Bridge allowlist defaults: GGA on, GST off, RMC off, PQTM off.");
    Serial.println("Use help bridge and help registry in the library console for runtime guidance.");
    Serial.print("GNSS UART: Serial1 RX=");
    Serial.print(kGnssRxPin);
    Serial.print(", TX=");
    Serial.println(kGnssTxPin);
    Serial.print("USB-UART bridge baud=");
    Serial.println(kUsbUartBaud);
    Serial.println("Board pin legend: U0RXD/U0TXD = GPIO44/GPIO43 (CH340), U1RXD/U1TXD = GPIO18/GPIO17 (GNSS).\n");
#if LC29H_CFG_ESP32_USB_UART_BRIDGE_STRICT_OWNERSHIP
    Serial.println("Strict ownership mode: local console commands are disabled while bridge is active.");
#else
    Serial.println("Strict ownership mode: disabled (local console commands enabled).");
#endif

    if (!LC29H_projectConfigAvailable()) {
        Serial.println("lc29hconfig.h is required for ESP32UsbUartBridge. Example stays disabled.");
        return;
    }

#if LC29H_CFG_ESP32_USB_UART_BRIDGE_APPLY_PROJECT_CONFIG
    LC29H_BringUpResult bringUp;
    if (!LC29H_bringUp(gnss, bringUp, &Serial)) {
        Serial.print("Bring-up failed, status=");
        Serial.println(profileStatusName(bringUp.profile.status));
        return;
    }
#else
    Serial.println("Auto-apply is off so QGNSS can own module config. Enable APPLY_PROJECT_CONFIG to run LC29H_bringUp.");
#endif

#if LC29H_CFG_ESP32_USB_UART_BRIDGE_STARTUP_SANITY_CHECKS
    runStartupSanityChecks();
#endif

    bridgeEnabled = true;
    Serial.println("USB-UART bridge active.");
    Serial.println("Console-only mode is supported: leave QGNSS disconnected and use this serial console for debug output.");
}

void loop() {
#if !LC29H_CFG_ESP32_USB_UART_BRIDGE_STRICT_OWNERSHIP
    gnss.processSerialCommands();
#endif

    if (!bridgeEnabled) {
        return;
    }

    gnss.ingestRawAvailable(usbUartPort, 0, &ingressStats, LC29H_CFG_ROVER_CORRECTION_CHUNK_SIZE);
    bytesFromGnss += static_cast<uint32_t>(gnss.forwardAvailable(usbUartPort, 0));

    const uint32_t now = millis();
    if ((now - lastStatusMs) >= kStatusIntervalMs) {
        Serial.print("ESP32UsbUartBridge: bytesFromHost=");
        Serial.print(ingressStats.bytesRead);
        Serial.print(", bytesToGnss=");
        Serial.print(ingressStats.bytesWritten);
        Serial.print(", shortWrites=");
        Serial.print(ingressStats.shortWrites);
        Serial.print(", bytesFromGnss=");
        Serial.print(bytesFromGnss);
        Serial.print(", startupChecks=");
        Serial.print(startupChecksPassed);
        Serial.print("/");
        Serial.print(startupChecksPassed + startupChecksFailed);
        Serial.print(", uptimeMs=");
        Serial.println(now);
        lastStatusMs = now;
    }
}