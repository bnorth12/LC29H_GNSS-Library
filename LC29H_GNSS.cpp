#include "LC29H_GNSS.h"

namespace {
// Lightweight CSV splitter for command/response parsing.
// We intentionally keep this local and allocation-light for MCU targets.
size_t splitCsv(const String& line, String* fields, size_t maxFields) {
    if (maxFields == 0) {
        return 0;
    }

    size_t count = 0;
    int start = 0;
    while (start <= static_cast<int>(line.length()) && count < maxFields) {
        const int comma = line.indexOf(',', start);
        if (comma < 0) {
            fields[count++] = line.substring(start);
            break;
        }
        fields[count++] = line.substring(start, comma);
        start = comma + 1;
    }

    return count;
}

String normalizeSentence(String line) {
    line.trim();
    if (line.startsWith("$")) {
        line.remove(0, 1);
    }
    const int starPos = line.indexOf('*');
    if (starPos >= 0) {
        line = line.substring(0, starPos);
    }
    return line;
}

String nmeaHead(const String& line) {
    if (line.length() < 2 || (line[0] != '$' && line[0] != '!')) {
        return "";
    }

    const int comma = line.indexOf(',');
    if (comma <= 1) {
        return "";
    }

    return line.substring(1, comma);
}

bool hasValidNmeaChecksum(const String& line) {
    String trimmed = line;
    trimmed.trim();
    if (trimmed.length() < 5 || (trimmed[0] != '$' && trimmed[0] != '!')) {
        return false;
    }

    const int star = trimmed.indexOf('*');
    if (star <= 1 || star + 3 != static_cast<int>(trimmed.length())) {
        return false;
    }

    uint8_t checksum = 0;
    for (int index = 1; index < star; ++index) {
        checksum ^= static_cast<uint8_t>(trimmed.charAt(index));
    }

    auto hexValue = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    };

    const int high = hexValue(trimmed.charAt(star + 1));
    const int low = hexValue(trimmed.charAt(star + 2));
    return high >= 0 && low >= 0 && checksum == static_cast<uint8_t>((high << 4) | low);
}

static bool parseFloatField(const String& s, float& out) {
    if (s.length() == 0) {
        return false;
    }
    const float value = s.toFloat();
    if (value <= 0.0f) {
        return false;
    }
    out = value;
    return true;
}

static void printHelpOverview(Stream& console) {
    console.println("Command groups:");
    console.println("- Identity: help qver uid");
    console.println("- Lifecycle: help restore save reboot hot warm cold gnss_start gnss_stop");
    console.println("- Core configuration: help rover base base_survey base_fixed mode_query msg_on msg_off msg_query baud baud_query fixrate fixrate_query rtcm");
    console.println("- Survey and base workflows: help profile_uas profile_base_survey profile_base_static survey_capture survey_pos survey_apply survey_finalize");
    console.println("- Output and diagnostics: help status survey_status rover_status");
    console.println("- Transport and bridge: help send bridge");
    console.println("- Registry: help registry metadata family");
    console.println("- Utilities: help families groups");
    console.println("Use help <command> for command-specific syntax and purpose.");
}

static bool isCurrentProvisionalCommand(const char* base) {
    return base != nullptr && (
        strcmp(base, "PQTMCFGBASE") == 0 ||
        strcmp(base, "PQTMMEPE") == 0
    );
}

static bool parseFamilyName(const String& input, LC29H_GNSS::CommandFamily& outFamily) {
    String family = input;
    family.trim();
    family.toLowerCase();

    if (family == "identity") {
        outFamily = LC29H_GNSS::CommandFamily::Identity;
        return true;
    }
    if (family == "lifecycle") {
        outFamily = LC29H_GNSS::CommandFamily::Lifecycle;
        return true;
    }
    if (family == "coreconfiguration" || family == "core" || family == "config") {
        outFamily = LC29H_GNSS::CommandFamily::CoreConfiguration;
        return true;
    }
    if (family == "surveyandbase" || family == "survey" || family == "base") {
        outFamily = LC29H_GNSS::CommandFamily::SurveyAndBase;
        return true;
    }
    if (family == "outputanddiagnostics" || family == "output" || family == "diagnostics") {
        outFamily = LC29H_GNSS::CommandFamily::OutputAndDiagnostics;
        return true;
    }
    if (family == "transportbridge" || family == "bridge") {
        outFamily = LC29H_GNSS::CommandFamily::TransportBridge;
        return true;
    }
    if (family == "paircontrol" || family == "pair") {
        outFamily = LC29H_GNSS::CommandFamily::PairControl;
        return true;
    }
    if (family == "consoleandutility" || family == "console" || family == "utility") {
        outFamily = LC29H_GNSS::CommandFamily::ConsoleAndUtility;
        return true;
    }
    if (family == "generic") {
        outFamily = LC29H_GNSS::CommandFamily::Generic;
        return true;
    }
    return false;
}

static bool printDetailedCommandHelp(Stream* console, const String& helpArgs) {
    String topic = helpArgs;
    topic.trim();
    topic.toLowerCase();

    if (topic.length() == 0) {
        return false;
    }

    if (topic == "help") {
        console->println("help [command]");
        console->println("Show command list, or details for one command.");
        console->println("Example: help base_survey");
        return true;
    }

    if (topic == "status") {
        console->println("status");
        console->println("Query version, mode, survey state, and baud on port 1.");
        return true;
    }

    if (topic == "restore") {
        console->println("restore");
        console->println("Restore receiver defaults.");
        return true;
    }

    if (topic == "save") {
        console->println("save");
        console->println("Save current receiver configuration to flash (PQTMSAVEPAR).");
        console->println("On DA/EA, CFGSVIN still needs reboot (PAIR023) after save.");
        return true;
    }

    if (topic == "reboot") {
        console->println("reboot");
        console->println("Full module reboot (PAIR023). Required after SAVEPAR for survey-in.");
        console->println("PAIR003/PAIR002 GNSS sleep is not a reboot.");
        return true;
    }

    if (topic == "rover") {
        console->println("rover [rateMs]");
        console->println("Set rover profile and optional fix rate in milliseconds.");
        console->println("Example: rover 200");
        return true;
    }

    if (topic == "base") {
        console->println("base");
        console->println("Set base-station mode with default behavior.");
        return true;
    }

    if (topic == "base_survey") {
        console->println("base_survey [minTimeSec] [stdDevM]");
        console->println("Enable Survey-In base mode with duration and accuracy limits.");
        console->println("AccLimit is meters; 15 starts <Obs> on DA. Then save and reboot.");
        console->println("Example: base_survey 300 15");
        return true;
    }

    if (topic == "base_fixed") {
        console->println("base_fixed <lat> <lon> <alt>");
        console->println("Enable fixed base coordinates (decimal degrees, meters).");
        console->println("Example: base_fixed 47.6205 -122.3493 52.4");
        return true;
    }

    if (topic == "profile_uas") {
        console->println("profile_uas [fixMs] [save0or1] [verify0or1]");
        console->println("Apply UAS rover profile with optional save/verify.");
        console->println("Example: profile_uas 200 1 1");
        return true;
    }

    if (topic == "profile_base_survey") {
        console->println("profile_base_survey [sec] [std] [rtcm0or1] [save0or1] [verify0or1]");
        console->println("Apply survey base profile with optional RTCM, save, verify.");
        console->println("AccLimit is meters; 15 starts <Obs> on DA. Then reboot (PAIR023).");
        console->println("Example: profile_base_survey 300 15 1 1 1");
        return true;
    }

    if (topic == "profile_base_static") {
        console->println("profile_base_static <lat> <lon> <alt> [rtcm0or1] [save0or1] [verify0or1]");
        console->println("Apply static base profile with fixed coordinates.");
        console->println("Example: profile_base_static 47.6205 -122.3493 52.4 1 1 1");
        return true;
    }

    if (topic == "survey_capture") {
        console->println("survey_capture [timeoutMs]");
        console->println("Capture ECEF from the current Survey-In response.");
        console->println("Example: survey_capture 2000");
        return true;
    }

    if (topic == "survey_pos") {
        console->println("survey_pos");
        console->println("Print captured Survey-In ECEF coordinates.");
        return true;
    }

    if (topic == "survey_status") {
        console->println("survey_status [tailCount]");
        console->println("Print survey accuracy tracker summary and optional recent samples.");
        console->println("Example: survey_status 10");
        return true;
    }

    if (topic == "rover_status") {
        console->println("rover_status [tailCount]");
        console->println("Print rover accuracy tracker summary and optional recent samples.");
        console->println("Example: rover_status 10");
        return true;
    }

    if (topic == "survey_apply") {
        console->println("survey_apply [save0or1]");
        console->println("Apply captured Survey-In ECEF as fixed base.");
        console->println("Example: survey_apply 1");
        return true;
    }

    if (topic == "survey_finalize") {
        console->println("survey_finalize [timeoutMs] [save0or1]");
        console->println("Capture Survey-In ECEF and apply it as fixed base.");
        console->println("Use only after PQTMSVINSTATUS Valid=2, not at survey start.");
        console->println("Example: survey_finalize 2000 1");
        return true;
    }

    if (topic == "mode_query") {
        console->println("mode_query");
        console->println("Query receiver mode.");
        return true;
    }

    if (topic == "msg_on") {
        console->println("msg_on <name> [port]");
        console->println("Enable an output message on a port.");
        console->println("Example: msg_on GGA 1");
        return true;
    }

    if (topic == "msg_off") {
        console->println("msg_off <name> [port]");
        console->println("Disable an output message on a port.");
        console->println("Example: msg_off GGA 1");
        return true;
    }

    if (topic == "msg_query") {
        console->println("msg_query <name> [port]");
        console->println("Query output message rate on a port.");
        console->println("Example: msg_query GGA 1");
        return true;
    }

    if (topic == "baud") {
        console->println("baud <rate> [port]");
        console->println("Set serial baud on a receiver port.");
        console->println("Example: baud 115200 1");
        return true;
    }

    if (topic == "baud_query") {
        console->println("baud_query [port]");
        console->println("Query serial baud on a receiver port.");
        console->println("Example: baud_query 1");
        return true;
    }

    if (topic == "fixrate") {
        console->println("fixrate <ms>");
        console->println("Set navigation fix interval in milliseconds.");
        console->println("Example: fixrate 200");
        return true;
    }

    if (topic == "fixrate_query") {
        console->println("fixrate_query");
        console->println("Query current navigation fix interval.");
        return true;
    }

    if (topic == "hot" || topic == "warm" || topic == "cold") {
        console->println("hot | warm | cold");
        console->println("Perform GNSS restart with selected startup mode.");
        console->println("These are not a full module reboot. Use reboot (PAIR023) after SAVEPAR.");
        return true;
    }

    if (topic == "gnss_start" || topic == "gnss_stop") {
        console->println("gnss_start | gnss_stop");
        console->println("Start or stop GNSS engine.");
        return true;
    }

    if (topic == "uid") {
        console->println("uid");
        console->println("Query receiver unique ID.");
        return true;
    }

    if (topic == "qver") {
        console->println("qver");
        console->println("Query firmware and protocol version info.");
        return true;
    }

    if (topic == "metadata") {
        console->println("metadata <command>");
        console->println("Print family, direction, ACK, and field metadata for a command.");
        console->println("Example: help metadata PQTMCFGSVIN");
        return true;
    }

    if (topic == "registry") {
        console->println("registry");
        console->println("Print the verified, provisional, and TODO placeholder command inventory.");
        console->println("Example: help registry");
        LC29H_GNSS::printCommandFamilySummary(*console);
        return true;
    }

    if (topic == "placeholders" || topic == "todo") {
        console->println("placeholders");
        console->println("Print the reserved V1.5 placeholder slots and their policy.");
        console->println("Example: help placeholders");
        console->println("Use help registry to see the current counts and family breakdown.");
        return true;
    }

    if (topic.startsWith("family ")) {
        const String familyArg = topic.substring(7);
        LC29H_GNSS::CommandFamily family = LC29H_GNSS::CommandFamily::Generic;
        if (parseFamilyName(familyArg, family)) {
            LC29H_GNSS::printCommandFamilyDefaults(*console, family);
            return true;
        }
        console->println("Unknown family. Try help families.");
        return false;
    }

    if (topic == "family" || topic == "defaults") {
        console->println("family <name>");
        console->println("Print default metadata for a command family.");
        console->println("Example: help family coreconfiguration");
        return true;
    }

    if (topic == "families" || topic == "groups") {
        printHelpOverview(*console);
        return true;
    }

    if (topic == "rtcm") {
        console->println("rtcm on|off");
        console->println("Enable or disable RTCM message outputs used by base workflows.");
        console->println("Example: rtcm on");
        return true;
    }

    if (topic == "send") {
        console->println("send <PQTM/PAIR payload>");
        console->println("Send raw payload; library wraps it as a checksummed sentence.");
        console->println("Example: send PQTMGNSSSTART");
        return true;
    }

    if (topic == "bridge") {
        console->println("bridge");
        console->println("Bridge helpers forward RTCM/NMEA/raw bytes while optionally filtering NMEA.");
        console->println("Modes: ForwardAll, RtcmOnly, RtcmAndNmeaAllowlist");
        console->println("Allowlist defaults: GGA on, GST off, RMC off, PQTM off");
        console->println("Examples:");
        console->println("- help bridge");
        console->println("- bridge_status");
        console->println("- bridge_mode RtcmAndNmeaAllowlist");
        return true;
    }

    if (topic == "rover" || topic == "base" || topic == "base_survey" || topic == "base_fixed") {
        console->println("These commands configure the receiver role and base/rover setup.");
        if (topic == "rover") {
            console->println("rover [rateMs]");
            console->println("Set rover profile and optional fix rate in milliseconds.");
            console->println("Example: rover 200");
        } else if (topic == "base") {
            console->println("base");
            console->println("Set base-station mode with default behavior.");
        } else if (topic == "base_survey") {
            console->println("base_survey [minTimeSec] [stdDevM]");
            console->println("Enable Survey-In base mode with duration and accuracy limits.");
            console->println("AccLimit is meters; 15 starts <Obs> on DA. Then save and reboot.");
            console->println("Example: base_survey 300 15");
        } else {
            console->println("base_fixed <lat> <lon> <alt>");
            console->println("Enable fixed base coordinates (decimal degrees, meters).");
            console->println("Example: base_fixed 47.6205 -122.3493 52.4");
        }
        return true;
    }

    if (LC29H_GNSS::findCommandMetadata(topic) != nullptr || LC29H_GNSS::inferCommandFamily(topic) != LC29H_GNSS::CommandFamily::Generic) {
        LC29H_GNSS::printCommandMetadata(*console, topic);
        return true;
    }

    if (LC29H_GNSS::findCommandMetadata(topic) != nullptr || LC29H_GNSS::inferCommandFamily(topic) != LC29H_GNSS::CommandFamily::Generic) {
        LC29H_GNSS::printCommandMetadata(*console, topic);
        return true;
    }

    return false;
}
} // namespace

