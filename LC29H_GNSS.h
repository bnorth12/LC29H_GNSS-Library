#pragma once

#include <Arduino.h>
#include <stdlib.h>

// High-level driver for Quectel LC29H/LC79H command workflows.
//
// Design choices:
// - Keep transport generic by depending only on Arduino Stream.
// - Keep command encoding simple: payload -> NMEA sentence with checksum.
// - Provide two layers:
//   1) low-level send/query helpers for custom control
//   2) preset/profile helpers for repeatable field setups
class LC29H_GNSS {
public:
    enum class ErrorCode {
        None,
        InvalidArgument,
        StreamWriteFailed,
        Timeout,
        ParseFailed,
        CommandFailed,
        SaveFailed,
        VerifyFailed,
        ShortWrite
    };

    enum class EventType {
        Info,
        Warning,
        Error
    };

    struct Event {
        EventType type;
        ErrorCode code;
        String context;
    };

    struct RecoveryPolicy {
        uint8_t commandRetries = 1;
        uint8_t queryRetries = 1;
        uint8_t rawWriteRetries = 1;
        uint16_t retryDelayMs = 10;
        bool emitRecoveryEvents = true;
    };

    typedef void (*EventHandler)(const Event& event, void* userData);

    // Captured ECEF coordinates from survey-in query response.
    // `valid` lets application code distinguish "not captured yet" from (0,0,0).
    struct EcefPosition {
        double x;
        double y;
        double z;
        bool valid;
    };

    // Result for preset helpers that optionally call saveConfig().
    enum class PresetResult {
        Success,
        CommandFailed,
        SaveFailed
    };

    // Result for mission-profile helpers that optionally perform verify queries.
    enum class ProfileStatus {
        Success,
        CommandFailed,
        SaveFailed,
        VerifyFailed
    };

    // Profile result carries both execution status and restart guidance.
    struct ProfileResult {
        ProfileStatus status;
        bool powerCycleRecommended;
    };

    enum class BridgeMode {
        ForwardAll,
        RtcmOnly,
        RtcmAndNmeaAllowlist
    };

    struct BridgeNmeaFilter {
        bool forwardGga = true;
        bool forwardGst = false;
        bool forwardRmc = false;
        bool forwardPqtm = false;
    };

    enum class LocalDebugOutputMode : uint8_t {
        None = 0,
        NmeaOnly = 1,
        RawBinary = 2
    };

    enum class CommandFamily : uint8_t {
        Identity,
        Lifecycle,
        CoreConfiguration,
        SurveyAndBase,
        OutputAndDiagnostics,
        TransportBridge,
        PairControl,
        ConsoleAndUtility,
        Generic
    };

    enum class CommandDirection : uint8_t {
        Write,
        Read,
        ReadWrite,
        Control
    };

    enum class CommandAckKind : uint8_t {
        None,
        PairAck,
        CommandOkError,
        StatusLine,
        DirectData
    };

    enum class CommandFieldType : uint8_t {
        Integer,
        Unsigned,
        Float,
        Text,
        Boolean,
        Bitmask,
        Enum,
        Degrees,
        Milliseconds,
        Seconds,
        Meters,
        Port,
        Rate,
        Payload
    };

    struct CommandFieldSpec {
        const char* name = nullptr;
        CommandFieldType type = CommandFieldType::Text;
        bool optional = false;
        const char* units = nullptr;
        const char* notes = nullptr;

                CommandFieldSpec() = default;
                CommandFieldSpec(const char* nameIn,
                                                 CommandFieldType typeIn,
                                                 bool optionalIn,
                                                 const char* unitsIn,
                                                 const char* notesIn)
                        : name(nameIn),
                            type(typeIn),
                            optional(optionalIn),
                            units(unitsIn),
                            notes(notesIn) {}
    };

    struct CommandResponseSpec {
        const char* prefix = nullptr;
        CommandAckKind ackKind = CommandAckKind::None;
        const char* notes = nullptr;

