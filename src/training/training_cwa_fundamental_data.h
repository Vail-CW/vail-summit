/*
 * CW Academy Fundamental Track - Content Data
 * Word lists and phrases organized by effective WPM level
 * Based on CWA Fundamental Curriculum v2.0
 */

#ifndef TRAINING_CWA_FUNDAMENTAL_DATA_H
#define TRAINING_CWA_FUNDAMENTAL_DATA_H

// ============================================
// Short common words for 6-7 WPM sessions
// Sessions 1-5: Building ICR foundation
// ============================================

const char* cwaFundamentalWords6WPM[] = {
  "THE", "AND", "FOR", "ARE", "BUT", "NOT", "YOU", "ALL",
  "CAN", "HER", "WAS", "ONE", "OUR", "OUT", "DAY", "HAD",
  "HOT", "HAS", "HIS", "HOW", "ITS", "MAY", "NEW", "NOW",
  "OLD", "SEE", "TWO", "WAY", "WHO", "BOY", "DID", "GET",
  "LET", "PUT", "SAY", "SHE", "TOO", "USE", "MAN", "RUN",
  "END", "SET", "TRY", "OWN", "TOP", "ANY", "BIG", "ASK",
  "EAT", "FAR", "FEW", "GOT", "HIM", "JOB", "KEY", "LAY",
  "LOW", "MET", "NOR", "OFF", "PAY", "RED", "SIT", "SUN",
  nullptr
};

// ============================================
// Medium words for 8-9 WPM sessions
// Sessions 6-11: Building speed and vocabulary
// ============================================

const char* cwaFundamentalWords8WPM[] = {
  "ABOUT", "AFTER", "AGAIN", "BEING", "BELOW", "COULD",
  "EVERY", "FIRST", "FOUND", "GREAT", "HOUSE", "LARGE",
  "LEARN", "NEVER", "OTHER", "PLACE", "PLANT", "POINT",
  "RIGHT", "SMALL", "SOUND", "SPELL", "STILL", "STUDY",
  "THEIR", "THERE", "THESE", "THING", "THINK", "THREE",
  "WATER", "WHERE", "WHICH", "WORLD", "WOULD", "WRITE",
  "YEARS", "AGAIN", "BEGIN", "BRING", "CARRY", "CAUSE",
  "CHECK", "CLEAR", "CLOSE", "COVER", "EARLY", "EARTH",
  "EIGHT", "ENJOY", "ENTER", "EQUAL", "EXACT", "FIELD",
  "FINAL", "FORCE", "FRONT", "GREEN", "GROUP", "HAPPY",
  "HEARD", "HEART", "HEAVY", "HORSE", "HUMAN", "KNOWN",
  "LEAVE", "LEVEL", "LIGHT", "MIGHT", "NIGHT", "NORTH",
  "OFTEN", "ORDER", "PAPER", "PIECE", "PLAIN", "POWER",
  "PRESS", "QUITE", "RADIO", "RANGE", "REACH", "READY",
  "RIVER", "ROUND", "SEVEN", "SHALL", "SHORT", "SHOWN",
  "SINCE", "SLEEP", "SOUTH", "SPEAK", "STAND", "START",
  "STATE", "STORY", "TABLE", "TAKEN", "TEACH", "THIRD",
  "THOSE", "TODAY", "TRADE", "TRAIN", "TREES", "UNDER",
  "UNTIL", "VOICE", "WATCH", "WHILE", "WHITE", "WHOLE",
  "WOMAN", "WRONG", "YOUNG", nullptr
};

// ============================================
// Longer words for 10-11 WPM sessions
// Sessions 12-16: Operating speed preparation
// ============================================

const char* cwaFundamentalWords10WPM[] = {
  "ALWAYS", "ANSWER", "BEFORE", "CHANGE", "DIFFERENT",
  "FOLLOW", "HAPPEN", "IMPORTANT", "LETTER", "LISTEN",
  "MOTHER", "NUMBER", "PEOPLE", "PICTURE", "QUESTION",
  "READING", "SENTENCE", "SHOULD", "SOMETHING", "STATION",
  "THOUGHT", "THROUGH", "TOGETHER", "WEATHER", "WITHOUT",
  "WORKING", "WRITING", "ANOTHER", "BELIEVE", "BETWEEN",
  "BROUGHT", "BUSINESS", "CERTAIN", "CHILDREN", "COUNTRY",
  "DECIDED", "DEVELOP", "EXAMPLE", "FACTORY", "FINALLY",
  "GENERAL", "GETTING", "HIMSELF", "HOWEVER", "HUNDRED",
  "INCLUDE", "INSTEAD", "INTEREST", "KEEPING", "LEARNED",
  "MEANING", "MEASURE", "MORNING", "NOTHING", "PATTERN",
  "PERHAPS", "PRESENT", "PROBLEM", "PROGRAM", "PROVIDE",
  "PURPOSE", "RUNNING", "SCIENCE", "SEVERAL", "STARTED",
  "STOPPED", "SUPPORT", "SURFACE", "TEACHER", "THINKING",
  "THROUGH", "TOGETHER", "TROUBLE", "USUALLY", "WALKING",
  "WANTING", "WHETHER", "WRITTEN", nullptr
};