LC29H_GNSS::LC29H_GNSS(Stream& gnssStream, Stream* debugStream)
    : _gnss(gnssStream),
      _debug(debugStream),
      _console(nullptr),
      _consoleBuffer(),
      _powerCycleRecommended(false),
            _recoveryPolicy(),
            _lastError(ErrorCode::None),
            _lastErrorContext(),
            _eventHandler(nullptr),
            _eventUserData(nullptr),
      _capturedSurveyEcef{0.0, 0.0, 0.0, false} {}

        LC29H_GNSS::~LC29H_GNSS() {
            if (_surveyAccuracy.ring != nullptr) {
                free(_surveyAccuracy.ring);
                _surveyAccuracy.ring = nullptr;
                _surveyAccuracy.capacity = 0;
            }
            if (_roverAccuracy.ring != nullptr) {
                free(_roverAccuracy.ring);
                _roverAccuracy.ring = nullptr;
                _roverAccuracy.capacity = 0;
            }
        }

        void LC29H_GNSS::_applyAccuracyTrackerConfig(AccuracyTrackerState& state, const AccuracyTrackerConfig& config) {
            AccuracyTrackerConfig sanitized = config;
            if (sanitized.windowSec == 0) {
                sanitized.windowSec = 1;
            }
            if (sanitized.maxPoints == 0) {
                sanitized.maxPoints = 1;
            }
            if (sanitized.maxPoints > kAccuracyRingCapacity) {
                sanitized.maxPoints = kAccuracyRingCapacity;
            }

            if (state.ring != nullptr) {
                free(state.ring);
                state.ring = nullptr;
                state.capacity = 0;
            }

            if (sanitized.enabled) {
                const size_t bytes = static_cast<size_t>(sanitized.maxPoints) * sizeof(AccuracySample);
                state.ring = static_cast<AccuracySample*>(malloc(bytes));
                if (state.ring == nullptr) {
                    sanitized.enabled = false;
                    sanitized.maxPoints = 0;
                } else {
                    state.capacity = sanitized.maxPoints;
                }
            }

            state.config = sanitized;
            _resetAccuracyTrackerState(state);
        }

        void LC29H_GNSS::_resetAccuracyTrackerState(AccuracyTrackerState& state) {
            state.head = 0;
            state.count = 0;
            state.total = 0;
            state.startMs = 0;
            state.lastSampleMs = 0;
            state.latest = 0.0f;
            state.best = 0.0f;
            state.worst = 0.0f;
            state.sum = 0.0;
        }

        bool LC29H_GNSS::_appendAccuracySample(AccuracyTrackerState& state, float estimate, uint32_t nowMs) {
            if (!state.config.enabled || state.ring == nullptr || state.capacity == 0 || estimate <= 0.0f) {
                return false;
            }

            if (state.startMs == 0) {
                state.startMs = nowMs;
            }

            const uint32_t windowMs = state.config.windowSec * 1000UL;
            uint32_t intervalMs = windowMs / static_cast<uint32_t>(state.capacity);
            if (intervalMs == 0) {
                intervalMs = 1000;
            }

            if (state.lastSampleMs != 0 && (nowMs - state.lastSampleMs) < intervalMs) {
                return false;
            }

            state.lastSampleMs = nowMs;
            state.latest = estimate;
            if (state.total == 0 || estimate < state.best) {
                state.best = estimate;
            }
            if (state.total == 0 || estimate > state.worst) {
                state.worst = estimate;
            }

            const uint16_t slot = state.head;
            const float replaced = state.ring[slot].estimate;
            const bool replacing = state.count >= state.capacity;
            if (replacing) {
                state.sum -= replaced;
            }

            AccuracySample s;
            s.elapsedSec = (nowMs - state.startMs) / 1000UL;
            s.estimate = estimate;
            state.ring[slot] = s;
            state.sum += estimate;

            state.head = static_cast<uint16_t>((state.head + 1U) % state.capacity);
            if (!replacing) {
                ++state.count;
            }
            ++state.total;
            return true;
        }

        bool LC29H_GNSS::_trackerSummary(const AccuracyTrackerState& state, AccuracyTrackerSummary& out) const {
            out = AccuracyTrackerSummary{};
            out.enabled = state.config.enabled;
            out.windowSec = state.config.windowSec;
            out.storedSamples = state.count;
            out.totalSamples = state.total;
            if (!state.config.enabled || state.ring == nullptr || state.capacity == 0 || state.count == 0) {
                return false;
            }

            const uint32_t windowMs = state.config.windowSec * 1000UL;
            uint32_t intervalSec = (windowMs / static_cast<uint32_t>(state.capacity)) / 1000UL;
            if (intervalSec == 0) {
                intervalSec = 1;
            }
            out.sampleIntervalSec = intervalSec;
            out.latest = state.latest;
            out.best = state.best;
            out.worst = state.worst;
            out.average = static_cast<float>(state.sum / static_cast<double>(state.count));

            const uint16_t oldest = (state.head + state.capacity - state.count) % state.capacity;
            const uint16_t newest = (state.head + state.capacity - 1U) % state.capacity;
            out.earliest = state.ring[oldest].estimate;
            out.elapsedSec = state.ring[newest].elapsedSec;
            out.delta = out.latest - out.earliest;
            return true;
        }

        void LC29H_GNSS::_printAccuracyStatus(const char* label, const AccuracyTrackerState& state, Stream& out, uint16_t tailSamples) const {
            AccuracyTrackerSummary summary;
            if (!_trackerSummary(state, summary)) {
                out.print(label);
                out.println(": tracker disabled or no samples yet.");
                return;
            }

            out.print(label);
            out.print(" status: samples=");
            out.print(summary.storedSamples);
            out.print("/");
            out.print(state.capacity);
            out.print(", total=");
            out.print(summary.totalSamples);
            out.print(", elapsedSec=");
            out.print(summary.elapsedSec);
            out.print(", windowSec=");
            out.print(summary.windowSec);
            out.print(", intervalSec=");
            out.print(summary.sampleIntervalSec);
            out.println();

            out.print("  latest=");
            out.print(summary.latest, 3);
            out.print(", earliest=");
            out.print(summary.earliest, 3);
            out.print(", delta=");
            out.print(summary.delta, 3);
            out.print(", best=");
            out.print(summary.best, 3);
            out.print(", worst=");
            out.print(summary.worst, 3);
            out.print(", avg=");
            out.println(summary.average, 3);

            if (tailSamples == 0) {
                return;
            }

            if (tailSamples > state.count) {
                tailSamples = state.count;
            }

            out.println("  tail samples (elapsedSec,estimate):");
            uint16_t idx = (state.head + state.capacity - tailSamples) % state.capacity;
            for (uint16_t i = 0; i < tailSamples; ++i) {
                const AccuracySample& s = state.ring[idx];
                out.print("    ");
                out.print(s.elapsedSec);
                out.print(",");
                out.println(s.estimate, 3);
                idx = static_cast<uint16_t>((idx + 1U) % state.capacity);
            }
        }

        void LC29H_GNSS::setSurveyAccuracyTrackerConfig(const AccuracyTrackerConfig& config) {
            _applyAccuracyTrackerConfig(_surveyAccuracy, config);
        }

        void LC29H_GNSS::setRoverAccuracyTrackerConfig(const AccuracyTrackerConfig& config) {
            _applyAccuracyTrackerConfig(_roverAccuracy, config);
        }

        void LC29H_GNSS::resetSurveyAccuracyTracker() {
            _resetAccuracyTrackerState(_surveyAccuracy);
        }

        void LC29H_GNSS::resetRoverAccuracyTracker() {
            _resetAccuracyTrackerState(_roverAccuracy);
        }

        bool LC29H_GNSS::getSurveyAccuracySummary(AccuracyTrackerSummary& out) const {
            return _trackerSummary(_surveyAccuracy, out);
        }

        bool LC29H_GNSS::getRoverAccuracySummary(AccuracyTrackerSummary& out) const {
            return _trackerSummary(_roverAccuracy, out);
        }

        void LC29H_GNSS::printSurveyAccuracyStatus(Stream& out, uint16_t tailSamples) const {
            _printAccuracyStatus("Survey accuracy", _surveyAccuracy, out, tailSamples);
        }

        void LC29H_GNSS::printRoverAccuracyStatus(Stream& out, uint16_t tailSamples) const {
            _printAccuracyStatus("Rover accuracy", _roverAccuracy, out, tailSamples);
        }

        void LC29H_GNSS::_observeLineForAccuracy(const String& line) {
            const String head = nmeaHead(line);
            if (head.length() != 5) {
                return;
            }

            const String type = head.substring(2);
            float estimate = 0.0f;

            if (type == "GST") {
                String fields[10];
                const size_t n = splitCsv(normalizeSentence(line), fields, 10);
                if (n >= 3 && parseFloatField(fields[2], estimate)) {
                    const uint32_t nowMs = millis();
                    _appendAccuracySample(_surveyAccuracy, estimate, nowMs);
                    _appendAccuracySample(_roverAccuracy, estimate, nowMs);
                    return;
                }
            }

            if (type == "GGA") {
                String fields[16];
                const size_t n = splitCsv(normalizeSentence(line), fields, 16);
                if (n >= 9 && parseFloatField(fields[8], estimate)) {
                    const uint32_t nowMs = millis();
                    _appendAccuracySample(_surveyAccuracy, estimate, nowMs);
                    _appendAccuracySample(_roverAccuracy, estimate, nowMs);
                }
            }
        }

void LC29H_GNSS::setDebugStream(Stream* debugStream) {
    _debug = debugStream;
}

void LC29H_GNSS::attachConsole(Stream& consoleStream) {
    _console = &consoleStream;
}

void LC29H_GNSS::setEventHandler(EventHandler handler, void* userData) {
    _eventHandler = handler;
    _eventUserData = userData;
}

void LC29H_GNSS::setRecoveryPolicy(const RecoveryPolicy& policy) {
    _recoveryPolicy = policy;
}

LC29H_GNSS::RecoveryPolicy LC29H_GNSS::getRecoveryPolicy() const {
    return _recoveryPolicy;
}

LC29H_GNSS::ErrorCode LC29H_GNSS::getLastError() const {
    return _lastError;
}

String LC29H_GNSS::getLastErrorContext() const {
    return _lastErrorContext;
}

void LC29H_GNSS::clearLastError() {
    _clearError();
}

const char* LC29H_GNSS::errorCodeName(ErrorCode code) {
    switch (code) {
    case ErrorCode::None:
        return "None";
    case ErrorCode::InvalidArgument:
        return "InvalidArgument";
    case ErrorCode::StreamWriteFailed:
        return "StreamWriteFailed";
    case ErrorCode::Timeout:
        return "Timeout";
    case ErrorCode::ParseFailed:
        return "ParseFailed";
    case ErrorCode::CommandFailed:
        return "CommandFailed";
    case ErrorCode::SaveFailed:
        return "SaveFailed";
    case ErrorCode::VerifyFailed:
        return "VerifyFailed";
    case ErrorCode::ShortWrite:
        return "ShortWrite";
    default:
        return "Unknown";
    }
}

void LC29H_GNSS::printLastError(Stream& out) const {
    out.print("LastError=");
    out.print(errorCodeName(_lastError));
    if (_lastErrorContext.length() > 0) {
        out.print(", Context=");
        out.print(_lastErrorContext);
    }
    out.println();
}

bool LC29H_GNSS::queryVersion() {
    return sendPayload("PQTMVERNO");
}

bool LC29H_GNSS::queryQVersion() {
    return sendPayload("PQTMQVER,1");
}

bool LC29H_GNSS::queryUniqueId() {
    return sendPayload("PQTMUNIQID");
}

bool LC29H_GNSS::querySerialNumber() {
    return sendPayload("PQTMSN");
}

bool LC29H_GNSS::restoreDefaults() {
    return sendPayload("PQTMRESTOREPAR");
}

bool LC29H_GNSS::resetToDefaults() {
    return restoreDefaults();
}

bool LC29H_GNSS::saveConfig() {
    // PQTMSAVEPAR writes working config to flash. On DA/EA, CFGSVIN and some other
    // writes still need rebootModule() (PAIR023) before they take effect.
    return sendPayload("PQTMSAVEPAR");
}

bool LC29H_GNSS::rebootModule() {
    // PAIR023 full module reboot. Required on DA/EA after PQTMSAVEPAR for
    // CFGSVIN (survey-in) to start counting <Obs>. PAIR003/PAIR002 GNSS
    // sleep is not a reboot.
    return sendPayload("PAIR023");
}

bool LC29H_GNSS::configureRover(uint16_t outputMs) {
    if (outputMs == 0) {
        _setError(ErrorCode::InvalidArgument, "configureRover: outputMs must be greater than zero");
        return false;
    }

    if (!setReceiverModeRover()) {
        return false;
    }

    return setFixRateMs(outputMs);
}

bool LC29H_GNSS::configureBaseStation() {
    return setReceiverModeBase();
}