                CommandResponseSpec() = default;
                CommandResponseSpec(const char* prefixIn,
                                                        CommandAckKind ackKindIn,
                                                        const char* notesIn)
                        : prefix(prefixIn),
                            ackKind(ackKindIn),
                            notes(notesIn) {}
    };

    struct CommandMetadata {
        const char* base = nullptr;
        const char* wrapper = nullptr;
        CommandFamily family = CommandFamily::Generic;
        CommandDirection direction = CommandDirection::Write;
        CommandAckKind ackKind = CommandAckKind::None;
        const char* summary = nullptr;
        const char* firmwareGate = nullptr;
        const char* moduleGate = nullptr;
        bool saveRecommended = false;
        bool powerCycleRecommended = false;
        bool genericFallback = true;
        const CommandFieldSpec* requestFields = nullptr;
        size_t requestFieldCount = 0;
        const CommandResponseSpec* response = nullptr;
        const CommandFieldSpec* responseFields = nullptr;
        size_t responseFieldCount = 0;

        CommandMetadata() = default;
        CommandMetadata(const char* baseIn,
                const char* wrapperIn,
                CommandFamily familyIn,
                CommandDirection directionIn,
                CommandAckKind ackKindIn,
                const char* summaryIn,
                const char* firmwareGateIn,
                const char* moduleGateIn,
                bool saveRecommendedIn,
                bool powerCycleRecommendedIn,
                bool genericFallbackIn,
                const CommandFieldSpec* requestFieldsIn,
                size_t requestFieldCountIn,
                const CommandResponseSpec* responseIn,
                const CommandFieldSpec* responseFieldsIn,
                size_t responseFieldCountIn)
            : base(baseIn),
              wrapper(wrapperIn),
              family(familyIn),
              direction(directionIn),
              ackKind(ackKindIn),
              summary(summaryIn),
              firmwareGate(firmwareGateIn),
              moduleGate(moduleGateIn),
              saveRecommended(saveRecommendedIn),
              powerCycleRecommended(powerCycleRecommendedIn),
              genericFallback(genericFallbackIn),
              requestFields(requestFieldsIn),
              requestFieldCount(requestFieldCountIn),
              response(responseIn),
              responseFields(responseFieldsIn),
              responseFieldCount(responseFieldCountIn) {}
    };

    struct CommandFamilyDefaults {
        CommandFamily family = CommandFamily::Generic;
        const char* familyName = nullptr;
        const char* summary = nullptr;
        CommandDirection defaultDirection = CommandDirection::Write;
        CommandAckKind defaultAckKind = CommandAckKind::None;
        const CommandFieldSpec* defaultRequestFields = nullptr;
        size_t defaultRequestFieldCount = 0;
        const CommandFieldSpec* defaultResponseFields = nullptr;
        size_t defaultResponseFieldCount = 0;
        bool genericFallback = true;

        CommandFamilyDefaults() = default;
        CommandFamilyDefaults(CommandFamily familyIn,
                      const char* familyNameIn,
                      const char* summaryIn,
                      CommandDirection defaultDirectionIn,
                      CommandAckKind defaultAckKindIn,
                      const CommandFieldSpec* defaultRequestFieldsIn,
                      size_t defaultRequestFieldCountIn,
                      const CommandFieldSpec* defaultResponseFieldsIn,
                      size_t defaultResponseFieldCountIn,
                      bool genericFallbackIn)
            : family(familyIn),
              familyName(familyNameIn),
              summary(summaryIn),
              defaultDirection(defaultDirectionIn),
              defaultAckKind(defaultAckKindIn),
              defaultRequestFields(defaultRequestFieldsIn),
              defaultRequestFieldCount(defaultRequestFieldCountIn),
              defaultResponseFields(defaultResponseFieldsIn),
              defaultResponseFieldCount(defaultResponseFieldCountIn),
              genericFallback(genericFallbackIn) {}
    };

