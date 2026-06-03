/*
 * POTA QSO Parser Implementation
 * Adapted from Morse Buddy project for VAIL SUMMIT
 */

#include "pota_qso_parser.h"
#include <time.h>
#include <ctype.h>

// US State abbreviations
const char* const POTAQSOParser::US_STATES[] = {
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA",
    "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD",
    "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ",
    "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC",
    "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY",
    "DC", "PR", "VI"
};
const int POTAQSOParser::US_STATES_COUNT = sizeof(US_STATES) / sizeof(US_STATES[0]);

// Canadian provinces
const char* const POTAQSOParser::CA_PROVINCES[] = {
    "AB", "BC", "MB", "NB", "NL", "NS", "NT", "NU", "ON", "PE", "QC", "SK", "YT"
};
const int POTAQSOParser::CA_PROVINCES_COUNT = sizeof(CA_PROVINCES) / sizeof(CA_PROVINCES[0]);

POTAQSOParser::POTAQSOParser() {
    memset(_myCallsign, 0, sizeof(_myCallsign));
    memset(_myPark, 0, sizeof(_myPark));
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
    _wordPos = 0;
    _state = POTAQSOState::IDLE;
    _lastActivityMs = 0;
    _hasNewQSO = false;
    _qsoCount = 0;
    _gotTheirCall = false;
    _gotRST = false;
    _gotQTH = false;
    _sawBK = false;
    _sawTU = false;
    _saw7 = false;
    _bkCount = 0;
    _hasPendingCall = false;
    memset(_pendingCallsign, 0, sizeof(_pendingCallsign));
    memset(&_currentQSO, 0, sizeof(_currentQSO));
    memset(&_lastCompletedQSO, 0, sizeof(_lastCompletedQSO));
}

void POTAQSOParser::setMyCallsign(const char* call) {
    strncpy(_myCallsign, call, sizeof(_myCallsign) - 1);
    _myCallsign[sizeof(_myCallsign) - 1] = '\0';
    for (int i = 0; _myCallsign[i]; i++) {
        _myCallsign[i] = toupper(_myCallsign[i]);
    }
}

void POTAQSOParser::setMyPark(const char* parkRef) {
    strncpy(_myPark, parkRef, sizeof(_myPark) - 1);
    _myPark[sizeof(_myPark) - 1] = '\0';
    for (int i = 0; _myPark[i]; i++) {
        _myPark[i] = toupper(_myPark[i]);
    }
}

void POTAQSOParser::feedText(const char* newText) {
    checkTimeout();

    for (int i = 0; newText[i]; i++) {
        char c = toupper(newText[i]);

        if (c == ' ' || c == '\n' || c == '\r') {
            if (_wordPos > 0) {
                _wordBuffer[_wordPos] = '\0';

                POTAParsedToken token;
                token.type = classifyToken(_wordBuffer);
                strncpy(token.value, _wordBuffer, sizeof(token.value) - 1);
                token.value[sizeof(token.value) - 1] = '\0';
                token.confidence = 80;

                processToken(token);

                _wordPos = 0;
                memset(_wordBuffer, 0, sizeof(_wordBuffer));
            }
        } else if (_wordPos < sizeof(_wordBuffer) - 1) {
            _wordBuffer[_wordPos++] = c;
        }
    }
}

POTATokenType POTAQSOParser::classifyToken(const char* token) {
    if (!token || !token[0]) return POTATokenType::UNKNOWN;

    // Check for prosigns and common words first
    if (strcmp(token, "CQ") == 0 || strcmp(token, "CQCQ") == 0) {
        return POTATokenType::CQ;
    }
    if (strcmp(token, "POTA") == 0) {
        return POTATokenType::POTA;
    }
    if (strcmp(token, "DE") == 0) {
        return POTATokenType::DE;
    }
    if (strcmp(token, "K") == 0 || strcmp(token, "KN") == 0) {
        return POTATokenType::K;
    }
    if (strcmp(token, "TU") == 0 || strcmp(token, "TNX") == 0 || strcmp(token, "TKS") == 0 ||
        strcmp(token, "GA") == 0 || strcmp(token, "GE") == 0 || strcmp(token, "GM") == 0) {
        return POTATokenType::TU;
    }
    if (strcmp(token, "R") == 0 || strcmp(token, "RR") == 0) {
        return POTATokenType::R;
    }
    if (strcmp(token, "73") == 0 || strcmp(token, "72") == 0) {
        return POTATokenType::SEVENTY_THREE;
    }
    if (strcmp(token, "BK") == 0) {
        return POTATokenType::BK;
    }
    if (strcmp(token, "UR") == 0) {
        return POTATokenType::UR;
    }

    // Check for POTA park reference (K-1234, VE-0001)
    if (isPOTAPark(token)) {
        return POTATokenType::POTA_PARK;
    }

    // Check for RST (599, 5NN, 559)
    if (isRST(token)) {
        return POTATokenType::RST;
    }

    // Check for callsign
    if (isCallsign(token)) {
        return POTATokenType::CALLSIGN;
    }

    // Check for state abbreviation
    if (isStateAbbrev(token)) {
        return POTATokenType::STATE;
    }

    // Check for grid square
    if (isGridSquare(token)) {
        return POTATokenType::GRID_SQUARE;
    }

    return POTATokenType::UNKNOWN;
}