bool LC29H_GNSS::configureBaseSurveyIn(uint32_t minTimeSec, float minStdDevM) {
    // Writes PQTMCFGRCVRMODE,W,2 then PQTMCFGSVIN,W,1,<MinDur>,<3D_AccLimit>,0,0,0.
    // <3D_AccLimit> is meters. Quectel default 15.0 starts <Obs> on DA; 0 is not usable.
    // Mode=1 requires ECEF 0,0,0. DA/EA: take effect only after PQTMSAVEPAR and PAIR023
    // (full module reboot). PAIR003/PAIR002 GNSS sleep is not enough. This helper does
    // not save or restart; the caller must.
    String payload = "PQTMCFGSVIN,W,1,";
    payload += String(minTimeSec);
    payload += ",";
    payload += String(minStdDevM, 1);
    payload += ",0,0,0";

    return setReceiverModeBase() && sendPayload(payload);
}

bool LC29H_GNSS::querySurveyIn() {
    return sendPayload("PQTMCFGSVIN,R");
}

bool LC29H_GNSS::getSurveyInConfig(SurveyInConfig& out, uint32_t timeoutMs) {
    out = SurveyInConfig{};
    String line;
    if (!queryAndWaitLine("PQTMCFGSVIN,R", "PQTMCFGSVIN,OK", line, timeoutMs)) {
        return false;
    }

    String fields[16];
    const size_t n = splitCsv(line, fields, 16);
    if (n < 5) {
        _setError(ErrorCode::ParseFailed, "getSurveyInConfig: insufficient fields");
        return false;
    }

    out.mode = static_cast<uint8_t>(fields[2].toInt());
    out.minDur = static_cast<uint32_t>(fields[3].toInt());
    out.accLimitM = fields[4].toFloat();
    if (n >= 8) {
        out.ecef.x = fields[5].toFloat();
        out.ecef.y = fields[6].toFloat();
        out.ecef.z = fields[7].toFloat();
        out.ecef.valid = true;
    }
    return true;
}

bool LC29H_GNSS::tryParseSvinStatus(const String& line, SvinStatus& out) {
    out = SvinStatus{};
    String s = normalizeSentence(line);
    if (!s.startsWith("PQTMSVINSTATUS")) {
        return false;
    }

    String fields[16];
    const size_t n = splitCsv(s, fields, 16);
    if (n < 12) {
        return false;
    }

    out.validCode = static_cast<uint8_t>(fields[3].toInt());
    out.obs = static_cast<uint32_t>(fields[6].toInt());
    out.cfgDur = static_cast<uint32_t>(fields[7].toInt());
    out.meanAccM = fields[11].toFloat();
    out.present = (out.validCode == 1 || out.validCode == 2);
    return true;
}

bool LC29H_GNSS::querySurveyInAndCaptureEcef(uint32_t timeoutMs) {
    // Query first, then parse the incoming response stream for the matching
    // PQTMCFGSVIN,OK payload that includes ECEF coordinates.
    if (!querySurveyIn()) {
        _setError(ErrorCode::CommandFailed, "querySurveyInAndCaptureEcef: query failed");
        return false;
    }

    const uint32_t startMs = millis();
    while ((millis() - startMs) < timeoutMs) {
        String line;
        if (!readLine(line, 50)) {
            continue;
        }

        EcefPosition parsed{0.0, 0.0, 0.0, false};
        if (_tryParseSurveyInConfigLine(line, parsed)) {
            _capturedSurveyEcef = parsed;
            return true;
        }
    }

    _setError(ErrorCode::Timeout, "querySurveyInAndCaptureEcef: timed out waiting for ECEF response");
    return false;
}

bool LC29H_GNSS::hasCapturedSurveyEcef() const {
    return _capturedSurveyEcef.valid;
}

LC29H_GNSS::EcefPosition LC29H_GNSS::getCapturedSurveyEcef() const {
    return _capturedSurveyEcef;
}

bool LC29H_GNSS::setFixedEcef(double x, double y, double z) {
    // Uses survey-in command family mode=2 to set fixed reference ECEF.
    String payload = "PQTMCFGSVIN,W,2,0,0,";
    payload += String(x, 4);
    payload += ",";
    payload += String(y, 4);
    payload += ",";
    payload += String(z, 4);

    const bool ok = setReceiverModeBase() && sendPayload(payload);
    if (ok) {
        _powerCycleRecommended = true;
    }
    return ok;
}

bool LC29H_GNSS::applyCapturedSurveyEcefAsFixed(bool save) {
    if (!_capturedSurveyEcef.valid) {
        _setError(ErrorCode::InvalidArgument, "applyCapturedSurveyEcefAsFixed: no captured ECEF");
        return false;
    }

    if (!setFixedEcef(_capturedSurveyEcef.x, _capturedSurveyEcef.y, _capturedSurveyEcef.z)) {
        _setError(ErrorCode::CommandFailed, "applyCapturedSurveyEcefAsFixed: setFixedEcef failed");
        return false;
    }

    if (save && !saveConfig()) {
        _setError(ErrorCode::SaveFailed, "applyCapturedSurveyEcefAsFixed: save failed");
        return false;
    }

    _powerCycleRecommended = true;
    return true;
}

LC29H_GNSS::PresetResult LC29H_GNSS::finalizeSurveyInToFixedBase(uint32_t timeoutMs, bool save) {
    // This sequence is intentionally explicit so field tooling can choose
    // to inspect captured values before committing if desired.
    if (!querySurveyInAndCaptureEcef(timeoutMs)) {
        _setError(ErrorCode::CommandFailed, "finalizeSurveyInToFixedBase: survey query/capture failed");
        return PresetResult::CommandFailed;
    }

    if (!setFixedEcef(_capturedSurveyEcef.x, _capturedSurveyEcef.y, _capturedSurveyEcef.z)) {
        _setError(ErrorCode::CommandFailed, "finalizeSurveyInToFixedBase: setFixedEcef failed");
        return PresetResult::CommandFailed;
    }

    if (save && !saveConfig()) {
        _setError(ErrorCode::SaveFailed, "finalizeSurveyInToFixedBase: save failed");
        return PresetResult::SaveFailed;
    }

    _powerCycleRecommended = true;
    return PresetResult::Success;
}

bool LC29H_GNSS::configureBaseFixed(double latDeg, double lonDeg, double altM) {
    String latField;
    String lonField;
    char latHemi = 'N';
    char lonHemi = 'E';

    _toNmeaDegrees(latDeg, true, latField, latHemi);
    _toNmeaDegrees(lonDeg, false, lonField, lonHemi);

    const long altitudeRounded = lround(altM);

    String payload = "PQTMCFGBASE,1,";
    payload += latField;
    payload += ",";
    payload += latHemi;
    payload += ",";
    payload += lonField;
    payload += ",";
    payload += lonHemi;
    payload += ",";
    payload += String(altitudeRounded);

    return setReceiverModeBase() && sendPayload(payload);
}

bool LC29H_GNSS::setReceiverModeRover() {
    return sendPayload("PQTMCFGRCVRMODE,W,1");
}

bool LC29H_GNSS::setReceiverModeBase() {
    return sendPayload("PQTMCFGRCVRMODE,W,2");
}

bool LC29H_GNSS::queryReceiverMode() {
    return sendPayload("PQTMCFGRCVRMODE,R");
}

bool LC29H_GNSS::setMessageRate(const String& messageName, uint8_t port, uint8_t rate) {
    // $PQTMCFGMSGRATE,W,<MsgName>,<Rate>[,<MsgVer>]. MsgVer is required for $PQTM names
    // (1, or 2 for PQTMEPE) and must be omitted for standard NMEA on LC29H(DA).
    String payload = "PQTMCFGMSGRATE,W,";
    payload += messageName;
    payload += ",";
    payload += String(rate);
    if (messageName.indexOf("PQTM") >= 0) {
        payload += ",";
        payload += (messageName.endsWith("PQTMEPE") ? "2" : "1");
    }

    (void)port;

    return sendPayload(payload);
}

bool LC29H_GNSS::queryMessageRate(const String& messageName, uint8_t port) {
    String payload = "PQTMCFGMSGRATE,R,";
    payload += messageName;
    payload += ",";
    payload += String(port);
    return sendPayload(payload);
}

bool LC29H_GNSS::enableMessageOutput(const String& messageName, uint8_t port) {
    return setMessageRate(messageName, port, 1);
}

bool LC29H_GNSS::disableMessageOutput(const String& messageName, uint8_t port) {
    return setMessageRate(messageName, port, 0);
}

bool LC29H_GNSS::setConstellations(bool gps, bool glo, bool gal, bool bds, bool qzss, bool navic) {
    String payload = "PQTMCFGCNST,W,";
    payload += gps ? "1" : "0";
    payload += ",";
    payload += glo ? "1" : "0";
    payload += ",";
    payload += gal ? "1" : "0";
    payload += ",";
    payload += bds ? "1" : "0";
    payload += ",";
    payload += qzss ? "1" : "0";
    payload += ",";
    payload += navic ? "1" : "0";
    return sendPayload(payload);
}

bool LC29H_GNSS::queryConstellations() {
    return sendPayload("PQTMCFGCNST,R");
}

bool LC29H_GNSS::setNavMode(uint8_t mode) {
    String payload = "PQTMCFGNAVMODE,W,";
    payload += String(mode);
    return sendPayload(payload);
}

bool LC29H_GNSS::queryNavMode() {
    return sendPayload("PQTMCFGNAVMODE,R");
}

bool LC29H_GNSS::setNmeaPrecision(uint8_t ggaDp, uint8_t gsvDp, uint8_t gsaDp, uint8_t rmcDp, uint8_t vtgDp, uint8_t zdaDp) {
    String payload = "PQTMCFGNMEADP,W,";
    payload += String(ggaDp);
    payload += ",";
    payload += String(gsvDp);
    payload += ",";
    payload += String(gsaDp);
    payload += ",";
    payload += String(rmcDp);
    payload += ",";
    payload += String(vtgDp);
    payload += ",";
    payload += String(zdaDp);
    return sendPayload(payload);
}

bool LC29H_GNSS::queryNmeaPrecision() {
    return sendPayload("PQTMCFGNMEADP,R");
}

bool LC29H_GNSS::setNmeaTalkerId(const String& talker, uint8_t mode) {
    String payload = "PQTMCFGNMEATID,W,";
    payload += talker;
    payload += ",";
    payload += String(mode);
    return sendPayload(payload);
}

bool LC29H_GNSS::queryNmeaTalkerId() {
    return sendPayload("PQTMCFGNMEATID,R");
}

bool LC29H_GNSS::setProtocolMask(uint8_t inPort, uint8_t outPort, uint32_t inMask, uint32_t outMask) {
    String payload = "PQTMCFGPROT,W,";
    payload += String(inPort);
    payload += ",";
    payload += String(outPort);
    payload += ",";
    payload += String(inMask);
    payload += ",";
    payload += String(outMask);
    return sendPayload(payload);
}

bool LC29H_GNSS::queryProtocolMask(uint8_t inPort, uint8_t outPort) {
    String payload = "PQTMCFGPROT,R,";
    payload += String(inPort);
    payload += ",";
    payload += String(outPort);
    return sendPayload(payload);
}

bool LC29H_GNSS::setPulsePerSecondConfig(const String& argsCsv) {
    String payload = "PQTMCFGPPS,W";
    if (argsCsv.length() > 0) {
        payload += ",";
        payload += argsCsv;
    }
    return sendPayload(payload);
}

bool LC29H_GNSS::queryPulsePerSecondConfig() {
    return sendPayload("PQTMCFGPPS,R");
}

bool LC29H_GNSS::setFixRateMs(uint32_t fixRateMs) {
    String payload = "PQTMCFGFIXRATE,W,";
    payload += String(fixRateMs);
    return sendPayload(payload);
}

bool LC29H_GNSS::queryFixRate() {
    return sendPayload("PQTMCFGFIXRATE,R");
}

bool LC29H_GNSS::setBaudRate(uint32_t baudRate, uint8_t uartPort) {
    String payload = "PQTMCFGUART,W,";
    payload += String(uartPort);
    payload += ",";
    payload += String(baudRate);
    return sendPayload(payload);
}

bool LC29H_GNSS::queryBaudRate(uint8_t uartPort) {
    String payload = "PQTMCFGUART,R,";
    payload += String(uartPort);
    return sendPayload(payload);
}

bool LC29H_GNSS::startGnss() {
    return sendPayload("PQTMGNSSSTART");
}

bool LC29H_GNSS::stopGnss() {
    return sendPayload("PQTMGNSSSTOP");
}

bool LC29H_GNSS::hotStart() {
    return sendPayload("PQTMHOT");
}

bool LC29H_GNSS::warmStart() {
    return sendPayload("PQTMWARM");
}

bool LC29H_GNSS::coldStart() {
    return sendPayload("PQTMCOLD");
}

bool LC29H_GNSS::queryDop() {
    return queryMessageRate("PQTMDOP", 1);
}

bool LC29H_GNSS::queryPvt() {
    return queryMessageRate("PQTMPVT", 1);
}

bool LC29H_GNSS::queryVelocity() {
    return queryMessageRate("PQTMVEL", 1);
}

bool LC29H_GNSS::queryStd() {
    return queryMessageRate("PQTMSTD", 1);
}

bool LC29H_GNSS::queryJammingStatus() {
    return queryMessageRate("PQTMJAMMINGSTATUS", 1);
}

bool LC29H_GNSS::queryGeoFenceStatus() {
    return queryMessageRate("PQTMGEOFENCESTATUS", 1);
}

bool LC29H_GNSS::queryOdometer() {
    return queryMessageRate("PQTMODO", 1);
}