    struct PairAck {
        uint16_t commandId = 0;
        uint8_t result = 0;
    };

    struct BridgeStats {
        uint32_t bytesObserved = 0;
        uint32_t bytesForwarded = 0;
        uint32_t rtcmFramesForwarded = 0;
        uint32_t nmeaLinesObserved = 0;
        uint32_t nmeaLinesForwarded = 0;
        // False NMEA runs aborted because the line grew past kMaxBridgeNmeaChars or hit a
        // non-ASCII byte (typical when a '$' in RTCM payload is mistaken for an NMEA start).
        uint32_t nmeaLineResyncs = 0;
    };

    enum class BridgeParserState : uint8_t {
        Idle,
        Nmea,
        RtcmHdr2,
        RtcmHdr3,
        RtcmFrame
    };

    // NMEA 4.11 is 82 chars; Quectel PQTM lines can be longer. Anything past this is not a real
    // sentence and is treated as a desync (see forwardBridgeAvailable).
    static constexpr size_t kMaxBridgeNmeaChars = 256;
    // Hard cap so a slow NMEA callback (or mixed RTCM) cannot keep the pump in while(available)
    // until heap/stack collapse. Remaining bytes are processed on the next call.
    static constexpr uint32_t kMaxBridgePumpMs = 15;

    struct BridgeState {
        BridgeParserState state = BridgeParserState::Idle;
        String nmeaLine;
        uint8_t rtcmBuf[1100] = {0};
        size_t rtcmIndex = 0;
        size_t rtcmExpected = 0;
    };

    struct RawIngressStats {
        uint32_t bytesRead = 0;
        uint32_t bytesWritten = 0;
        uint32_t shortWrites = 0;
    };

    struct AccuracyTrackerConfig {
        bool enabled = false;
        uint32_t windowSec = 3600;
        uint16_t maxPoints = 120;
    };

    struct AccuracySample {
        uint32_t elapsedSec = 0;
        float estimate = 0.0f;
    };

    struct AccuracyTrackerSummary {
        bool enabled = false;
        uint32_t elapsedSec = 0;
        uint32_t windowSec = 0;
        uint32_t sampleIntervalSec = 0;
        uint16_t storedSamples = 0;
        uint16_t totalSamples = 0;
        float latest = 0.0f;
        float earliest = 0.0f;
        float best = 0.0f;
        float worst = 0.0f;
        float average = 0.0f;
        float delta = 0.0f;
    };

    explicit LC29H_GNSS(Stream& gnssStream, Stream* debugStream = nullptr);
    ~LC29H_GNSS();

    void setDebugStream(Stream* debugStream);
    void attachConsole(Stream& consoleStream);
    void setEventHandler(EventHandler handler, void* userData = nullptr);
    void setRecoveryPolicy(const RecoveryPolicy& policy);
    RecoveryPolicy getRecoveryPolicy() const;
    ErrorCode getLastError() const;
    String getLastErrorContext() const;
    void clearLastError();
    static const char* errorCodeName(ErrorCode code);
    void printLastError(Stream& out) const;

    bool queryVersion();
    bool queryQVersion();
    bool queryUniqueId();
    bool querySerialNumber();
    bool restoreDefaults();
    bool resetToDefaults();
    bool saveConfig();
    // Full module reboot (PAIR023). Required on DA/EA after PQTMSAVEPAR for CFGSVIN
    // (survey-in) and some other writes to take effect. PAIR003/PAIR002 GNSS sleep
    // is not a reboot and will not start <Obs>.
    bool rebootModule();