bool POTAQSOParser::isCallsign(const char* token) {
    int len = strlen(token);
    if (len < 3 || len > 8) return false;

    bool hasDigit = false;
    bool hasLetter = false;
    int digitPos = -1;

    for (int i = 0; i < len; i++) {
        if (isdigit(token[i])) {
            hasDigit = true;
            if (digitPos < 0) digitPos = i;
        } else if (isalpha(token[i])) {
            hasLetter = true;
        } else {
            return false;
        }
    }

    if (!hasDigit || !hasLetter) return false;
    if (digitPos < 1 || digitPos > 3) return false;
    if (!isalpha(token[len - 1])) return false;

    // Filter out common false positives
    if (strcmp(token, "5NN") == 0 || strcmp(token, "599") == 0) return false;
    if (len == 2) return false;

    return true;
}

bool POTAQSOParser::isRST(const char* token) {
    int len = strlen(token);
    if (len != 3) return false;

    char r = token[0];
    char s = token[1];
    char t = token[2];

    if (r == 'N') r = '9';
    if (s == 'N') s = '9';
    if (t == 'N') t = '9';

    if (r < '1' || r > '5') return false;
    if (s < '1' || s > '9') return false;
    if (t < '1' || t > '9') return false;

    return true;
}

bool POTAQSOParser::isPOTAPark(const char* token) {
    int len = strlen(token);
    if (len < 5 || len > 10) return false;

    const char* hyphen = strchr(token, '-');
    if (!hyphen) return false;

    int prefixLen = hyphen - token;
    if (prefixLen < 1 || prefixLen > 4) return false;

    for (int i = 0; i < prefixLen; i++) {
        if (!isalpha(token[i])) return false;
    }

    const char* numPart = hyphen + 1;
    int numLen = strlen(numPart);
    if (numLen < 1 || numLen > 5) return false;

    for (int i = 0; i < numLen; i++) {
        if (!isdigit(numPart[i])) return false;
    }

    return true;
}

bool POTAQSOParser::isStateAbbrev(const char* token) {
    if (strlen(token) != 2) return false;

    char upper[3];
    upper[0] = toupper(token[0]);
    upper[1] = toupper(token[1]);
    upper[2] = '\0';

    for (int i = 0; i < US_STATES_COUNT; i++) {
        if (strcmp(upper, US_STATES[i]) == 0) return true;
    }

    for (int i = 0; i < CA_PROVINCES_COUNT; i++) {
        if (strcmp(upper, CA_PROVINCES[i]) == 0) return true;
    }

    return false;
}

bool POTAQSOParser::isGridSquare(const char* token) {
    int len = strlen(token);
    if (len != 4 && len != 6) return false;

    char c1 = toupper(token[0]);
    char c2 = toupper(token[1]);
    if (c1 < 'A' || c1 > 'R' || c2 < 'A' || c2 > 'R') return false;

    if (!isdigit(token[2]) || !isdigit(token[3])) return false;

    if (len == 6) {
        char c5 = toupper(token[4]);
        char c6 = toupper(token[5]);
        if (c5 < 'A' || c5 > 'X' || c6 < 'A' || c6 > 'X') return false;
    }

    return true;
}

void POTAQSOParser::normalizeRST(const char* input, char* output) {
    for (int i = 0; i < 3 && input[i]; i++) {
        if (input[i] == 'N') {
            output[i] = '9';
        } else {
            output[i] = input[i];
        }
    }
    output[3] = '\0';
}