bool LC29H_GNSS::queryAndWaitLine(const String& queryPayload, const String& expectedPrefix, String& outLine, uint32_t timeoutMs) {
    outLine = "";

    for (uint8_t attempt = 0; attempt <= _recoveryPolicy.queryRetries; ++attempt) {
        if (!sendPayload(queryPayload)) {
            _setError(ErrorCode::CommandFailed, String("queryAndWaitLine: send failed for ") + queryPayload);
        } else {
            const uint32_t startMs = millis();
            while ((millis() - startMs) < timeoutMs) {
                String line;
                if (!readLine(line, 50)) {
                    continue;
                }

                line = normalizeSentence(line);
                if (line.startsWith(expectedPrefix)) {
                    outLine = line;
                    return true;
                }
            }

            _setError(ErrorCode::Timeout, String("queryAndWaitLine: timeout waiting for ") + expectedPrefix);
        }

        if (attempt < _recoveryPolicy.queryRetries) {
            if (_recoveryPolicy.emitRecoveryEvents) {
                _emitEvent(
                    EventType::Warning,
                    _lastError,
                    String("queryAndWaitLine: retry ") + String(attempt + 1) + " for " + queryPayload);
            }
            if (_recoveryPolicy.retryDelayMs > 0) {
                delay(_recoveryPolicy.retryDelayMs);
            }
        }
    }

    return false;
}

bool LC29H_GNSS::getReceiverMode(uint8_t& outMode, uint32_t timeoutMs) {
    String line;
    if (!queryAndWaitLine("PQTMCFGRCVRMODE,R", "PQTMCFGRCVRMODE,OK", line, timeoutMs)) {
        return false;
    }

    String fields[8];
    const size_t n = splitCsv(line, fields, 8);
    if (n < 3) {
        _setError(ErrorCode::ParseFailed, "getReceiverMode: insufficient fields");
        return false;
    }

    outMode = static_cast<uint8_t>(fields[2].toInt());
    return true;
}

bool LC29H_GNSS::getFixRateMs(uint32_t& outFixRateMs, uint32_t timeoutMs) {
    String line;
    if (!queryAndWaitLine("PQTMCFGFIXRATE,R", "PQTMCFGFIXRATE,OK", line, timeoutMs)) {
        return false;
    }

    String fields[8];
    const size_t n = splitCsv(line, fields, 8);
    if (n < 3) {
        _setError(ErrorCode::ParseFailed, "getFixRateMs: insufficient fields");
        return false;
    }

    outFixRateMs = static_cast<uint32_t>(fields[2].toInt());
    if (outFixRateMs == 0) {
        _setError(ErrorCode::ParseFailed, "getFixRateMs: invalid parsed value");
    }
    return outFixRateMs > 0;
}

bool LC29H_GNSS::getBaudRate(uint32_t& outBaudRate, uint8_t uartPort, uint32_t timeoutMs) {
    String query = "PQTMCFGUART,R,";
    query += String(uartPort);

    String line;
    if (!queryAndWaitLine(query, "PQTMCFGUART,OK", line, timeoutMs)) {
        return false;
    }

    String fields[12];
    const size_t n = splitCsv(line, fields, 12);
    if (n < 3) {
        _setError(ErrorCode::ParseFailed, "getBaudRate: insufficient fields");
        return false;
    }

    if (n >= 4 && fields[2].toInt() == static_cast<int>(uartPort)) {
        outBaudRate = static_cast<uint32_t>(fields[3].toInt());
    } else {
        outBaudRate = static_cast<uint32_t>(fields[2].toInt());
    }

    if (outBaudRate == 0) {
        _setError(ErrorCode::ParseFailed, "getBaudRate: invalid parsed value");
    }
    return outBaudRate > 0;
}

bool LC29H_GNSS::getMessageRate(const String& messageName, uint8_t& outRate, uint8_t port, uint32_t timeoutMs) {
    String query = "PQTMCFGMSGRATE,R,";
    query += messageName;
    query += ",";
    query += String(port);

    String line;
    if (!queryAndWaitLine(query, "PQTMCFGMSGRATE,OK", line, timeoutMs)) {
        return false;
    }

    String fields[12];
    const size_t n = splitCsv(line, fields, 12);
    if (n < 5) {
        _setError(ErrorCode::ParseFailed, "getMessageRate: insufficient fields");
        return false;
    }

    outRate = static_cast<uint8_t>(fields[n - 1].toInt());
    return true;
}

bool LC29H_GNSS::enableRTCM(bool enable) {
    if (enable) {
        // Observed in QGNSS logs.
        const bool ok = sendPayload("PAIR432,1") && sendPayload("PAIR434,1");
        if (!ok) {
            _setError(ErrorCode::CommandFailed, "enableRTCM: failed to enable PAIR432/PAIR434");
        }
        return ok;
    }

    // PAIR432: -1 = disable RTCM, 0 = MSM4, 1 = MSM7. 0 is not "off".
    const bool ok = sendPayload("PAIR432,-1") && sendPayload("PAIR434,0");
    if (!ok) {
        _setError(ErrorCode::CommandFailed, "enableRTCM: failed to disable PAIR432/PAIR434");
    }
    return ok;
}

LC29H_GNSS::PresetResult LC29H_GNSS::applyRoverPreset(uint16_t outputMs, bool save) {
    if (!configureRover(outputMs)) {
        return PresetResult::CommandFailed;
    }

    _powerCycleRecommended = true;

    if (!save) {
        return PresetResult::Success;
    }

    if (!saveConfig()) {
        return PresetResult::SaveFailed;
    }

    _powerCycleRecommended = true;
    return PresetResult::Success;
}

LC29H_GNSS::PresetResult LC29H_GNSS::applyBaseSurveyPreset(uint32_t minTimeSec, float minStdDevM, bool enableRtcm, bool save) {
    if (!configureBaseSurveyIn(minTimeSec, minStdDevM)) {
        return PresetResult::CommandFailed;
    }

    if (enableRtcm && !enableRTCM(true)) {
        return PresetResult::CommandFailed;
    }

    _powerCycleRecommended = true;

    if (!save) {
        return PresetResult::Success;
    }

    if (!saveConfig()) {
        return PresetResult::SaveFailed;
    }

    _powerCycleRecommended = true;
    return PresetResult::Success;
}

LC29H_GNSS::PresetResult LC29H_GNSS::applyBaseFixedPreset(double latDeg, double lonDeg, double altM, bool enableRtcm, bool save) {
    if (!configureBaseFixed(latDeg, lonDeg, altM)) {
        return PresetResult::CommandFailed;
    }

    if (enableRtcm && !enableRTCM(true)) {
        return PresetResult::CommandFailed;
    }

    _powerCycleRecommended = true;

    if (!save) {
        return PresetResult::Success;
    }

    if (!saveConfig()) {
        return PresetResult::SaveFailed;
    }

    _powerCycleRecommended = true;
    return PresetResult::Success;
}

LC29H_GNSS::ProfileResult LC29H_GNSS::applyUasRoverProfile(uint32_t fixRateMs, bool save, bool verify) {
    if (!setReceiverModeRover()) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!setFixRateMs(fixRateMs)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!enableMessageOutput("PQTMPVT", 1)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!enableMessageOutput("PQTMVEL", 1)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!enableMessageOutput("PQTMDOP", 1)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!enableMessageOutput("PQTMSTD", 1)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }

    _powerCycleRecommended = true;

    if (save && !saveConfig()) {
        return {ProfileStatus::SaveFailed, _powerCycleRecommended};
    }

    if (verify) {
        // Verify stage checks that key configuration queries are accepted.
        // It now validates critical returned fields where supported.
        uint8_t mode = 0;
        uint32_t fixMs = 0;
        uint8_t pvtRate = 0;
        uint8_t velRate = 0;
        uint8_t dopRate = 0;
        uint8_t stdRate = 0;
        uint32_t baud = 0;
        const bool ok = getReceiverMode(mode) &&
                        mode == 1 &&
                        getFixRateMs(fixMs) &&
                        fixMs == fixRateMs &&
                        getMessageRate("PQTMPVT", pvtRate, 1) &&
                        pvtRate > 0 &&
                        getMessageRate("PQTMVEL", velRate, 1) &&
                        velRate > 0 &&
                        getMessageRate("PQTMDOP", dopRate, 1) &&
                        dopRate > 0 &&
                        getMessageRate("PQTMSTD", stdRate, 1) &&
                        stdRate > 0 &&
                        getBaudRate(baud, 1);
        if (!ok) {
            return {ProfileStatus::VerifyFailed, _powerCycleRecommended};
        }
    }

    return {ProfileStatus::Success, _powerCycleRecommended};
}

LC29H_GNSS::ProfileResult LC29H_GNSS::applySurveyBaseProfile(uint32_t minTimeSec, float minStdDevM, bool enableRtcm, bool save, bool verify) {
    if (!setReceiverModeBase()) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!configureBaseSurveyIn(minTimeSec, minStdDevM)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    // RATE 1 is a safe enable. Callers should then lower bulky NMEA (GSV and
    // PQTMSVINSTATUS to RATE 10) so 1 Hz RTCM keeps the UART budget.
    if (!enableMessageOutput("PQTMSVINSTATUS", 1)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (enableRtcm && !enableRTCM(true)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }

    _powerCycleRecommended = true;

    if (save && !saveConfig()) {
        return {ProfileStatus::SaveFailed, _powerCycleRecommended};
    }

    if (verify) {
        uint8_t mode = 0;
        uint8_t svinRate = 0;
        uint32_t baud = 0;
        const bool ok = getReceiverMode(mode) &&
                        mode == 2 &&
                        querySurveyIn() &&
                        getMessageRate("PQTMSVINSTATUS", svinRate, 1) &&
                        svinRate > 0 &&
                        getBaudRate(baud, 1);
        if (!ok) {
            return {ProfileStatus::VerifyFailed, _powerCycleRecommended};
        }
    }

    return {ProfileStatus::Success, _powerCycleRecommended};
}

LC29H_GNSS::ProfileResult LC29H_GNSS::applyStaticBaseProfile(double latDeg, double lonDeg, double altM, bool enableRtcm, bool save, bool verify) {
    if (!setReceiverModeBase()) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!configureBaseFixed(latDeg, lonDeg, altM)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (!enableMessageOutput("GGA", 1)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }
    if (enableRtcm && !enableRTCM(true)) {
        return {ProfileStatus::CommandFailed, _powerCycleRecommended};
    }

    _powerCycleRecommended = true;

    if (save && !saveConfig()) {
        return {ProfileStatus::SaveFailed, _powerCycleRecommended};
    }

    if (verify) {
        uint8_t mode = 0;
        uint8_t ggaRate = 0;
        uint32_t baud = 0;
        const bool ok = getReceiverMode(mode) &&
                        mode == 2 &&
                        getMessageRate("GGA", ggaRate, 1) &&
                        ggaRate > 0 &&
                        getBaudRate(baud, 1);
        if (!ok) {
            return {ProfileStatus::VerifyFailed, _powerCycleRecommended};
        }
    }

    return {ProfileStatus::Success, _powerCycleRecommended};
}

bool LC29H_GNSS::isPowerCycleRecommended() const {
    return _powerCycleRecommended;
}

void LC29H_GNSS::clearPowerCycleRecommended() {
    _powerCycleRecommended = false;
}

bool LC29H_GNSS::sendCommand(const String& command, const String& argsCsv) {
    return sendPayload(buildPayload(command, argsCsv));
}

bool LC29H_GNSS::sendCommand(const String& command, const String* args, size_t argCount) {
    return sendPayload(buildPayload(command, args, argCount));
}

bool LC29H_GNSS::sendPayload(const String& payloadWithoutDollarOrChecksum) {
    _clearError();

    if (payloadWithoutDollarOrChecksum.length() == 0) {
        _setError(ErrorCode::InvalidArgument, "sendPayload: empty payload");
        return false;
    }

    // Central path: normalize to a checksummed sentence before TX.
    const String sentence = makeSentence(payloadWithoutDollarOrChecksum);

    for (uint8_t attempt = 0; attempt <= _recoveryPolicy.commandRetries; ++attempt) {
        if (_writeLine(sentence)) {
            return true;
        }

        if (attempt < _recoveryPolicy.commandRetries) {
            if (_recoveryPolicy.emitRecoveryEvents) {
                _emitEvent(
                    EventType::Warning,
                    ErrorCode::StreamWriteFailed,
                    String("sendPayload: retry ") + String(attempt + 1) + " for " + payloadWithoutDollarOrChecksum);
            }
            if (_recoveryPolicy.retryDelayMs > 0) {
                delay(_recoveryPolicy.retryDelayMs);
            }
        }
    }

    return false;
}

bool LC29H_GNSS::sendSentence(const String& sentenceMaybeWithChecksum) {
    String line = sentenceMaybeWithChecksum;
    line.trim();

    if (line.length() == 0) {
        _setError(ErrorCode::InvalidArgument, "sendSentence: empty sentence");
        return false;
    }

    if (!line.startsWith("$")) {
        // Bare payload, e.g. "PQTMCFGRCVRMODE,W,1"
        return sendPayload(line);
    }

    if (line.indexOf('*') < 0) {
        // $-prefixed sentence without checksum.
        return sendPayload(line.substring(1));
    }

    // Full sentence with checksum already provided.
    return _writeLine(line + "\r\n");
}

size_t LC29H_GNSS::writeRaw(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return 0;
    }
    return _gnss.write(data, len);
}