// ============================================
// Common CW abbreviations
// Used across all sessions
// ============================================

const char* cwaFundamentalAbbreviations[] = {
  "CQ", "DE", "ES", "UR", "RST", "ANT", "RIG", "WX",
  "TNX", "FB", "OM", "YL", "XYL", "HI", "PSE", "AGN",
  "BK", "BTU", "CFM", "CL", "CUL", "DX", "FER", "GA",
  "GB", "GE", "GM", "GN", "HPE", "HR", "HW", "LID",
  "NR", "NW", "OB", "OP", "PWR", "R", "RCVR", "RPT",
  "SIG", "SKED", "SRI", "TU", "VY", "WKD", "WL", "XMTR",
  "ABT", "ADR", "AGE", "ANI", "BN", "BURO", "C", "CHK",
  "CK", "CLG", "CPY", "CUD", "CW", "DR", "DWN", "EL",
  "ENUF", "ERE", "FWD", "GD", "GLD", "GND", "GUD", "HAM",
  "INFO", "K", "KN", "LNG", "LTR", "MNI", "MSG", "NIL",
  "NM", "NW", "OC", "PLS", "QRM", "QRN", "QRP", "QRZ",
  "QSB", "QSL", "QSO", "QSY", "QTH", "RX", "SD", "SGD",
  "SN", "STN", "TMW", "TT", "TX", "U", "UFB", "VFB",
  "WID", "WKG", "WRK", "WUD", "XCVR", "XMT", "YR", "ZN",
  nullptr
};

// ============================================
// Simple QSO phrases
// Used in sessions 10-16 for exchange practice
// ============================================

const char* cwaFundamentalPhrases[] = {
  "NAME IS", "QTH IS", "RST IS", "RIG IS", "ANT IS",
  "WX IS", "THANK YOU", "GOOD LUCK", "BEST 73", "CUL",
  "NICE QSO", "FB COPY", "SOLID COPY", "HOW COPY",
  "UR RST", "MY NAME", "MY QTH", "MY RIG", "MY ANT",
  "FINE BUSINESS", "GOOD SIGNAL", "THANKS FOR QSO",
  "HOPE TO CU", "HAVE A GOOD", "TAKE CARE", "ALL THE BEST",
  "MANY THANKS", "GOOD DAY", "GOOD EVENING", "GOOD MORNING",
  nullptr
};

// ============================================
// Common first names for QSO practice
// ============================================

const char* cwaFundamentalNames[] = {
  "BOB", "JIM", "TOM", "BILL", "JACK", "MIKE", "DAVE",
  "JOHN", "PAUL", "PETE", "RICK", "STEVE", "MARK", "DAN",
  "ED", "JOE", "KEN", "RON", "AL", "ART", "BEN", "CARL",
  "DON", "FRED", "GARY", "HAL", "IVAN", "JERRY", "KURT",
  "LOU", "MAX", "NEIL", "OSCAR", "PAT", "RAY", "SAM",
  "TED", "VIC", "WALT", "WAYNE", "ALEX", nullptr
};

// ============================================
// Common QTH (locations) for QSO practice
// ============================================

const char* cwaFundamentalQTH[] = {
  "NY", "CA", "TX", "FL", "PA", "OH", "IL", "MI", "GA", "NC",
  "NJ", "VA", "WA", "AZ", "MA", "TN", "IN", "MO", "MD", "WI",
  "CO", "MN", "SC", "AL", "LA", "KY", "OR", "OK", "CT", "IA",
  "UT", "NV", "AR", "MS", "KS", "NM", "NE", "WV", "ID", "HI",
  "NH", "ME", "MT", "RI", "DE", "SD", "ND", "AK", "VT", "WY",
  nullptr
};

#endif // TRAINING_CWA_FUNDAMENTAL_DATA_H