// POTA Activator single-sided exchange flow:
// 1. I send: [hunter call] TU UR [RST] [RST] [my QTH] [my QTH] BK
// 2. Hunter responds (we don't hear this)
// 3. I send closing: BK TU [hunter QTH] 73 E E
//
// We capture:
// - Hunter callsign (first callsign I send that's not mine)
// - RST I sent (after UR)
// - Hunter's QTH (state/grid in closing, after BK TU)
// - Auto-log on 73

void POTAQSOParser::processToken(const POTAParsedToken& token) {
    _lastActivityMs = millis();

    // Handle "7" "3" as separate tokens
    if (strcmp(token.value, "7") == 0) {
        _saw7 = true;
        return;
    }

    // Check if this is "3" following a "7"
    bool is73 = (token.type == POTATokenType::SEVENTY_THREE);
    if (strcmp(token.value, "3") == 0 && _saw7) {
        is73 = true;
    }
    _saw7 = false;

    // Create effective token
    POTAParsedToken effectiveToken = token;
    if (is73 && token.type != POTATokenType::SEVENTY_THREE) {
        effectiveToken.type = POTATokenType::SEVENTY_THREE;
        strcpy(effectiveToken.value, "73");
    }

    switch (_state) {
        case POTAQSOState::IDLE:
            // Look for a callsign that's not mine
            if (effectiveToken.type == POTATokenType::CALLSIGN) {
                if (strcmp(effectiveToken.value, _myCallsign) != 0) {
                    strncpy(_pendingCallsign, effectiveToken.value, sizeof(_pendingCallsign) - 1);
                    _pendingCallsign[sizeof(_pendingCallsign) - 1] = '\0';
                    _hasPendingCall = true;
                    Serial.print("[POTA Parser] Pending callsign: ");
                    Serial.println(_pendingCallsign);
                }
            }
            // Exchange markers confirm QSO start
            else if (_hasPendingCall &&
                     (effectiveToken.type == POTATokenType::TU ||
                      effectiveToken.type == POTATokenType::UR ||
                      effectiveToken.type == POTATokenType::RST)) {
                // Start new QSO
                memset(&_currentQSO, 0, sizeof(_currentQSO));
                strncpy(_currentQSO.myCallsign, _myCallsign, sizeof(_currentQSO.myCallsign) - 1);
                strncpy(_currentQSO.myPark, _myPark, sizeof(_currentQSO.myPark) - 1);
                strncpy(_currentQSO.theirCallsign, _pendingCallsign, sizeof(_currentQSO.theirCallsign) - 1);
                _gotTheirCall = true;
                _gotRST = false;
                _gotQTH = false;
                _sawBK = false;
                _sawTU = false;
                _bkCount = 0;
                _hasPendingCall = false;
                transitionTo(POTAQSOState::SENT_EXCHANGE);
                Serial.print("[POTA Parser] QSO started with: ");
                Serial.println(_currentQSO.theirCallsign);

                // If this was RST, capture it
                if (effectiveToken.type == POTATokenType::RST) {
                    normalizeRST(effectiveToken.value, _currentQSO.rstSent);
                    _gotRST = true;
                }
            }
            break;

        case POTAQSOState::SENT_EXCHANGE:
            // I'm sending my exchange to the hunter
            if (effectiveToken.type == POTATokenType::RST && !_gotRST) {
                normalizeRST(effectiveToken.value, _currentQSO.rstSent);
                _gotRST = true;
                Serial.print("[POTA Parser] RST sent: ");
                Serial.println(_currentQSO.rstSent);
            }
            else if (effectiveToken.type == POTATokenType::BK) {
                _bkCount++;
                _sawBK = true;
                _sawTU = false;  // Reset TU flag for next BK TU detection
                Serial.printf("[POTA Parser] BK #%d heard\n", _bkCount);

                // First BK ends my exchange, hunter will respond
                // Second BK starts my closing
                if (_bkCount >= 2) {
                    transitionTo(POTAQSOState::CLOSING);
                    Serial.println("[POTA Parser] Entering closing phase");
                }
            }
            else if (effectiveToken.type == POTATokenType::SEVENTY_THREE) {
                // 73 while in exchange = quick close, log it
                if (_gotTheirCall) {
                    finalizeQSO();
                    Serial.println("[POTA Parser] QSO logged on 73 (quick close)");
                }
            }
            else if (effectiveToken.type == POTATokenType::CALLSIGN) {
                // New callsign - might be starting next QSO
                if (strcmp(effectiveToken.value, _currentQSO.theirCallsign) != 0 &&
                    strcmp(effectiveToken.value, _myCallsign) != 0) {
                    // Log current if we have enough
                    if (_gotTheirCall) {
                        finalizeQSO();
                    }
                    // Start new QSO
                    memset(&_currentQSO, 0, sizeof(_currentQSO));
                    strncpy(_currentQSO.myCallsign, _myCallsign, sizeof(_currentQSO.myCallsign) - 1);
                    strncpy(_currentQSO.myPark, _myPark, sizeof(_currentQSO.myPark) - 1);
                    strncpy(_currentQSO.theirCallsign, effectiveToken.value, sizeof(_currentQSO.theirCallsign) - 1);
                    _gotTheirCall = true;
                    _gotRST = false;
                    _gotQTH = false;
                    _sawBK = false;
                    _sawTU = false;
                    _bkCount = 0;
                    Serial.print("[POTA Parser] New QSO started: ");
                    Serial.println(effectiveToken.value);
                }
            }
            break;

        case POTAQSOState::CLOSING:
            // I'm sending my closing: BK TU [hunter QTH] 73
            if (effectiveToken.type == POTATokenType::TU && _sawBK) {
                _sawTU = true;  // TU after BK = expect QTH next
                Serial.println("[POTA Parser] TU after BK - expecting QTH");
            }
            else if ((effectiveToken.type == POTATokenType::STATE ||
                      effectiveToken.type == POTATokenType::GRID_SQUARE) && !_gotQTH) {
                // Capture hunter's QTH
                strncpy(_currentQSO.stateReceived, effectiveToken.value, sizeof(_currentQSO.stateReceived) - 1);
                _gotQTH = true;
                Serial.print("[POTA Parser] Hunter QTH: ");
                Serial.println(_currentQSO.stateReceived);
            }
            else if (effectiveToken.type == POTATokenType::POTA_PARK) {
                // Park-to-park!
                strncpy(_currentQSO.theirPark, effectiveToken.value, sizeof(_currentQSO.theirPark) - 1);
                Serial.print("[POTA Parser] Park-to-Park: ");
                Serial.println(_currentQSO.theirPark);
            }
            else if (effectiveToken.type == POTATokenType::SEVENTY_THREE) {
                // 73 = log the QSO
                if (_gotTheirCall) {
                    finalizeQSO();
                    Serial.println("[POTA Parser] QSO logged on 73");
                }
            }
            else if (effectiveToken.type == POTATokenType::BK) {
                _bkCount++;
                _sawBK = true;
            }
            else if (effectiveToken.type == POTATokenType::CALLSIGN) {
                // New callsign - next QSO starting
                if (strcmp(effectiveToken.value, _currentQSO.theirCallsign) != 0 &&
                    strcmp(effectiveToken.value, _myCallsign) != 0) {
                    // Log current
                    if (_gotTheirCall) {
                        finalizeQSO();
                    }
                    // Start new QSO
                    memset(&_currentQSO, 0, sizeof(_currentQSO));
                    strncpy(_currentQSO.myCallsign, _myCallsign, sizeof(_currentQSO.myCallsign) - 1);
                    strncpy(_currentQSO.myPark, _myPark, sizeof(_currentQSO.myPark) - 1);
                    strncpy(_currentQSO.theirCallsign, effectiveToken.value, sizeof(_currentQSO.theirCallsign) - 1);
                    _gotTheirCall = true;
                    _gotRST = false;
                    _gotQTH = false;
                    _sawBK = false;
                    _sawTU = false;
                    _bkCount = 0;
                    transitionTo(POTAQSOState::SENT_EXCHANGE);
                    Serial.print("[POTA Parser] New QSO: ");
                    Serial.println(effectiveToken.value);
                }
            }
            break;

        case POTAQSOState::QSO_COMPLETE:
            // QSO logged, look for next one
            if (effectiveToken.type == POTATokenType::CALLSIGN) {
                if (strcmp(effectiveToken.value, _myCallsign) != 0) {
                    // New hunter
                    memset(&_currentQSO, 0, sizeof(_currentQSO));
                    strncpy(_currentQSO.myCallsign, _myCallsign, sizeof(_currentQSO.myCallsign) - 1);
                    strncpy(_currentQSO.myPark, _myPark, sizeof(_currentQSO.myPark) - 1);
                    strncpy(_currentQSO.theirCallsign, effectiveToken.value, sizeof(_currentQSO.theirCallsign) - 1);
                    _gotTheirCall = true;
                    _gotRST = false;
                    _gotQTH = false;
                    _sawBK = false;
                    _sawTU = false;
                    _bkCount = 0;
                    transitionTo(POTAQSOState::SENT_EXCHANGE);
                    Serial.print("[POTA Parser] Next QSO: ");
                    Serial.println(effectiveToken.value);
                }
            } else {
                // Go back to IDLE after some activity without callsign
                transitionTo(POTAQSOState::IDLE);
            }
            break;
    }
}