size_t LC29H_GNSS::ingestRawAvailable(Stream& in, size_t maxBytes, RawIngressStats* stats, size_t chunkSize) {
    _clearError();

    if (chunkSize == 0) {
        chunkSize = 256;
    }
    if (chunkSize > 512) {
        chunkSize = 512;
    }

    uint8_t buf[512];
    size_t pending = 0;
    size_t totalWritten = 0;

    while (in.available() > 0) {
        if (maxBytes > 0 && totalWritten >= maxBytes) {
            break;
        }

        const int b = in.read();
        if (b < 0) {
            break;
        }

        buf[pending++] = static_cast<uint8_t>(b);
        if (stats != nullptr) {
            ++stats->bytesRead;
        }

        const bool flushOnChunk = pending >= chunkSize;
        const bool flushOnDrain = (in.available() <= 0);
        const bool flushOnLimit = (maxBytes > 0 && (totalWritten + pending) >= maxBytes);

        if (!(flushOnChunk || flushOnDrain || flushOnLimit)) {
            continue;
        }

        size_t toWrite = pending;
        if (maxBytes > 0) {
            const size_t remaining = maxBytes - totalWritten;
            if (toWrite > remaining) {
                toWrite = remaining;
            }
        }

        if (toWrite == 0) {
            break;
        }

        size_t writtenTotal = writeRaw(buf, toWrite);
        size_t retries = 0;

        while (writtenTotal < toWrite && retries < _recoveryPolicy.rawWriteRetries) {
            ++retries;

            if (_recoveryPolicy.emitRecoveryEvents) {
                _emitEvent(
                    EventType::Warning,
                    ErrorCode::ShortWrite,
                    String("ingestRawAvailable: raw write retry ") + String(retries));
            }

            if (_recoveryPolicy.retryDelayMs > 0) {
                delay(_recoveryPolicy.retryDelayMs);
            }

            const size_t extra = writeRaw(buf + writtenTotal, toWrite - writtenTotal);
            if (extra == 0) {
                break;
            }
            writtenTotal += extra;
        }

        totalWritten += writtenTotal;

        if (stats != nullptr) {
            stats->bytesWritten += static_cast<uint32_t>(writtenTotal);
        }

        if (writtenTotal < toWrite) {
            if (stats != nullptr) {
                ++stats->shortWrites;
            }
            _setError(ErrorCode::ShortWrite, "ingestRawAvailable: short write to GNSS stream");
            break;
        }

        pending = 0;
    }

    return totalWritten;
}

size_t LC29H_GNSS::forwardAvailable(Stream& out, size_t maxBytes) {
    size_t forwarded = 0;

    while (_gnss.available() > 0) {
        if (maxBytes > 0 && forwarded >= maxBytes) {
            break;
        }

        const int b = _gnss.read();
        if (b < 0) {
            break;
        }

        out.write(static_cast<uint8_t>(b));
        ++forwarded;
    }

    return forwarded;
}

bool LC29H_GNSS::forwardNmeaLine(Stream& out, uint32_t timeoutMs) {
    String line;
    if (!readLine(line, timeoutMs)) {
        return false;
    }

    if (!isNmeaSentence(line)) {
        return false;
    }

    out.print(line);
    out.print("\r\n");
    return true;
}

size_t LC29H_GNSS::forwardBridgeAvailable(
    Stream& out,
    BridgeState& state,
    BridgeMode mode,
    const BridgeNmeaFilter& nmeaFilter,
    bool nmeaFilterEnabled,
    BridgeStats* stats,
    size_t maxBytes,
    Stream* localNmeaOut,
    Stream* localRawOut) {
    size_t consumed = 0;
    const uint32_t pumpStartMs = millis();

    while (_gnss.available() > 0) {
        if (maxBytes > 0 && consumed >= maxBytes) {
            break;
        }
        if ((millis() - pumpStartMs) >= kMaxBridgePumpMs) {
            break;
        }

        const int b = _gnss.read();
        if (b < 0) {
            break;
        }

        ++consumed;
        if (stats != nullptr) {
            ++stats->bytesObserved;
        }

        const uint8_t by = static_cast<uint8_t>(b);

        if (localRawOut != nullptr) {
            localRawOut->write(by);
        }

        if (mode == BridgeMode::ForwardAll) {
            out.write(by);
            if (stats != nullptr) {
                ++stats->bytesForwarded;
            }
        }

        switch (state.state) {
        case BridgeParserState::Idle:
            if (by == '$' || by == '!') {
                state.nmeaLine = "";
                state.nmeaLine.reserve(kMaxBridgeNmeaChars);
                state.nmeaLine += static_cast<char>(by);
                state.state = BridgeParserState::Nmea;
                continue;
            }

            if (by == 0xD3) {
                state.rtcmBuf[0] = by;
                state.rtcmIndex = 1;
                state.rtcmExpected = 0;
                state.state = BridgeParserState::RtcmHdr2;
                continue;
            }
            continue;

        case BridgeParserState::Nmea: {
            if (by == '\n') {
                String line = state.nmeaLine;
                if (line.length() > 0) {
                    if (!hasValidNmeaChecksum(line)) {
                        state.nmeaLine = "";
                        state.state = BridgeParserState::Idle;
                        continue;
                    }
                    if (stats != nullptr) {
                        ++stats->nmeaLinesObserved;
                    }

                    const bool shouldForwardNmea =
                        mode == BridgeMode::RtcmAndNmeaAllowlist &&
                        (!nmeaFilterEnabled || isNmeaAllowedByFilter(line, nmeaFilter));

                    if (localNmeaOut != nullptr) {
                        if (mode != BridgeMode::RtcmAndNmeaAllowlist || shouldForwardNmea) {
                            localNmeaOut->println(line);
                        }
                    } else {
                        // Host already parses via localNmeaOut; skip the extra String-heavy
                        // accuracy walk so the two stacks are not nested inside this pump.
                        _observeLineForAccuracy(line);
                    }

                    if (shouldForwardNmea) {
                        out.print(line);
                        out.print("\r\n");
                        if (stats != nullptr) {
                            stats->bytesForwarded += static_cast<uint32_t>(line.length() + 2U);
                            ++stats->nmeaLinesForwarded;
                        }
                    }
                }

                state.nmeaLine = "";
                state.state = BridgeParserState::Idle;
                continue;
            }

            if (by == '\r') {
                continue;
            }

            const bool printable = (by >= 0x20 && by <= 0x7E);
            if (!printable || state.nmeaLine.length() >= kMaxBridgeNmeaChars) {
                if (stats != nullptr) {
                    ++stats->nmeaLineResyncs;
                }
                state.nmeaLine = "";
                state.state = BridgeParserState::Idle;
                if (by == 0xD3) {
                    state.rtcmBuf[0] = by;
                    state.rtcmIndex = 1;
                    state.rtcmExpected = 0;
                    state.state = BridgeParserState::RtcmHdr2;
                } else if (by == '$' || by == '!') {
                    state.nmeaLine.reserve(kMaxBridgeNmeaChars);
                    state.nmeaLine += static_cast<char>(by);
                    state.state = BridgeParserState::Nmea;
                }
                continue;
            }

            state.nmeaLine += static_cast<char>(by);
            continue;
        }

        case BridgeParserState::RtcmHdr2:
            if (state.rtcmIndex >= sizeof(state.rtcmBuf)) {
                state.state = BridgeParserState::Idle;
                state.rtcmIndex = 0;
                state.rtcmExpected = 0;
                continue;
            }
            state.rtcmBuf[state.rtcmIndex++] = by;
            state.state = BridgeParserState::RtcmHdr3;
            continue;

        case BridgeParserState::RtcmHdr3: {
            if (state.rtcmIndex >= sizeof(state.rtcmBuf)) {
                state.state = BridgeParserState::Idle;
                state.rtcmIndex = 0;
                state.rtcmExpected = 0;
                continue;
            }
            state.rtcmBuf[state.rtcmIndex++] = by;

            const size_t payloadLen =
                ((static_cast<size_t>(state.rtcmBuf[1]) & 0x03U) << 8U) |
                static_cast<size_t>(state.rtcmBuf[2]);

            state.rtcmExpected = payloadLen + 6U;
            if (state.rtcmExpected > sizeof(state.rtcmBuf) || state.rtcmExpected < 6U) {
                state.state = BridgeParserState::Idle;
                state.rtcmIndex = 0;
                state.rtcmExpected = 0;
                continue;
            }

            state.state = BridgeParserState::RtcmFrame;
            continue;
        }

        case BridgeParserState::RtcmFrame:
            if (state.rtcmIndex >= sizeof(state.rtcmBuf)) {
                state.state = BridgeParserState::Idle;
                state.rtcmIndex = 0;
                state.rtcmExpected = 0;
                continue;
            }
            state.rtcmBuf[state.rtcmIndex++] = by;
            if (state.rtcmIndex >= state.rtcmExpected) {
                if (mode == BridgeMode::RtcmOnly || mode == BridgeMode::RtcmAndNmeaAllowlist) {
                    out.write(state.rtcmBuf, state.rtcmExpected);
                    if (stats != nullptr) {
                        stats->bytesForwarded += static_cast<uint32_t>(state.rtcmExpected);
                    }
                }

                if (stats != nullptr) {
                    ++stats->rtcmFramesForwarded;
                }

                state.state = BridgeParserState::Idle;
                state.rtcmIndex = 0;
                state.rtcmExpected = 0;
            }
            continue;
        }
    }

    return consumed;
}

bool LC29H_GNSS::readLine(String& outLine, uint32_t timeoutMs) {
    outLine = "";

    const uint32_t start = millis();

    while (true) {
        while (_gnss.available() > 0) {
            char c = static_cast<char>(_gnss.read());
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (outLine.length() > 0 && outLine.length() <= kMaxBridgeNmeaChars) {
                    _observeLineForAccuracy(outLine);
                    return true;
                }
                outLine = "";
                continue;
            }
            if (outLine.length() >= kMaxBridgeNmeaChars) {
                continue;
            }
            outLine += c;
        }

        if (timeoutMs == 0) {
            return false;
        }

        if ((millis() - start) >= timeoutMs) {
            return false;
        }

        delay(1);
    }
}

void LC29H_GNSS::processSerialCommands() {
    if (_console == nullptr) {
        return;
    }

    while (_console->available() > 0) {
        const char c = static_cast<char>(_console->read());

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _consoleBuffer.trim();
            if (_consoleBuffer.length() > 0) {
                _handleConsoleLine(_consoleBuffer);
            }
            _consoleBuffer = "";
            continue;
        }

        _consoleBuffer += c;
    }
}

String LC29H_GNSS::buildPayload(const String& command, const String& argsCsv) {
    String cmd = command;
    cmd.trim();

    if (cmd.startsWith("$")) {
        cmd.remove(0, 1);
    }

    const int starPos = cmd.indexOf('*');
    if (starPos >= 0) {
        cmd = cmd.substring(0, starPos);
    }

    if (argsCsv.length() == 0) {
        return cmd;
    }

    String payload = cmd;
    payload += ",";
    payload += argsCsv;
    return payload;
}

String LC29H_GNSS::buildPayload(const String& command, const String* args, size_t argCount) {
    if (args == nullptr || argCount == 0) {
        return buildPayload(command, "");
    }

    String argsCsv;
    for (size_t i = 0; i < argCount; ++i) {
        if (i > 0) {
            argsCsv += ",";
        }
        argsCsv += args[i];
    }

    return buildPayload(command, argsCsv);
}

String LC29H_GNSS::makeSentence(const String& payloadWithoutDollarOrChecksum) {
    String payload = payloadWithoutDollarOrChecksum;
    if (payload.startsWith("$")) {
        payload.remove(0, 1);
    }

    const int starPos = payload.indexOf('*');
    if (starPos >= 0) {
        payload = payload.substring(0, starPos);
    }

    char checksumHex[3] = {0};
    snprintf(checksumHex, sizeof(checksumHex), "%02X", nmeaChecksum(payload));

    String line = "$";
    line += payload;
    line += "*";
    line += checksumHex;
    line += "\r\n";

    return line;
}

uint8_t LC29H_GNSS::nmeaChecksum(const String& payloadWithoutDollarOrChecksum) {
    uint8_t sum = 0;
    for (size_t i = 0; i < payloadWithoutDollarOrChecksum.length(); ++i) {
        sum ^= static_cast<uint8_t>(payloadWithoutDollarOrChecksum[i]);
    }
    return sum;
}

bool LC29H_GNSS::isNmeaSentence(const String& line) {
    String s = line;
    s.trim();

    if (s.length() == 0) {
        return false;
    }

    return s.startsWith("$");
}

bool LC29H_GNSS::isNmeaAllowedByFilter(const String& line, const BridgeNmeaFilter& filter) {
    const String head = nmeaHead(line);
    if (head.length() == 0) {
        return false;
    }

    if (head.length() == 5) {
        const String type = head.substring(2);
        if (filter.forwardGga && type == "GGA") {
            return true;
        }
        if (filter.forwardGst && type == "GST") {
            return true;
        }
        if (filter.forwardRmc && type == "RMC") {
            return true;
        }
    }

    if (filter.forwardPqtm && head.startsWith("PQTM")) {
        return true;
    }

    return false;
}

const char* LC29H_GNSS::bridgeModeName(BridgeMode mode) {
    switch (mode) {
    case BridgeMode::ForwardAll:
        return "ForwardAll";
    case BridgeMode::RtcmOnly:
        return "RtcmOnly";
    case BridgeMode::RtcmAndNmeaAllowlist:
        return "RtcmAndNmeaAllowlist";
    default:
        return "Unknown";
    }
}

const char* LC29H_GNSS::localDebugOutputModeName(LocalDebugOutputMode mode) {
    switch (mode) {
    case LocalDebugOutputMode::None:
        return "None";
    case LocalDebugOutputMode::NmeaOnly:
        return "NmeaOnly";
    case LocalDebugOutputMode::RawBinary:
        return "RawBinary";
    default:
        return "Unknown";
    }
}

const char* LC29H_GNSS::commandFamilyName(CommandFamily family) {
    switch (family) {
    case CommandFamily::Identity:
        return "Identity";
    case CommandFamily::Lifecycle:
        return "Lifecycle";
    case CommandFamily::CoreConfiguration:
        return "CoreConfiguration";
    case CommandFamily::SurveyAndBase:
        return "SurveyAndBase";
    case CommandFamily::OutputAndDiagnostics:
        return "OutputAndDiagnostics";
    case CommandFamily::TransportBridge:
        return "TransportBridge";
    case CommandFamily::PairControl:
        return "PairControl";
    case CommandFamily::ConsoleAndUtility:
        return "ConsoleAndUtility";
    case CommandFamily::Generic:
        return "Generic";
    default:
        return "Unknown";
    }
}

