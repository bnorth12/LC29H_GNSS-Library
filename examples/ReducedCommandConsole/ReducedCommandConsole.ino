#include <Arduino.h>

#if defined(ARDUINO_ARCH_AVR) && !defined(HAVE_HWSERIAL1)
#include <SoftwareSerial.h>
SoftwareSerial gnssPort(4, 3); // RX, TX
#else
#define gnssPort Serial1
#endif

static const uint32_t kConsoleBaud = 115200;
static const uint32_t kGnssBaud = 115200;

static char gCmdBuf[96];
static uint8_t gCmdLen = 0;

static bool startsWith(const char* text, const char* prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return false;
        }
    }
    return true;
}

static uint8_t nmeaChecksum(const char* payload) {
    uint8_t cs = 0;
    while (*payload != '\0') {
        cs ^= static_cast<uint8_t>(*payload++);
    }
    return cs;
}

static void sendPayload(const char* payload) {
    const uint8_t cs = nmeaChecksum(payload);
    gnssPort.print('$');
    gnssPort.print(payload);
    gnssPort.print('*');
    if (cs < 16) {
        gnssPort.print('0');
    }
    gnssPort.print(cs, HEX);
    gnssPort.print("\r\n");
}

static void printHelp() {
    Serial.println(F("ReducedCommandConsole commands:"));
    Serial.println(F("  help"));
    Serial.println(F("  help registry | help bridge | help placeholders"));
    Serial.println(F("  status"));
    Serial.println(F("  rover [rateMs]"));
    Serial.println(F("  base"));
    Serial.println(F("  fixrate <ms>"));
    Serial.println(F("  hot | warm | cold"));
    Serial.println(F("  gnss_start | gnss_stop"));
    Serial.println(F("  send <payload>"));
    Serial.println(F("  library help now includes registry, bridge, placeholders, and family summaries."));
}

static void handleCommand(char* line) {
    while (*line == ' ') {
        ++line;
    }

    if (*line == '\0') {
        return;
    }

    if (strcmp(line, "help") == 0) {
        printHelp();
        return;
    }

    if (strcmp(line, "status") == 0) {
        sendPayload("PQTMVERNO");
        sendPayload("PQTMCFGRCVRMODE,R");
        sendPayload("PQTMCFGFIXRATE,R");
        Serial.println(F("Status queries sent."));
        return;
    }

    if (startsWith(line, "rover")) {
        uint32_t rateMs = 200;
        char* arg = strchr(line, ' ');
        if (arg != nullptr) {
            while (*arg == ' ') {
                ++arg;
            }
            if (*arg != '\0') {
                const uint32_t parsed = static_cast<uint32_t>(atol(arg));
                if (parsed > 0) {
                    rateMs = parsed;
                }
            }
        }

        sendPayload("PQTMCFGRCVRMODE,W,1");
        char payload[40];
        snprintf(payload, sizeof(payload), "PQTMCFGFIXRATE,W,%lu", static_cast<unsigned long>(rateMs));
        sendPayload(payload);
        Serial.println(F("Rover commands sent."));
        return;
    }

    if (strcmp(line, "base") == 0) {
        sendPayload("PQTMCFGRCVRMODE,W,2");
        Serial.println(F("Base mode command sent."));
        return;
    }

    if (startsWith(line, "fixrate ")) {
        const uint32_t rateMs = static_cast<uint32_t>(atol(line + 8));
        if (rateMs == 0) {
            Serial.println(F("Usage: fixrate <ms>"));
            return;
        }
        char payload[40];
        snprintf(payload, sizeof(payload), "PQTMCFGFIXRATE,W,%lu", static_cast<unsigned long>(rateMs));
        sendPayload(payload);
        Serial.println(F("Fix rate command sent."));
        return;
    }

    if (strcmp(line, "hot") == 0) {
        sendPayload("PQTMHOT");
        return;
    }

    if (strcmp(line, "warm") == 0) {
        sendPayload("PQTMWARM");
        return;
    }

    if (strcmp(line, "cold") == 0) {
        sendPayload("PQTMCOLD");
        return;
    }

    if (strcmp(line, "gnss_start") == 0) {
        sendPayload("PQTMGNSSSTART");
        return;
    }

    if (strcmp(line, "gnss_stop") == 0) {
        sendPayload("PQTMGNSSSTOP");
        return;
    }

    if (startsWith(line, "send ")) {
        char* payload = line + 5;
        while (*payload == ' ') {
            ++payload;
        }
        if (*payload == '\0') {
            Serial.println(F("Usage: send <payload>"));
            return;
        }
        sendPayload(payload);
        return;
    }

    Serial.println(F("Unknown command. Type help."));
}

void setup() {
    Serial.begin(kConsoleBaud);
#if defined(ARDUINO_ARCH_AVR) && !defined(HAVE_HWSERIAL1)
    gnssPort.begin(kGnssBaud);
#else
    gnssPort.begin(kGnssBaud);
#endif

    Serial.println(F("ReducedCommandConsole ready."));
    Serial.println(F("Type help for commands."));
}

void loop() {
    while (gnssPort.available() > 0) {
        Serial.write(static_cast<uint8_t>(gnssPort.read()));
    }

    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            gCmdBuf[gCmdLen] = '\0';
            handleCommand(gCmdBuf);
            gCmdLen = 0;
            continue;
        }

        if (gCmdLen + 1 < sizeof(gCmdBuf)) {
            gCmdBuf[gCmdLen++] = c;
        }
    }
}