    // Sets rover mode and the receiver output/fix rate used by the rover preset.
    bool configureRover(uint16_t outputMs = 200);
    bool configureBaseStation();
    // Writes base mode + CFGSVIN. AccLimit is meters; 15 is the Quectel default that lets
    // <Obs> start on DA (0/2/8 left Obs at 0). Caller must PQTMSAVEPAR then rebootModule()
    // (PAIR023). PAIR003/PAIR002 GNSS sleep is not enough on DA.
    bool configureBaseSurveyIn(uint32_t minTimeSec = 300, float minStdDevM = 15.0f);
    bool querySurveyIn();
    // Sends a survey-in query and waits for an OK response containing ECEF values.
    // Returns true only when ECEF is parsed and stored in _capturedSurveyEcef.
    bool querySurveyInAndCaptureEcef(uint32_t timeoutMs = 2000);
    bool hasCapturedSurveyEcef() const;
    EcefPosition getCapturedSurveyEcef() const;
    // Writes fixed-base coordinates using ECEF values (no lat/lon conversion).
    bool setFixedEcef(double x, double y, double z);
    // Applies previously captured ECEF values back to module as fixed-base config.
    bool applyCapturedSurveyEcefAsFixed(bool save = true);
    // End-to-end helper: query survey result, capture ECEF, apply fixed ECEF, optional save.
    PresetResult finalizeSurveyInToFixedBase(uint32_t timeoutMs = 2000, bool save = true);
    bool configureBaseFixed(double latDeg, double lonDeg, double altM);

    bool setReceiverModeRover();
    bool setReceiverModeBase();
    bool queryReceiverMode();

    bool setMessageRate(const String& messageName, uint8_t port, uint8_t rate);
    bool queryMessageRate(const String& messageName, uint8_t port = 1);
    bool enableMessageOutput(const String& messageName, uint8_t port = 1);
    bool disableMessageOutput(const String& messageName, uint8_t port = 1);

    bool setConstellations(bool gps, bool glo, bool gal, bool bds, bool qzss = false, bool navic = false);
    bool queryConstellations();
    bool setNavMode(uint8_t mode);
    bool queryNavMode();
    bool setNmeaPrecision(uint8_t ggaDp = 3, uint8_t gsvDp = 6, uint8_t gsaDp = 1, uint8_t rmcDp = 2, uint8_t vtgDp = 3, uint8_t zdaDp = 2);
    bool queryNmeaPrecision();
    bool setNmeaTalkerId(const String& talker = "GP", uint8_t mode = 0);
    bool queryNmeaTalkerId();
    bool setProtocolMask(uint8_t inPort, uint8_t outPort, uint32_t inMask, uint32_t outMask);
    bool queryProtocolMask(uint8_t inPort, uint8_t outPort);
    bool setPulsePerSecondConfig(const String& argsCsv);
    bool queryPulsePerSecondConfig();

    bool setFixRateMs(uint32_t fixRateMs);
    bool queryFixRate();

    bool setBaudRate(uint32_t baudRate, uint8_t uartPort = 1);
    bool queryBaudRate(uint8_t uartPort = 1);

    bool startGnss();
    bool stopGnss();
    bool hotStart();
    bool warmStart();
    bool coldStart();

    bool queryDop();
    bool queryPvt();
    bool queryVelocity();
    bool queryStd();
    bool queryJammingStatus();
    bool queryGeoFenceStatus();
    bool queryOdometer();

    // Query helpers that return parsed values for app-level control logic.
    bool queryAndWaitLine(const String& queryPayload, const String& expectedPrefix, String& outLine, uint32_t timeoutMs = 1500);
    bool getReceiverMode(uint8_t& outMode, uint32_t timeoutMs = 1500);
    bool getFixRateMs(uint32_t& outFixRateMs, uint32_t timeoutMs = 1500);
    bool getBaudRate(uint32_t& outBaudRate, uint8_t uartPort = 1, uint32_t timeoutMs = 1500);
    bool getMessageRate(const String& messageName, uint8_t& outRate, uint8_t port = 1, uint32_t timeoutMs = 1500);

    // Enables/disables RTCM stream family controls used in the reference workflows.
    bool enableRTCM(bool enable = true);