const char* LC29H_GNSS::commandDirectionName(CommandDirection direction) {
    switch (direction) {
    case CommandDirection::Write:
        return "Write";
    case CommandDirection::Read:
        return "Read";
    case CommandDirection::ReadWrite:
        return "ReadWrite";
    case CommandDirection::Control:
        return "Control";
    default:
        return "Unknown";
    }
}

const char* LC29H_GNSS::commandAckKindName(CommandAckKind ackKind) {
    switch (ackKind) {
    case CommandAckKind::None:
        return "None";
    case CommandAckKind::PairAck:
        return "PairAck";
    case CommandAckKind::CommandOkError:
        return "CommandOkError";
    case CommandAckKind::StatusLine:
        return "StatusLine";
    case CommandAckKind::DirectData:
        return "DirectData";
    default:
        return "Unknown";
    }
}

const char* LC29H_GNSS::commandFieldTypeName(CommandFieldType type) {
    switch (type) {
    case CommandFieldType::Integer:
        return "Integer";
    case CommandFieldType::Unsigned:
        return "Unsigned";
    case CommandFieldType::Float:
        return "Float";
    case CommandFieldType::Text:
        return "Text";
    case CommandFieldType::Boolean:
        return "Boolean";
    case CommandFieldType::Bitmask:
        return "Bitmask";
    case CommandFieldType::Enum:
        return "Enum";
    case CommandFieldType::Degrees:
        return "Degrees";
    case CommandFieldType::Milliseconds:
        return "Milliseconds";
    case CommandFieldType::Seconds:
        return "Seconds";
    case CommandFieldType::Meters:
        return "Meters";
    case CommandFieldType::Port:
        return "Port";
    case CommandFieldType::Rate:
        return "Rate";
    case CommandFieldType::Payload:
        return "Payload";
    default:
        return "Unknown";
    }
}