void POTAQSOParser::transitionTo(POTAQSOState newState) {
    _state = newState;
    _lastActivityMs = millis();
}

void POTAQSOParser::finalizeQSO() {
    fillTimestamp(_currentQSO);
    _currentQSO.isComplete = true;

    memcpy(&_lastCompletedQSO, &_currentQSO, sizeof(POTAQSORecord));
    _hasNewQSO = true;
    _qsoCount++;

    transitionTo(POTAQSOState::QSO_COMPLETE);

    Serial.print("[POTA Parser] QSO LOGGED: ");
    Serial.print(_currentQSO.theirCallsign);
    Serial.print(" RST: ");
    Serial.print(_currentQSO.rstSent);
    Serial.print(" QTH: ");
    Serial.println(_currentQSO.stateReceived);
}

void POTAQSOParser::fillTimestamp(POTAQSORecord& qso) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    gmtime_r(&now, &timeinfo);

    qso.timestamp = (uint32_t)now;

    snprintf(qso.qsoDate, sizeof(qso.qsoDate), "%04d%02d%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);

    snprintf(qso.timeOn, sizeof(qso.timeOn), "%02d%02d%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

void POTAQSOParser::checkTimeout() {
    if (_state != POTAQSOState::IDLE && _lastActivityMs > 0) {
        if (millis() - _lastActivityMs > QSO_TIMEOUT_MS) {
            // Timeout - save partial if we have callsign
            if (_gotTheirCall) {
                _currentQSO.isComplete = false;
                fillTimestamp(_currentQSO);
                memcpy(&_lastCompletedQSO, &_currentQSO, sizeof(POTAQSORecord));
                _hasNewQSO = true;
                _qsoCount++;
                Serial.println("[POTA Parser] QSO timeout - saved partial");
            }
            transitionTo(POTAQSOState::IDLE);
            _gotTheirCall = false;
            _gotRST = false;
            _gotQTH = false;
            _sawBK = false;
            _sawTU = false;
            _bkCount = 0;
        }
    }
}

bool POTAQSOParser::hasNewQSO() const {
    return _hasNewQSO;
}

POTAQSORecord POTAQSOParser::getLastQSO() {
    _hasNewQSO = false;
    return _lastCompletedQSO;
}

POTAQSOState POTAQSOParser::getState() const {
    return _state;
}

const char* POTAQSOParser::getCurrentCallsign() const {
    return _currentQSO.theirCallsign;
}

const char* POTAQSOParser::getCurrentRST() const {
    return _currentQSO.rstSent;
}

const char* POTAQSOParser::getCurrentState() const {
    return _currentQSO.stateReceived;
}

const char* POTAQSOParser::getCurrentPark() const {
    return _currentQSO.theirPark;
}

int POTAQSOParser::getQSOCount() const {
    return _qsoCount;
}

const char* POTAQSOParser::getStateString() const {
    switch (_state) {
        case POTAQSOState::IDLE: return "IDLE";
        case POTAQSOState::SENT_EXCHANGE: return "EXCHANGE";
        case POTAQSOState::CLOSING: return "CLOSING";
        case POTAQSOState::QSO_COMPLETE: return "LOGGED!";
        default: return "UNKNOWN";
    }
}

void POTAQSOParser::reset() {
    _state = POTAQSOState::IDLE;
    _wordPos = 0;
    memset(_wordBuffer, 0, sizeof(_wordBuffer));
    memset(&_currentQSO, 0, sizeof(_currentQSO));
    _gotTheirCall = false;
    _gotRST = false;
    _gotQTH = false;
    _sawBK = false;
    _sawTU = false;
    _saw7 = false;
    _bkCount = 0;
    _hasPendingCall = false;
    memset(_pendingCallsign, 0, sizeof(_pendingCallsign));
    _lastActivityMs = 0;
}