    // Preset-level workflows for common setup paths.
    // Survey AccLimit default is 15 m (Quectel DA default that starts <Obs>).
    // Presets save when asked but do not reboot; call rebootModule() after SAVEPAR on DA/EA.
    PresetResult applyRoverPreset(uint16_t outputMs = 200, bool save = true);
    PresetResult applyBaseSurveyPreset(uint32_t minTimeSec = 300, float minStdDevM = 15.0f, bool enableRtcm = true, bool save = true);
    PresetResult applyBaseFixedPreset(double latDeg, double lonDeg, double altM, bool enableRtcm = true, bool save = true);

    // Mission profiles with optional verification queries after configuration.
    // applySurveyBaseProfile enables PQTMSVINSTATUS at RATE 1; lower bulky NMEA
    // (GSV / PQTMSVINSTATUS RATE 10) after apply, then SAVEPAR + rebootModule().
    ProfileResult applyUasRoverProfile(uint32_t fixRateMs = 200, bool save = true, bool verify = true);
    ProfileResult applySurveyBaseProfile(uint32_t minTimeSec = 300, float minStdDevM = 15.0f, bool enableRtcm = true, bool save = true, bool verify = true);
    ProfileResult applyStaticBaseProfile(double latDeg, double lonDeg, double altM, bool enableRtcm = true, bool save = true, bool verify = true);

    // Many module settings only fully take effect after save + rebootModule() (PAIR023).
    // These methods expose that recommendation without forcing hardware policy.
    bool isPowerCycleRecommended() const;
    void clearPowerCycleRecommended();

    bool sendCommand(const String& command, const String& argsCsv = "");
    bool sendCommand(const String& command, const String* args, size_t argCount);

    // sendPayload expects command body (without $ and checksum) and adds checksum.
    bool sendPayload(const String& payloadWithoutDollarOrChecksum);
    // sendSentence accepts either raw payload, $-prefixed no-checksum sentence,
    // or full sentence with existing checksum.
    bool sendSentence(const String& sentenceMaybeWithChecksum);

    // Raw byte path for RTCM/NMEA passthrough use cases.
    size_t writeRaw(const uint8_t* data, size_t len);
    size_t ingestRawAvailable(
        Stream& in,
        size_t maxBytes = 0,
        RawIngressStats* stats = nullptr,
        size_t chunkSize = 256);
    size_t forwardAvailable(Stream& out, size_t maxBytes = 0);
    bool forwardNmeaLine(Stream& out, uint32_t timeoutMs = 0);
    // Single UART reader for mixed NMEA+RTCM. Call every loop. Stops after maxBytes
    // or kMaxBridgePumpMs (15 ms). Do not parse/log/LittleFS inside localNmeaOut;
    // queue complete lines and process after the pump returns. Do not call readLine
    // or query* on the same Stream while this pump owns it.
    size_t forwardBridgeAvailable(
        Stream& out,
        BridgeState& state,
        BridgeMode mode,
        const BridgeNmeaFilter& nmeaFilter,
        bool nmeaFilterEnabled = true,
        BridgeStats* stats = nullptr,
        size_t maxBytes = 0,
        Stream* localNmeaOut = nullptr,
        Stream* localRawOut = nullptr);
    static bool isNmeaAllowedByFilter(const String& line, const BridgeNmeaFilter& filter);
    static const char* bridgeModeName(BridgeMode mode);
    static const char* localDebugOutputModeName(LocalDebugOutputMode mode);
    static const char* commandFamilyName(CommandFamily family);
    static const char* commandDirectionName(CommandDirection direction);
    static const char* commandAckKindName(CommandAckKind ackKind);
    static const char* commandFieldTypeName(CommandFieldType type);
    static CommandFamily inferCommandFamily(const String& commandOrPayload);
    static const CommandFamilyDefaults* getCommandFamilyDefaults(CommandFamily family);
    static CommandMetadata inferCommandMetadata(const String& commandOrPayload);
    static const CommandMetadata* findCommandMetadata(const String& commandOrPayload);
    static void printCommandMetadata(Stream& out, const String& commandOrPayload);
    static void printCommandFamilyDefaults(Stream& out, CommandFamily family);
    static void printCommandFamilySummary(Stream& out);
    static bool tryParsePairAck(const String& line, PairAck& outAck);
    static const char* pairAckResultName(uint8_t result);
    static void printPairAck(Stream& out, const PairAck& ack, const char* label = nullptr);
    bool readPairAck(PairAck& outAck, uint32_t timeoutMs = 1500);
    static void printBridgeStatus(
        Stream& out,
        const char* label,
        BridgeMode mode,
        const BridgeStats& stats,
        uint32_t uptimeMs = 0);
    static void printRawIngressStatus(
        Stream& out,
        const char* label,
        const RawIngressStats& stats,
        uint32_t uptimeMs = 0);