namespace {
const LC29H_GNSS::CommandFieldSpec kEmptyFieldList[] = {};
const LC29H_GNSS::CommandFieldSpec kIdentityRequestFields[] = {};
const LC29H_GNSS::CommandFieldSpec kConfigReadRequestFields[] = {};
const LC29H_GNSS::CommandFieldSpec kRoverRequestFields[] = {{"rateMs", LC29H_GNSS::CommandFieldType::Milliseconds, true, "ms", "Optional rover fix interval"}};
const LC29H_GNSS::CommandFieldSpec kBaseSurveyRequestFields[] = {{"minTimeSec", LC29H_GNSS::CommandFieldType::Seconds, true, "s", "Survey-in MinDur (fix count at 1 Hz)"}, {"minStdDevM", LC29H_GNSS::CommandFieldType::Meters, true, "m", "3D AccLimit meters; 15 starts Obs on DA"}};
const LC29H_GNSS::CommandFieldSpec kBaseFixedRequestFields[] = {{"latDeg", LC29H_GNSS::CommandFieldType::Degrees, true, "deg", "Latitude in decimal degrees"}, {"lonDeg", LC29H_GNSS::CommandFieldType::Degrees, true, "deg", "Longitude in decimal degrees"}, {"altM", LC29H_GNSS::CommandFieldType::Meters, true, "m", "Altitude in meters"}};
const LC29H_GNSS::CommandFieldSpec kMessageRequestFields[] = {{"messageName", LC29H_GNSS::CommandFieldType::Text, false, nullptr, "NMEA/PQTM message base"}, {"port", LC29H_GNSS::CommandFieldType::Port, true, nullptr, "Output port"}, {"rate", LC29H_GNSS::CommandFieldType::Rate, true, nullptr, "Output rate"}};
const LC29H_GNSS::CommandFieldSpec kBaudRequestFields[] = {{"uartPort", LC29H_GNSS::CommandFieldType::Port, true, nullptr, "UART port index"}, {"baudRate", LC29H_GNSS::CommandFieldType::Unsigned, true, "baud", "UART baud rate"}};
const LC29H_GNSS::CommandFieldSpec kPpsRequestFields[] = {{"argsCsv", LC29H_GNSS::CommandFieldType::Payload, true, nullptr, "Firmware-specific PPS payload"}};
const LC29H_GNSS::CommandFieldSpec kPairWriteFields[] = {{"pairPayload", LC29H_GNSS::CommandFieldType::Payload, false, nullptr, "PAIR command body"}};

const LC29H_GNSS::CommandResponseSpec kPairAckResponse = {"PAIR001", LC29H_GNSS::CommandAckKind::PairAck, "PAIR command acknowledgement"};
const LC29H_GNSS::CommandResponseSpec kStatusLineResponse = {nullptr, LC29H_GNSS::CommandAckKind::StatusLine, "Parsed status line or data response"};

const LC29H_GNSS::CommandFamilyDefaults kFamilyDefaults[] = {
    {LC29H_GNSS::CommandFamily::Identity, "Identity", "Identity and discovery commands", LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, kIdentityRequestFields, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::Lifecycle, "Lifecycle", "Restore/save and receiver lifecycle commands", LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, kEmptyFieldList, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::CoreConfiguration, "CoreConfiguration", "Receiver configuration commands", LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, kConfigReadRequestFields, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::SurveyAndBase, "SurveyAndBase", "Survey-in and fixed-base workflows", LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, kBaseSurveyRequestFields, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::OutputAndDiagnostics, "OutputAndDiagnostics", "Status and diagnostic queries", LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, kEmptyFieldList, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::TransportBridge, "TransportBridge", "Raw byte and bridge helpers", LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::None, kEmptyFieldList, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::PairControl, "PairControl", "PAIR command and ACK helpers", LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::PairAck, kPairWriteFields, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::ConsoleAndUtility, "ConsoleAndUtility", "Console and helper commands", LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::None, kEmptyFieldList, 0, kEmptyFieldList, 0, true},
    {LC29H_GNSS::CommandFamily::Generic, "Generic", "Generic fallback commands", LC29H_GNSS::CommandDirection::Write, LC29H_GNSS::CommandAckKind::None, kEmptyFieldList, 0, kEmptyFieldList, 0, true}
};

const LC29H_GNSS::CommandMetadata kKnownMetadata[] = {
    {"PQTMVERNO", "queryVersion", LC29H_GNSS::CommandFamily::Identity, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Query module firmware/variant", nullptr, nullptr, false, false, true, kIdentityRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMQVER", "queryQVersion", LC29H_GNSS::CommandFamily::Identity, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Query firmware/protocol version details", nullptr, nullptr, false, false, true, kIdentityRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMUNIQID", "queryUniqueId", LC29H_GNSS::CommandFamily::Identity, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Query module unique ID", nullptr, nullptr, false, false, true, kIdentityRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMSN", "querySerialNumber", LC29H_GNSS::CommandFamily::Identity, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Query serial number or similar identity data", nullptr, nullptr, false, false, true, kIdentityRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMRESTOREPAR", "restoreDefaults", LC29H_GNSS::CommandFamily::Lifecycle, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, "Restore receiver defaults", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMSAVEPAR", "saveConfig", LC29H_GNSS::CommandFamily::Lifecycle, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, "Save current configuration to flash", nullptr, nullptr, true, true, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMGNSSSTART", "startGnss", LC29H_GNSS::CommandFamily::Lifecycle, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, "Start GNSS engine", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMGNSSSTOP", "stopGnss", LC29H_GNSS::CommandFamily::Lifecycle, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, "Stop GNSS engine", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMHOT", "hotStart", LC29H_GNSS::CommandFamily::Lifecycle, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, "Hot restart", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMWARM", "warmStart", LC29H_GNSS::CommandFamily::Lifecycle, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, "Warm restart", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCOLD", "coldStart", LC29H_GNSS::CommandFamily::Lifecycle, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::CommandOkError, "Cold restart", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGRCVRMODE", "set/getReceiverMode", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Receiver mode control", nullptr, nullptr, false, false, true, kConfigReadRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGMSGRATE", "set/queryMessageRate", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Message rate control", nullptr, nullptr, false, false, true, kMessageRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGFIXRATE", "set/queryFixRateMs", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Navigation fix interval control", nullptr, nullptr, false, false, true, kRoverRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGUART", "set/queryBaudRate", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "UART configuration", nullptr, nullptr, false, false, true, kBaudRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGPROT", "set/queryProtocolMask", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Protocol mask control", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGCNST", "set/queryConstellations", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Constellation selection", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGNAVMODE", "set/queryNavMode", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Navigation mode control", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGNMEADP", "set/queryNmeaPrecision", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "NMEA decimal precision", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGNMEATID", "set/queryNmeaTalkerId", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "NMEA talker ID control", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGPPS", "set/queryPulsePerSecondConfig", LC29H_GNSS::CommandFamily::CoreConfiguration, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Pulse-per-second configuration", nullptr, nullptr, false, false, true, kPpsRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGSVIN", "configureBaseSurveyIn/querySurveyIn", LC29H_GNSS::CommandFamily::SurveyAndBase, LC29H_GNSS::CommandDirection::ReadWrite, LC29H_GNSS::CommandAckKind::StatusLine, "Survey-In and fixed-base workflow", nullptr, nullptr, true, true, true, kBaseSurveyRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMCFGBASE", "configureBaseFixed", LC29H_GNSS::CommandFamily::SurveyAndBase, LC29H_GNSS::CommandDirection::Write, LC29H_GNSS::CommandAckKind::StatusLine, "Fixed-base latitude/longitude/altitude setup", nullptr, nullptr, true, true, true, kBaseFixedRequestFields, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMPVT", "queryPvt", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "PVT output query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMVEL", "queryVelocity", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Velocity output query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMSTD", "queryStd", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Standard deviation output query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMDOP", "queryDop", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "DOP output query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMJAMMINGSTATUS", "queryJammingStatus", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Jamming/interference output query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMGEOFENCESTATUS", "queryGeoFenceStatus", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Geofence output query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMODO", "queryOdometer", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Odometer output query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMSVINSTATUS", "viaMessageRateHelpers", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Survey-In status output", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PQTMMEPE", "noDedicatedWrapper", LC29H_GNSS::CommandFamily::OutputAndDiagnostics, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::DirectData, "Estimated error query", nullptr, nullptr, false, false, true, kEmptyFieldList, 0, &kStatusLineResponse, kEmptyFieldList, 0},
    {"PAIR004", "genericSendHelper", LC29H_GNSS::CommandFamily::PairControl, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::PairAck, "PAIR family member observed in ModeInfo.json", nullptr, nullptr, false, false, true, kPairWriteFields, 0, &kPairAckResponse, kEmptyFieldList, 0},
    {"PAIR005", "genericSendHelper", LC29H_GNSS::CommandFamily::PairControl, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::PairAck, "PAIR family member observed in ModeInfo.json", nullptr, nullptr, false, false, true, kPairWriteFields, 0, &kPairAckResponse, kEmptyFieldList, 0},
    {"PAIR006", "genericSendHelper", LC29H_GNSS::CommandFamily::PairControl, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::PairAck, "PAIR family member observed in ModeInfo.json", nullptr, nullptr, false, false, true, kPairWriteFields, 0, &kPairAckResponse, kEmptyFieldList, 0},
    {"PAIR007", "genericSendHelper", LC29H_GNSS::CommandFamily::PairControl, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::PairAck, "PAIR family member observed in ModeInfo.json", nullptr, nullptr, false, false, true, kPairWriteFields, 0, &kPairAckResponse, kEmptyFieldList, 0},
    {"PAIR023", "rebootModule", LC29H_GNSS::CommandFamily::PairControl, LC29H_GNSS::CommandDirection::Control, LC29H_GNSS::CommandAckKind::PairAck, "Full module reboot. Required on DA/EA after SAVEPAR for CFGSVIN to start Obs. PAIR003/PAIR002 sleep is not a reboot.", nullptr, nullptr, false, false, true, kPairWriteFields, 0, &kPairAckResponse, kEmptyFieldList, 0},
    {"PAIR001", "readPairAck/tryParsePairAck", LC29H_GNSS::CommandFamily::PairControl, LC29H_GNSS::CommandDirection::Read, LC29H_GNSS::CommandAckKind::PairAck, "PAIR acknowledgement capture", nullptr, nullptr, false, false, true, kPairWriteFields, 0, &kPairAckResponse, kEmptyFieldList, 0}
};

const LC29H_GNSS::CommandMetadata* findKnownMetadata(const String& commandOrPayload) {
    String normalized = normalizeSentence(commandOrPayload);
    normalized.trim();
    normalized.toUpperCase();
    const int comma = normalized.indexOf(',');
    const String base = (comma < 0) ? normalized : normalized.substring(0, comma);

    for (const auto& entry : kKnownMetadata) {
        if (base == entry.base) {
            return &entry;
        }
    }

    return nullptr;
}

const LC29H_GNSS::CommandFamilyDefaults* findFamilyDefaults(LC29H_GNSS::CommandFamily family) {
    for (const auto& defaults : kFamilyDefaults) {
        if (defaults.family == family) {
            return &defaults;
        }
    }
    return nullptr;
}
} // namespace

const LC29H_GNSS::CommandFamilyDefaults* LC29H_GNSS::getCommandFamilyDefaults(CommandFamily family) {
    return findFamilyDefaults(family);
}

LC29H_GNSS::CommandFamily LC29H_GNSS::inferCommandFamily(const String& commandOrPayload) {
    if (const CommandMetadata* metadata = findKnownMetadata(commandOrPayload)) {
        return metadata->family;
    }

    String normalized = normalizeSentence(commandOrPayload);
    normalized.trim();
    normalized.toUpperCase();
    if (normalized.length() == 0) {
        return CommandFamily::Generic;
    }

    const int comma = normalized.indexOf(',');
    const String base = (comma < 0) ? normalized : normalized.substring(0, comma);

    if (base.startsWith("PAIR")) {
        return CommandFamily::PairControl;
    }
    if (base.startsWith("PQTMCFG")) {
        return CommandFamily::CoreConfiguration;
    }
    if (base.startsWith("PQTM")) {
        return CommandFamily::OutputAndDiagnostics;
    }
    return CommandFamily::Generic;
}

LC29H_GNSS::CommandMetadata LC29H_GNSS::inferCommandMetadata(const String& commandOrPayload) {
    if (const CommandMetadata* metadata = findKnownMetadata(commandOrPayload)) {
        return *metadata;
    }

    CommandMetadata metadata;
    metadata.family = inferCommandFamily(commandOrPayload);
    if (const CommandFamilyDefaults* defaults = getCommandFamilyDefaults(metadata.family)) {
        metadata.direction = defaults->defaultDirection;
        metadata.ackKind = defaults->defaultAckKind;
        metadata.requestFields = defaults->defaultRequestFields;
        metadata.requestFieldCount = defaults->defaultRequestFieldCount;
        metadata.responseFields = defaults->defaultResponseFields;
        metadata.responseFieldCount = defaults->defaultResponseFieldCount;
        metadata.genericFallback = defaults->genericFallback;
        metadata.summary = defaults->summary;
    } else {
        metadata.direction = (metadata.family == CommandFamily::PairControl) ? CommandDirection::Control : CommandDirection::Write;
        metadata.ackKind = (metadata.family == CommandFamily::PairControl) ? CommandAckKind::PairAck : CommandAckKind::None;
        metadata.summary = "Generic family fallback; use typed wrapper when available";
        metadata.genericFallback = true;
    }
    return metadata;
}

const LC29H_GNSS::CommandMetadata* LC29H_GNSS::findCommandMetadata(const String& commandOrPayload) {
    return findKnownMetadata(commandOrPayload);
}

static void printFieldList(Stream& out, const char* label, const LC29H_GNSS::CommandFieldSpec* fields, size_t fieldCount) {
    out.print(label);
    out.println(":");
    if (fields == nullptr || fieldCount == 0) {
        out.println("  (none)");
        return;
    }

    for (size_t i = 0; i < fieldCount; ++i) {
        out.print("  - ");
        out.print(fields[i].name ? fields[i].name : "field");
        out.print(" (");
        out.print(LC29H_GNSS::commandFieldTypeName(fields[i].type));
        out.print(")");
        if (fields[i].units != nullptr) {
            out.print(" [");
            out.print(fields[i].units);
            out.print("]");
        }
        if (fields[i].optional) {
            out.print(" optional");
        }
        if (fields[i].notes != nullptr) {
            out.print(" - ");
            out.print(fields[i].notes);
        }
        out.println();
    }
}

void LC29H_GNSS::printCommandMetadata(Stream& out, const String& commandOrPayload) {
    const CommandMetadata metadata = inferCommandMetadata(commandOrPayload);
    String normalized = normalizeSentence(commandOrPayload);
    normalized.trim();
    normalized.toUpperCase();
    const int comma = normalized.indexOf(',');
    const String base = (comma < 0) ? normalized : normalized.substring(0, comma);

    out.print("Command: ");
    out.println(base);
    out.print("Family: ");
    out.println(commandFamilyName(metadata.family));
    out.print("Status: ");
    out.println(isCurrentProvisionalCommand(base.c_str()) ? "provisional" : "verified");
    out.print("Direction: ");
    out.println(commandDirectionName(metadata.direction));
    out.print("ACK: ");
    out.println(commandAckKindName(metadata.ackKind));
    out.print("Wrapper: ");
    out.println(metadata.wrapper != nullptr ? metadata.wrapper : "generic");
    out.print("Summary: ");
    out.println(metadata.summary != nullptr ? metadata.summary : "(none)");
    out.print("Save recommended: ");
    out.println(metadata.saveRecommended ? "yes" : "no");
    out.print("Power-cycle recommended: ");
    out.println(metadata.powerCycleRecommended ? "yes" : "no");
    out.print("Generic fallback: ");
    out.println(metadata.genericFallback ? "yes" : "no");
    printFieldList(out, "Request fields", metadata.requestFields, metadata.requestFieldCount);
    printFieldList(out, "Response fields", metadata.responseFields, metadata.responseFieldCount);
    if (metadata.response != nullptr) {
        out.print("Response prefix: ");
        out.println(metadata.response->prefix != nullptr ? metadata.response->prefix : "(none)");
        out.print("Response notes: ");
        out.println(metadata.response->notes != nullptr ? metadata.response->notes : "(none)");
    }
}

void LC29H_GNSS::printCommandFamilySummary(Stream& out) {
    size_t verifiedCount = 0;
    size_t provisionalCount = 0;

    for (const auto& entry : kKnownMetadata) {
        if (isCurrentProvisionalCommand(entry.base)) {
            ++provisionalCount;
        } else {
            ++verifiedCount;
        }
    }

    out.println("LC29H command registry summary");
    out.print("Verified command bases: ");
    out.println(verifiedCount);
    out.print("Provisional command bases: ");
    out.println(provisionalCount);
    out.println("TODO placeholders: V1.5-001 through V1.5-068");

    for (const auto& defaults : kFamilyDefaults) {
        size_t familyVerified = 0;
        size_t familyProvisional = 0;

        for (const auto& entry : kKnownMetadata) {
            if (entry.family != defaults.family) {
                continue;
            }
            if (isCurrentProvisionalCommand(entry.base)) {
                ++familyProvisional;
            } else {
                ++familyVerified;
            }
        }

        out.println();
        out.print(defaults.familyName != nullptr ? defaults.familyName : commandFamilyName(defaults.family));
        out.print(": ");
        out.print(familyVerified);
        out.print(" verified, ");
        out.print(familyProvisional);
        out.println(" provisional");

        for (const auto& entry : kKnownMetadata) {
            if (entry.family != defaults.family) {
                continue;
            }

            out.print("  - ");
            out.print(entry.base != nullptr ? entry.base : "(unnamed)");
            out.print(" [");
            out.print(isCurrentProvisionalCommand(entry.base) ? "provisional" : "verified");
            out.print("] ");
            out.print(entry.wrapper != nullptr ? entry.wrapper : "generic");
            out.print(" - ");
            out.println(entry.summary != nullptr ? entry.summary : "(none)");
        }
    }

    out.println();
    out.println("Placeholder policy:");
    out.println("- Reserve the missing V1.5 slots until the canonical Temp extraction or live validation exists.");
    out.println("- Do not invent new command identities for the TODO slots.");
}

void LC29H_GNSS::printCommandFamilyDefaults(Stream& out, CommandFamily family) {
    const CommandFamilyDefaults* defaults = getCommandFamilyDefaults(family);
    if (defaults == nullptr) {
        out.println("No family defaults available.");
        return;
    }

    out.print("Family: ");
    out.println(defaults->familyName != nullptr ? defaults->familyName : commandFamilyName(family));
    out.print("Summary: ");
    out.println(defaults->summary != nullptr ? defaults->summary : "(none)");
    out.print("Default direction: ");
    out.println(commandDirectionName(defaults->defaultDirection));
    out.print("Default ACK: ");
    out.println(commandAckKindName(defaults->defaultAckKind));
    out.print("Generic fallback: ");
    out.println(defaults->genericFallback ? "yes" : "no");
    printFieldList(out, "Default request fields", defaults->defaultRequestFields, defaults->defaultRequestFieldCount);
    printFieldList(out, "Default response fields", defaults->defaultResponseFields, defaults->defaultResponseFieldCount);
}

bool LC29H_GNSS::tryParsePairAck(const String& line, PairAck& outAck) {
    String s = normalizeSentence(line);
    if (!s.startsWith("PAIR001,")) {
        return false;
    }

    String fields[6];
    const size_t n = splitCsv(s, fields, 6);
    if (n < 3) {
        return false;
    }

    const long commandId = fields[1].toInt();
    const long result = fields[2].toInt();
    if (commandId < 0L || commandId > 65535L || result < 0L || result > 255L) {
        return false;
    }

    outAck.commandId = static_cast<uint16_t>(commandId);
    outAck.result = static_cast<uint8_t>(result);
    return true;
}

const char* LC29H_GNSS::pairAckResultName(uint8_t result) {
    switch (result) {
    case 0:
        return "Success";
    case 1:
        return "InProgress";
    case 2:
        return "SendFailed";
    case 3:
        return "CommandNotSupported";
    case 4:
        return "ParameterError";
    case 5:
        return "ServiceBusy";
    default:
        return "Unknown";
    }
}

bool LC29H_GNSS::readPairAck(PairAck& outAck, uint32_t timeoutMs) {
    const uint32_t startMs = millis();
    while ((millis() - startMs) < timeoutMs) {
        String line;
        if (!readLine(line, 50)) {
            continue;
        }
        if (tryParsePairAck(line, outAck)) {
            return true;
        }
    }

    _setError(ErrorCode::Timeout, "readPairAck: timed out waiting for PAIR001");
    return false;
}

void LC29H_GNSS::printPairAck(Stream& out, const PairAck& ack, const char* label) {
    if (label != nullptr && label[0] != '\0') {
        out.print(label);
        out.print(": ");
    }

    out.print("PAIR001 cmd=");
    out.print(ack.commandId);
    out.print(", result=");
    out.print(ack.result);
    out.print(" (");
    out.print(pairAckResultName(ack.result));
    out.println(")");
}

void LC29H_GNSS::printBridgeStatus(
    Stream& out,
    const char* label,
    BridgeMode mode,
    const BridgeStats& stats,
    uint32_t uptimeMs) {
    if (label != nullptr && label[0] != '\0') {
        out.print(label);
        out.print(": ");
    }

    out.print("mode=");
    out.print(bridgeModeName(mode));
    out.print(", bytesObs=");
    out.print(stats.bytesObserved);
    out.print(", bytesFwd=");
    out.print(stats.bytesForwarded);
    out.print(", rtcmFramesFwd=");
    out.print(stats.rtcmFramesForwarded);
    out.print(", nmeaObs=");
    out.print(stats.nmeaLinesObserved);
    out.print(", nmeaFwd=");
    out.print(stats.nmeaLinesForwarded);
    out.print(", nmeaResyncs=");
    out.print(stats.nmeaLineResyncs);

    if (uptimeMs > 0) {
        out.print(", uptimeMs=");
        out.print(uptimeMs);
    }

    out.println();
}

void LC29H_GNSS::printRawIngressStatus(
    Stream& out,
    const char* label,
    const RawIngressStats& stats,
    uint32_t uptimeMs) {
    if (label != nullptr && label[0] != '\0') {
        out.print(label);
        out.print(": ");
    }

    out.print("bytesRead=");
    out.print(stats.bytesRead);
    out.print(", bytesWritten=");
    out.print(stats.bytesWritten);
    out.print(", shortWrites=");
    out.print(stats.shortWrites);

    if (uptimeMs > 0) {
        out.print(", uptimeMs=");
        out.print(uptimeMs);
    }

    out.println();
}

bool LC29H_GNSS::_writeLine(const String& line) {
    const size_t written = _gnss.print(line);
    const bool ok = written == line.length();

    if (!ok) {
        _setError(ErrorCode::StreamWriteFailed, "_writeLine: stream write length mismatch");
    }

    _debugPrint(String("TX: ") + line);
    return ok;
}

void LC29H_GNSS::_debugPrint(const String& text) {
    if (_debug == nullptr) {
        return;
    }
    _debug->println(text);
}

void LC29H_GNSS::_emitEvent(EventType type, ErrorCode code, const String& context) {
    if (_eventHandler == nullptr) {
        return;
    }

    Event e{type, code, context};
    _eventHandler(e, _eventUserData);
}

void LC29H_GNSS::_setError(ErrorCode code, const String& context) {
    _lastError = code;
    _lastErrorContext = context;
    _emitEvent(EventType::Error, code, context);
}

void LC29H_GNSS::_clearError() {
    _lastError = ErrorCode::None;
    _lastErrorContext = "";
}

bool LC29H_GNSS::_tryParseSurveyInConfigLine(const String& line, EcefPosition& out) const {
    String s = line;
    s.trim();

    if (s.startsWith("$")) {
        s.remove(0, 1);
    }

    const int starPos = s.indexOf('*');
    if (starPos >= 0) {
        s = s.substring(0, starPos);
    }

    // Expected response format:
    // PQTMCFGSVIN,OK,<Mode>,<MinDur>,<3D_AccLimit>,<ECEF_X>,<ECEF_Y>,<ECEF_Z>[,...]
    // We only require fields through Z; optional trailing fields are ignored.
    if (!s.startsWith("PQTMCFGSVIN,OK")) {
        return false;
    }

    String fields[16];
    const size_t n = splitCsv(s, fields, 16);
    if (n < 8) {
        return false;
    }

    const int mode = fields[2].toInt();
    if (mode <= 0) {
        return false;
    }

    out.x = fields[5].toFloat();
    out.y = fields[6].toFloat();
    out.z = fields[7].toFloat();
    out.valid = true;
    return true;
}

void LC29H_GNSS::_toNmeaDegrees(double degrees, bool isLatitude, String& value, char& hemi) {
    hemi = (degrees >= 0.0) ? (isLatitude ? 'N' : 'E') : (isLatitude ? 'S' : 'W');

    const double absDeg = fabs(degrees);
    const int wholeDeg = static_cast<int>(absDeg);
    const double minutes = (absDeg - static_cast<double>(wholeDeg)) * 60.0;

    char buf[24] = {0};
    if (isLatitude) {
        snprintf(buf, sizeof(buf), "%02d%07.4f", wholeDeg, minutes);
    } else {
        snprintf(buf, sizeof(buf), "%03d%07.4f", wholeDeg, minutes);
    }

    value = String(buf);
}

bool LC29H_GNSS::_handleConsoleLine(const String& line) {
    String cmd = line;
    cmd.trim();

    const int firstSpace = cmd.indexOf(' ');
    String head = (firstSpace < 0) ? cmd : cmd.substring(0, firstSpace);
    head.toLowerCase();

    if (head == "help") {
        if (firstSpace >= 0) {
            const String helpArgs = cmd.substring(firstSpace + 1);
            if (printDetailedCommandHelp(_console, helpArgs)) {
                return true;
            }
            _console->println("Unknown help topic. Type help.");
            return false;
        }

        printHelpOverview(*_console);
        _console->println("Tip: use help <command> for details, e.g. help base_survey");
        return true;
    }

    if (head == "status") {
        return queryVersion() && queryReceiverMode() && querySurveyIn() && queryBaudRate(1);
    }

    if (head == "restore") {
        return restoreDefaults();
    }

    if (head == "save") {
        return saveConfig();
    }

    if (head == "reboot") {
        return rebootModule();
    }

    if (head == "rover") {
        uint16_t rateMs = 200;
        if (firstSpace >= 0) {
            rateMs = static_cast<uint16_t>(cmd.substring(firstSpace + 1).toInt());
            if (rateMs == 0) {
                rateMs = 200;
            }
        }
        return configureRover(rateMs);
    }

    if (head == "base") {
        return configureBaseStation();
    }

    if (head == "mode_query") {
        return queryReceiverMode();
    }

    if (head == "profile_uas") {
        uint32_t fixMs = 200;
        bool save = true;
        bool verify = true;

        if (firstSpace >= 0) {
            String args = cmd.substring(firstSpace + 1);
            args.replace(' ', ',');
            String fields[4];
            const size_t n = splitCsv(args, fields, 4);

            if (n >= 1) {
                const uint32_t parsed = static_cast<uint32_t>(fields[0].toInt());
                if (parsed > 0) {
                    fixMs = parsed;
                }
            }
            if (n >= 2) {
                save = fields[1].toInt() != 0;
            }
            if (n >= 3) {
                verify = fields[2].toInt() != 0;
            }
        }

        const ProfileResult r = applyUasRoverProfile(fixMs, save, verify);
        if (r.status == ProfileStatus::Success) {
            _console->println("UAS rover profile applied.");
            return true;
        }
        _console->println("UAS rover profile failed.");
        return false;
    }

    if (head == "profile_base_survey") {
        uint32_t sec = 300;
        float stdDev = 15.0f;
        bool rtcm = true;
        bool save = true;
        bool verify = true;

        if (firstSpace >= 0) {
            String args = cmd.substring(firstSpace + 1);
            args.replace(' ', ',');
            String fields[6];
            const size_t n = splitCsv(args, fields, 6);

            if (n >= 1) {
                const uint32_t parsed = static_cast<uint32_t>(fields[0].toInt());
                if (parsed > 0) {
                    sec = parsed;
                }
            }
            if (n >= 2) {
                const float parsed = fields[1].toFloat();
                if (parsed > 0.0f) {
                    stdDev = parsed;
                }
            }
            if (n >= 3) {
                rtcm = fields[2].toInt() != 0;
            }
            if (n >= 4) {
                save = fields[3].toInt() != 0;
            }
            if (n >= 5) {
                verify = fields[4].toInt() != 0;
            }
        }

        const ProfileResult r = applySurveyBaseProfile(sec, stdDev, rtcm, save, verify);
        if (r.status == ProfileStatus::Success) {
            _console->println("Survey base profile applied.");
            return true;
        }
        _console->println("Survey base profile failed.");
        return false;
    }

    if (head == "profile_base_static") {
        if (firstSpace < 0) {
            _console->println("Usage: profile_base_static <lat> <lon> <alt> [rtcm0or1] [save0or1] [verify0or1]");
            return false;
        }

        const String args = cmd.substring(firstSpace + 1);
        String fields[8];
        size_t n = 0;
        int start = 0;
        while (start <= static_cast<int>(args.length()) && n < 8) {
            while (start < static_cast<int>(args.length()) && args[start] == ' ') {
                ++start;
            }
            if (start >= static_cast<int>(args.length())) {
                break;
            }
            const int space = args.indexOf(' ', start);
            if (space < 0) {
                fields[n++] = args.substring(start);
                break;
            }
            fields[n++] = args.substring(start, space);
            start = space + 1;
        }

        if (n < 3) {
            _console->println("Usage: profile_base_static <lat> <lon> <alt> [rtcm0or1] [save0or1] [verify0or1]");
            return false;
        }

        const double lat = fields[0].toFloat();
        const double lon = fields[1].toFloat();
        const double alt = fields[2].toFloat();
        bool rtcm = (n >= 4) ? (fields[3].toInt() != 0) : true;
        bool save = (n >= 5) ? (fields[4].toInt() != 0) : true;
        bool verify = (n >= 6) ? (fields[5].toInt() != 0) : true;

        const ProfileResult r = applyStaticBaseProfile(lat, lon, alt, rtcm, save, verify);
        if (r.status == ProfileStatus::Success) {
            _console->println("Static base profile applied.");
            return true;
        }
        _console->println("Static base profile failed.");
        return false;
    }

    if (head == "survey_capture") {
        uint32_t timeoutMs = 2000;
        if (firstSpace >= 0) {
            const uint32_t parsed = static_cast<uint32_t>(cmd.substring(firstSpace + 1).toInt());
            if (parsed > 0) {
                timeoutMs = parsed;
            }
        }
        const bool ok = querySurveyInAndCaptureEcef(timeoutMs);
        if (ok) {
            _console->println("Captured survey ECEF from PQTMCFGSVIN response.");
        } else {
            _console->println("Failed to capture survey ECEF (timeout or response missing).");
        }
        return ok;
    }

    if (head == "survey_pos") {
        if (!hasCapturedSurveyEcef()) {
            _console->println("No captured survey ECEF yet.");
            return false;
        }
        const EcefPosition p = getCapturedSurveyEcef();
        _console->println(String("ECEF X=") + String(p.x, 4) + ", Y=" + String(p.y, 4) + ", Z=" + String(p.z, 4));
        return true;
    }

    if (head == "survey_status") {
        uint16_t tail = 0;
        if (firstSpace >= 0) {
            const int parsed = cmd.substring(firstSpace + 1).toInt();
            if (parsed > 0) {
                tail = static_cast<uint16_t>(parsed);
            }
        }
        printSurveyAccuracyStatus(*_console, tail);
        return true;
    }

    if (head == "rover_status") {
        uint16_t tail = 0;
        if (firstSpace >= 0) {
            const int parsed = cmd.substring(firstSpace + 1).toInt();
            if (parsed > 0) {
                tail = static_cast<uint16_t>(parsed);
            }
        }
        printRoverAccuracyStatus(*_console, tail);
        return true;
    }

    if (head == "survey_apply") {
        bool save = true;
        if (firstSpace >= 0) {
            const int parsed = cmd.substring(firstSpace + 1).toInt();
            save = parsed != 0;
        }
        const bool ok = applyCapturedSurveyEcefAsFixed(save);
        if (!ok) {
            _console->println("Failed to apply captured survey ECEF as fixed base.");
        }
        return ok;
    }

    if (head == "survey_finalize") {
        uint32_t timeoutMs = 2000;
        bool save = true;

        if (firstSpace >= 0) {
            const String args = cmd.substring(firstSpace + 1);
            const int p = args.indexOf(' ');
            if (p < 0) {
                const uint32_t parsedTimeout = static_cast<uint32_t>(args.toInt());
                if (parsedTimeout > 0) {
                    timeoutMs = parsedTimeout;
                }
            } else {
                const uint32_t parsedTimeout = static_cast<uint32_t>(args.substring(0, p).toInt());
                if (parsedTimeout > 0) {
                    timeoutMs = parsedTimeout;
                }
                save = args.substring(p + 1).toInt() != 0;
            }
        }

        const PresetResult r = finalizeSurveyInToFixedBase(timeoutMs, save);
        if (r == PresetResult::Success) {
            _console->println("Survey finalized to fixed ECEF.");
            return true;
        }
        if (r == PresetResult::SaveFailed) {
            _console->println("Survey ECEF applied, but save failed.");
        } else {
            _console->println("Survey finalize failed.");
        }
        return false;
    }

    if (head == "base_survey") {
        uint32_t sec = 300;
        float stdDev = 15.0f;

        if (firstSpace >= 0) {
            const String args = cmd.substring(firstSpace + 1);
            const int secondSpace = args.indexOf(' ');
            if (secondSpace < 0) {
                const uint32_t parsed = static_cast<uint32_t>(args.toInt());
                if (parsed > 0) {
                    sec = parsed;
                }
            } else {
                const uint32_t parsedSec = static_cast<uint32_t>(args.substring(0, secondSpace).toInt());
                if (parsedSec > 0) {
                    sec = parsedSec;
                }
                const float parsedStd = args.substring(secondSpace + 1).toFloat();
                if (parsedStd > 0.0f) {
                    stdDev = parsedStd;
                }
            }
        }

        return configureBaseSurveyIn(sec, stdDev);
    }

    if (head == "base_fixed") {
        if (firstSpace < 0) {
            _console->println("Usage: base_fixed <lat> <lon> <alt>");
            return false;
        }

        const String args = cmd.substring(firstSpace + 1);
        const int p1 = args.indexOf(' ');
        const int p2 = (p1 < 0) ? -1 : args.indexOf(' ', p1 + 1);

        if (p1 < 0 || p2 < 0) {
            _console->println("Usage: base_fixed <lat> <lon> <alt>");
            return false;
        }

        const double lat = args.substring(0, p1).toFloat();
        const double lon = args.substring(p1 + 1, p2).toFloat();
        const double alt = args.substring(p2 + 1).toFloat();

        return configureBaseFixed(lat, lon, alt);
    }

    if (head == "rtcm") {
        if (firstSpace < 0) {
            _console->println("Usage: rtcm on|off");
            return false;
        }

        String state = cmd.substring(firstSpace + 1);
        state.trim();
        state.toLowerCase();

        if (state == "on") {
            return enableRTCM(true);
        }
        if (state == "off") {
            return enableRTCM(false);
        }

        _console->println("Usage: rtcm on|off");
        return false;
    }

    if (head == "msg_on") {
        if (firstSpace < 0) {
            _console->println("Usage: msg_on <name> [port]");
            return false;
        }

        const String args = cmd.substring(firstSpace + 1);
        const int p = args.indexOf(' ');
        const String name = (p < 0) ? args : args.substring(0, p);
        uint8_t port = 1;
        if (p >= 0) {
            const int parsed = args.substring(p + 1).toInt();
            if (parsed > 0 && parsed <= 255) {
                port = static_cast<uint8_t>(parsed);
            }
        }
        return enableMessageOutput(name, port);
    }

    if (head == "msg_off") {
        if (firstSpace < 0) {
            _console->println("Usage: msg_off <name> [port]");
            return false;
        }

        const String args = cmd.substring(firstSpace + 1);
        const int p = args.indexOf(' ');
        const String name = (p < 0) ? args : args.substring(0, p);
        uint8_t port = 1;
        if (p >= 0) {
            const int parsed = args.substring(p + 1).toInt();
            if (parsed > 0 && parsed <= 255) {
                port = static_cast<uint8_t>(parsed);
            }
        }
        return disableMessageOutput(name, port);
    }

    if (head == "msg_query") {
        if (firstSpace < 0) {
            _console->println("Usage: msg_query <name> [port]");
            return false;
        }

        const String args = cmd.substring(firstSpace + 1);
        const int p = args.indexOf(' ');
        const String name = (p < 0) ? args : args.substring(0, p);
        uint8_t port = 1;
        if (p >= 0) {
            const int parsed = args.substring(p + 1).toInt();
            if (parsed > 0 && parsed <= 255) {
                port = static_cast<uint8_t>(parsed);
            }
        }
        return queryMessageRate(name, port);
    }

    if (head == "baud") {
        if (firstSpace < 0) {
            _console->println("Usage: baud <rate> [port]");
            return false;
        }

        const String args = cmd.substring(firstSpace + 1);
        const int p = args.indexOf(' ');
        const uint32_t rate = (p < 0)
                                  ? static_cast<uint32_t>(args.toInt())
                                  : static_cast<uint32_t>(args.substring(0, p).toInt());

        if (rate == 0) {
            _console->println("Usage: baud <rate> [port]");
            return false;
        }

        uint8_t port = 1;
        if (p >= 0) {
            const int parsed = args.substring(p + 1).toInt();
            if (parsed > 0 && parsed <= 255) {
                port = static_cast<uint8_t>(parsed);
            }
        }

        return setBaudRate(rate, port);
    }

    if (head == "baud_query") {
        uint8_t port = 1;
        if (firstSpace >= 0) {
            const int parsed = cmd.substring(firstSpace + 1).toInt();
            if (parsed > 0 && parsed <= 255) {
                port = static_cast<uint8_t>(parsed);
            }
        }
        return queryBaudRate(port);
    }

    if (head == "fixrate") {
        if (firstSpace < 0) {
            _console->println("Usage: fixrate <ms>");
            return false;
        }
        const uint32_t rateMs = static_cast<uint32_t>(cmd.substring(firstSpace + 1).toInt());
        if (rateMs == 0) {
            _console->println("Usage: fixrate <ms>");
            return false;
        }
        return setFixRateMs(rateMs);
    }

    if (head == "fixrate_query") {
        return queryFixRate();
    }

    if (head == "hot") {
        return hotStart();
    }

    if (head == "warm") {
        return warmStart();
    }

    if (head == "cold") {
        return coldStart();
    }

    if (head == "gnss_start") {
        return startGnss();
    }

    if (head == "gnss_stop") {
        return stopGnss();
    }

    if (head == "uid") {
        return queryUniqueId();
    }

    if (head == "qver") {
        return queryQVersion();
    }

    if (head == "families" || head == "groups") {
        printCommandFamilySummary(*_console);
        return true;
    }

    if (head == "send") {
        if (firstSpace < 0) {
            _console->println("Usage: send <PQTM/PAIR payload>");
            return false;
        }

        String payload = cmd.substring(firstSpace + 1);
        payload.trim();
        return sendSentence(payload);
    }

    _console->println("Unknown command. Type help.");
    return false;
}
