/*
 * POTA QSO Parser
 * Parses decoded morse text to extract QSO information for POTA activations
 * Adapted from Morse Buddy project for VAIL SUMMIT
 */

#ifndef POTA_QSO_PARSER_H
#define POTA_QSO_PARSER_H

#include <Arduino.h>

// QSO state machine states - designed for POTA activator workflow
// Single-sided capture: we only hear what the activator sends
enum class POTAQSOState {
    IDLE,               // Waiting for a new QSO to start
    SENT_EXCHANGE,      // I sent my exchange (callsign + TU/UR), waiting
    CLOSING,            // Sending closing (BK TU [QTH] 73)
    QSO_COMPLETE        // QSO finished, ready to log
};

// Token types for parsed words
enum class POTATokenType {
    UNKNOWN,
    CALLSIGN,       // W1ABC, VE3XYZ, JA1ABC
    RST,            // 599, 559, 5NN
    STATE,          // FL, NY, CA, ON, BC
    GRID_SQUARE,    // FN42, EM85
    POTA_PARK,      // K-1234, VE-0001
    CQ,             // CQ, CQCQ
    POTA,           // POTA keyword
    DE,             // DE prosign
    K,              // K (go ahead)
    TU,             // Thank you
    R,              // Roger
    SEVENTY_THREE,  // 73
    BK,             // Break
    UR,             // Your/You're
    NAME            // Likely a name
};

// Parsed token structure
struct POTAParsedToken {
    POTATokenType type;
    char value[16];
    uint8_t confidence;     // 0-100
};

// QSO record for logging (simplified for POTA single-sided capture)
struct POTAQSORecord {
    char myCallsign[12];        // My callsign
    char theirCallsign[12];     // Hunter's callsign (echoed back by me)
    char qsoDate[9];            // YYYYMMDD format
    char timeOn[7];             // HHMMSS format
    char rstSent[4];            // RST I sent (captured)
    char rstReceived[4];        // RST received (usually blank - not captured)
    char stateReceived[8];      // Hunter's state/grid (captured from closing)
    char myPark[12];            // MY_SIG_INFO - my park
    char theirPark[12];         // SIG_INFO - their park (park-to-park)
    uint32_t timestamp;         // Unix timestamp
    bool isComplete;            // QSO completed?
};

class POTAQSOParser {
public:
    POTAQSOParser();

    // Configuration
    void setMyCallsign(const char* call);
    void setMyPark(const char* parkRef);

    // Main parsing interface
    void feedText(const char* newText);

    // Get parsing results
    bool hasNewQSO() const;
    POTAQSORecord getLastQSO();
    POTAQSOState getState() const;

    // Get current exchange info (for live display)
    const char* getCurrentCallsign() const;
    const char* getCurrentRST() const;
    const char* getCurrentState() const;
    const char* getCurrentPark() const;
    int getQSOCount() const;

    // Get state as string for display
    const char* getStateString() const;

    // Reset
    void reset();

private:
    // Token classification
    POTATokenType classifyToken(const char* token);
    bool isCallsign(const char* token);
    bool isRST(const char* token);
    bool isPOTAPark(const char* token);
    bool isStateAbbrev(const char* token);
    bool isGridSquare(const char* token);

    // Normalize RST (5NN -> 599)
    void normalizeRST(const char* input, char* output);

    // State machine
    void processToken(const POTAParsedToken& token);
    void transitionTo(POTAQSOState newState);
    void finalizeQSO();
    void checkTimeout();

    // Fill timestamp from RTC/NTP
    void fillTimestamp(POTAQSORecord& qso);

    // Configuration
    char _myCallsign[12];
    char _myPark[12];

    // Parsing buffer
    char _wordBuffer[32];
    uint8_t _wordPos;

    // State machine
    POTAQSOState _state;
    uint32_t _lastActivityMs;
    static const uint32_t QSO_TIMEOUT_MS = 120000;  // 2 minutes

    // Current QSO tracking
    POTAQSORecord _currentQSO;
    POTAQSORecord _lastCompletedQSO;
    bool _hasNewQSO;
    int _qsoCount;

    // Track what we've received in current QSO
    bool _gotTheirCall;     // We sent their callsign (starting the exchange)
    bool _gotRST;           // We sent RST
    bool _gotQTH;           // We sent their QTH (in closing)
    bool _sawBK;            // Saw "BK"
    bool _sawTU;            // Saw "TU" (after BK = entering closing)
    bool _saw7;             // Saw "7" - looking for "3" to complete 73
    int _bkCount;           // Count BKs to detect closing phase

    // Pending callsign - waiting for exchange marker to confirm QSO start
    char _pendingCallsign[12];
    bool _hasPendingCall;

    // State abbreviation tables
    static const char* const US_STATES[];
    static const int US_STATES_COUNT;
    static const char* const CA_PROVINCES[];
    static const int CA_PROVINCES_COUNT;
};

#endif // POTA_QSO_PARSER_H