    void setSurveyAccuracyTrackerConfig(const AccuracyTrackerConfig& config);
    void setRoverAccuracyTrackerConfig(const AccuracyTrackerConfig& config);
    void resetSurveyAccuracyTracker();
    void resetRoverAccuracyTracker();
    bool getSurveyAccuracySummary(AccuracyTrackerSummary& out) const;
    bool getRoverAccuracySummary(AccuracyTrackerSummary& out) const;
    void printSurveyAccuracyStatus(Stream& out, uint16_t tailSamples = 0) const;
    void printRoverAccuracyStatus(Stream& out, uint16_t tailSamples = 0) const;

    bool readLine(String& outLine, uint32_t timeoutMs = 0);
    void processSerialCommands();

    // Helpers for payload/sentence construction and checksum generation.
    static String buildPayload(const String& command, const String& argsCsv = "");
    static String buildPayload(const String& command, const String* args, size_t argCount);
    static String makeSentence(const String& payloadWithoutDollarOrChecksum);
    static uint8_t nmeaChecksum(const String& payloadWithoutDollarOrChecksum);
    static bool isNmeaSentence(const String& line);

private:
    Stream& _gnss;
    Stream* _debug;
    Stream* _console;
    String _consoleBuffer;
    bool _powerCycleRecommended;
    RecoveryPolicy _recoveryPolicy;
    ErrorCode _lastError;
    String _lastErrorContext;
    EventHandler _eventHandler;
    void* _eventUserData;

    bool _writeLine(const String& line);
    void _debugPrint(const String& text);
    void _emitEvent(EventType type, ErrorCode code, const String& context);
    void _setError(ErrorCode code, const String& context);
    void _clearError();
    bool _tryParseSurveyInConfigLine(const String& line, EcefPosition& out) const;
    void _observeLineForAccuracy(const String& line);

    static constexpr uint16_t kAccuracyRingCapacity = 200;

    struct AccuracyTrackerState {
        AccuracyTrackerConfig config;
        AccuracySample* ring = nullptr;
        uint16_t capacity = 0;
        uint16_t head = 0;
        uint16_t count = 0;
        uint16_t total = 0;
        uint32_t startMs = 0;
        uint32_t lastSampleMs = 0;
        float latest = 0.0f;
        float best = 0.0f;
        float worst = 0.0f;
        double sum = 0.0;
    };

    void _applyAccuracyTrackerConfig(AccuracyTrackerState& state, const AccuracyTrackerConfig& config);
    void _resetAccuracyTrackerState(AccuracyTrackerState& state);
    bool _appendAccuracySample(AccuracyTrackerState& state, float estimate, uint32_t nowMs);
    bool _trackerSummary(const AccuracyTrackerState& state, AccuracyTrackerSummary& out) const;
    void _printAccuracyStatus(const char* label, const AccuracyTrackerState& state, Stream& out, uint16_t tailSamples) const;

    static void _toNmeaDegrees(double degrees, bool isLatitude, String& value, char& hemi);
    bool _handleConsoleLine(const String& line);

    EcefPosition _capturedSurveyEcef;
    AccuracyTrackerState _surveyAccuracy;
    AccuracyTrackerState _roverAccuracy;
};
