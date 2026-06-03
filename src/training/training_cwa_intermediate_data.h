/*
 * CW Academy Intermediate Track - Training Data
 * Session-specific content for words, prefixes, suffixes, QSO, and POTA exchanges
 * Based on CWA Intermediate Level CW Curriculum (Version 2.1)
 *
 * Focus: Speed building (10-25 WPM) and word/prefix/suffix recognition
 * All characters are already known - this track builds speed and recognition skills
 */

#ifndef TRAINING_CWA_INTERMEDIATE_DATA_H
#define TRAINING_CWA_INTERMEDIATE_DATA_H

// ===========================================
// Word Lists - Progressive Difficulty by Series
// ===========================================

// 101 Series - Basic common words (4-5 chars) - Sessions 1-3
const char* inter_words_101[] = {
  "THE", "AND", "FOR", "ARE", "BUT", "NOT", "YOU", "ALL",
  "CAN", "HAD", "HER", "WAS", "ONE", "OUR", "OUT", "DAY",
  "GET", "HAS", "HIM", "HIS", "HOW", "MAN", "NEW", "NOW",
  "OLD", "SEE", "WAY", "WHO", "BOY", "DID", nullptr
};

// 102 Series - Building vocabulary
const char* inter_words_102[] = {
  "ALSO", "BACK", "BEEN", "CALL", "COME", "COULD", "DOWN",
  "EVEN", "FIND", "FIRST", "FROM", "GOOD", "HAVE", "HERE",
  "INTO", "JUST", "KNOW", "LIKE", "LONG", "LOOK", "MADE",
  "MAKE", "MORE", "MOST", "MUCH", "MUST", "NAME", nullptr
};

// 103 Series - Speed increase content
const char* inter_words_103[] = {
  "NEED", "NEXT", "ONLY", "OVER", "PART", "SAME", "SOME",
  "SUCH", "TAKE", "THAN", "THAT", "THEM", "THEN", "THERE",
  "THEY", "THIS", "TIME", "VERY", "WANT", "WELL", "WERE",
  "WHAT", "WHEN", "WILL", "WITH", "WORK", "YEAR", nullptr
};

// 202 Series - Intermediate words (5-6 chars) - Sessions 4-6
const char* inter_words_202[] = {
  "ABOUT", "AFTER", "AGAIN", "BEING", "EVERY", "FIRST",
  "GREAT", "HOUSE", "LARGE", "LITTLE", "NIGHT", "OTHER",
  "PLACE", "RIGHT", "SMALL", "SOUND", "STILL", "THEIR",
  "THESE", "THING", "THINK", "THREE", "WATER", "WHERE",
  "WHICH", "WHILE", "WORLD", "WOULD", "WRITE", nullptr
};

// 203 Series
const char* inter_words_203[] = {
  "ALWAYS", "BEFORE", "BETTER", "CALLED", "CHANGE", "COMES",
  "COULD", "DOING", "DURING", "ENOUGH", "FOLLOW", "FOUND",
  "GOING", "GROUP", "HAVING", "HEARD", "HELP", "ITSELF",
  "KEEPING", "LATER", "LEARN", "LISTEN", "LIVING", "MEANS",
  "MIGHT", "MOVING", "NUMBER", "PEOPLE", nullptr
};

// 205 Series
const char* inter_words_205[] = {
  "ANSWER", "AROUND", "BECAME", "BECOME", "BEGIN", "BELOW",
  "BESIDE", "BETTER", "BETWEEN", "BRING", "BUILD", "CARRY",
  "CLOSE", "COMING", "COUNTRY", "COURSE", "COVER", "CREATE",
  "DIFFERENT", "EARLY", "EARTH", "EITHER", "ENDING", nullptr
};

// 301 Series - Longer words (6-7 chars) - Sessions 7-10
const char* inter_words_301[] = {
  "ALREADY", "ANOTHER", "BECAUSE", "BELIEVE", "BETWEEN",
  "CERTAIN", "CHILDREN", "COMPLETE", "CONSIDER", "CONTINUE",
  "COUNTRY", "DEVELOP", "DIFFERENT", "DURING", "EXAMPLE",
  "FAMILY", "FEELING", "FINALLY", "FOLLOWING", "FORWARD",
  "FRIENDS", "GENERAL", "GETTING", "GIVING", nullptr
};

// 302 Series
const char* inter_words_302[] = {
  "GOING", "GOVERNMENT", "HAPPENING", "HAVING", "HERSELF",
  "HIMSELF", "HOWEVER", "IMPORTANT", "INCLUDE", "INTEREST",
  "ITSELF", "KEEPING", "KNOWING", "LEARNING", "LEAVING",
  "LOOKING", "MAKING", "MEANING", "MEETING", "MORNING",
  "MYSELF", "NOTHING", "OFFICE", nullptr
};

// 303 Series
const char* inter_words_303[] = {
  "OPENING", "ORDER", "PERHAPS", "PERSON", "PICTURE",
  "PLAYING", "POINT", "POSSIBLE", "PRESENT", "PROBLEM",
  "PRODUCE", "PROGRAM", "PROVIDE", "PUBLIC", "QUESTION",
  "READING", "REASON", "RESULT", "RUNNING", "SAYING",
  "SCHOOL", "SECOND", "SEEING", nullptr
};

// 304 Series
const char* inter_words_304[] = {
  "SERVICE", "SEVERAL", "SHOULD", "SHOWING", "SIMPLE",
  "SINCE", "SOMETHING", "SOMETIMES", "SPECIAL", "STARTED",
  "STATE", "STILL", "STORY", "STUDENT", "STUDY", "SYSTEM",
  "TAKING", "TELLING", "THINKING", "THROUGH", "TODAY",
  "TOGETHER", "TOMORROW", nullptr
};

// 401 Series - Complex words (7-8 chars) - Sessions 11-16
const char* inter_words_401[] = {
  "ACTIVITY", "ACTUALLY", "ADDITION", "AFTERNOON", "AGREEMENT",
  "ALTHOUGH", "AMERICAN", "ANYTHING", "APPEARED", "ATTENTION",
  "AVAILABLE", "BEAUTIFUL", "BEGINNING", "BUILDING", "BUSINESS",
  "CERTAINLY", "CHARACTER", "CHILDREN", "COMMUNITY", "COMPANY",
  "COMPLETE", "CONDITION", "CONSIDER", nullptr
};

// 402 Series
const char* inter_words_402[] = {
  "CONTINUE", "CONTROL", "DECISION", "DESCRIBE", "DETERMINE",
  "DEVELOP", "DIRECTION", "DISCOVER", "DISTANCE", "ECONOMIC",
  "EDUCATION", "EFFECTIVE", "ELECTION", "ESPECIALLY", "ESTABLISH",
  "EVIDENCE", "EXAMPLE", "EXCELLENT", "EXPECTED", "EXPERIENCE",
  "EXPLAIN", "EXTENDED", nullptr
};

// 403 Series - 20 WPM content
const char* inter_words_403[] = {
  "FAMILIAR", "FAVORITE", "FINANCIAL", "FOLLOWING", "FORWARD",
  "FUNCTION", "GENERALLY", "GOVERNMENT", "HAPPENING", "HOSPITAL",
  "HOWEVER", "IDENTIFY", "IMPORTANT", "IMPOSSIBLE", "IMPROVE",
  "INCLUDE", "INCREASE", "INDUSTRY", "INFLUENCE", "INFORMATION",
  "INSTEAD", "INTEREST", nullptr
};

// 404 Series
const char* inter_words_404[] = {
  "INTRODUCE", "INVOLVED", "KNOWLEDGE", "LANGUAGE", "LEARNING",
  "MATERIAL", "MEASURE", "MEETING", "MILITARY", "MINISTER",
  "MORNING", "MOVEMENT", "NATIONAL", "NATURAL", "NECESSARY",
  "NEWSPAPER", "NOTHING", "OFFICIAL", "OPERATION", "OPPORTUNITY",
  "ORIGINAL", nullptr
};

// 405 Series - 25 WPM content
const char* inter_words_405[] = {
  "OURSELVES", "PARAGRAPH", "PARTICULAR", "PERFORMANCE", "PERMANENT",
  "PERSONAL", "PHYSICAL", "POLITICAL", "POPULATION", "POSITION",
  "POSSIBLE", "PRACTICE", "PRESIDENT", "PRESSURE", "PROBABLY",
  "PROBLEM", "PRODUCE", "PRODUCTION", "PROFESSIONAL", "PROGRAM",
  "PROGRESS", "PROJECT", nullptr
};

// ===========================================
// Prefix Word Lists - Common Prefixes
// ===========================================

// DIS- prefix words
const char* inter_prefix_dis[] = {
  "DISABLE", "DISCARD", "DISCORD", "DISCUSS", "DISEASE",
  "DISGUST", "DISMISS", "DISPLAY", "DISPOSE", "DISPUTE",
  "DISRUPT", "DISTANT", "DISTORT", "DISTURB", "DISCOVER",
  "DISCOUNT", "DISCOURAGE", "DISLIKE", "DISORDER", nullptr
};

// IM- prefix words
const char* inter_prefix_im[] = {
  "IMAGE", "IMPACT", "IMPORT", "IMPOSE", "IMPRESS",
  "IMPROVE", "IMPULSE", "IMAGINE", "IMITATE", "IMMENSE",
  "IMMUNE", "IMPART", "IMPAIR", "IMPEACH", "IMPEDE",
  "IMPEL", "IMPEND", "IMPLY", nullptr
};

// IN- prefix words
const char* inter_prefix_in[] = {
  "INCLUDE", "INCOME", "INDEED", "INDEX", "INDOOR",
  "INFORM", "INJECT", "INLAND", "INSERT", "INSIDE",
  "INSIST", "INSPECT", "INSTALL", "INSTANT", "INSTEAD",
  "INTEND", "INTEREST", "INTERNAL", "INVENT", "INVEST",
  "INVITE", "INVOLVE", nullptr
};

// IR- prefix words (less common)
const char* inter_prefix_ir[] = {
  "IRONIC", "IRATE", "IRON", nullptr
};

// RE- prefix words
const char* inter_prefix_re[] = {
  "REACH", "REACT", "READY", "REASON", "RECALL",
  "RECENT", "RECORD", "REDUCE", "REFER", "REFORM",
  "REFUSE", "REGARD", "REGION", "RELATE", "REMAIN",
  "REMIND", "REMOTE", "REMOVE", "REPAIR", "REPEAT",
  "REPLACE", "REPORT", "REQUEST", "REQUIRE", "RESCUE",
  "RESERVE", "RESIST", "RESOLVE", "RESORT", "RESOURCE",
  "RESPOND", "RESTORE", "RESULT", "RETAIN", "RETIRE",
  "RETURN", "REVEAL", "REVIEW", "REVISE", "REWARD", nullptr
};

// UN- prefix words
const char* inter_prefix_un[] = {
  "UNABLE", "UNCLE", "UNDER", "UNFAIR", "UNHAPPY",
  "UNIQUE", "UNITED", "UNKNOWN", "UNLESS", "UNLIKE",
  "UNLOCK", "UNSAFE", "UNTIL", "UNUSUAL", "UPDATE",
  "UPPER", "UPSET", "URBAN", "URGENT", "USEFUL", nullptr
};

// ===========================================
// Suffix Word Lists - Common Suffixes
// ===========================================

// -ED suffix words
const char* inter_suffix_ed[] = {
  "ASKED", "CALLED", "CHANGED", "CLOSED", "ENDED",
  "FAILED", "HELPED", "JOINED", "KICKED", "LANDED",
  "MOVED", "NEEDED", "OPENED", "PLAYED", "RAISED",
  "SAILED", "TALKED", "TURNED", "USED", "WANTED",
  "WATCHED", "WORKED", "YELLED", "ZIPPED", nullptr
};

// -ES suffix words
const char* inter_suffix_es[] = {
  "BOXES", "BUSES", "CASES", "DISHES", "EDGES",
  "FIXES", "GASES", "HORSES", "INCHES", "JUDGES",
  "KISSES", "LOSSES", "MATCHES", "MIXES", "NURSES",
  "PASSES", "PLACES", "PRICES", "RAISES", "RICHES",
  "SIZES", "TAXES", "VOICES", "WISHES", nullptr
};

// -ING suffix words
const char* inter_suffix_ing[] = {
  "ASKING", "BEING", "CALLING", "COMING", "DOING",
  "EATING", "FALLING", "GETTING", "GIVING", "GOING",
  "HAVING", "HELPING", "JOINING", "KEEPING", "KNOWING",
  "LIVING", "LOOKING", "MAKING", "MOVING", "NOTHING",
  "OPENING", "PLAYING", "READING", "SAYING", "TAKING",
  "TELLING", "THINKING", "WAITING", "WALKING", "WORKING", nullptr
};

// -LY suffix words
const char* inter_suffix_ly[] = {
  "BADLY", "BARELY", "CLEARLY", "CLOSELY", "DAILY",
  "EASILY", "EXACTLY", "FAIRLY", "FINALLY", "FIRMLY",
  "GREATLY", "HARDLY", "LARGELY", "LATELY", "LIKELY",
  "MAINLY", "NEARLY", "ONLY", "PARTLY", "QUICKLY",
  "RARELY", "REALLY", "SAFELY", "SIMPLY", "SLOWLY",
  "SOFTLY", "SURELY", "USUALLY", "WIDELY", nullptr
};

// ===========================================
// QSO Exchange Data
// ===========================================

// Callsigns for QSO practice
const char* inter_qso_callsigns[] = {
  "W1AW", "K2ABC", "N3DEF", "WA4GHI", "KD5JKL",
  "W6MNO", "K7PQR", "N8STU", "WB9VWX", "KC0YZA",
  "W1XYZ", "K2MNO", "N3PQR", "WA4STU", "KD5VWX",
  "W6YZA", "K7BCD", "N8EFG", "WB9HIJ", "KC0KLM",
  "VE3ABC", "VE7XYZ", "G4ABC", "DL1XYZ", "JA1ABC", nullptr
};

// Operator names for QSO practice
const char* inter_qso_names[] = {
  "JOHN", "BOB", "TOM", "JIM", "BILL", "MIKE", "DAVE",
  "ROB", "DAN", "SAM", "PETE", "JACK", "GARY", "PAUL",
  "STEVE", "MARK", "FRED", "RICK", "TONY", "ALAN",
  "CHRIS", "SCOTT", "KEVIN", "BRIAN", "JEFF", nullptr
};

// QTH (locations) for QSO practice - US states
const char* inter_qso_qth[] = {
  "NY", "CA", "TX", "FL", "OH", "PA", "IL", "GA", "NC", "MI",
  "NJ", "VA", "WA", "AZ", "MA", "TN", "IN", "MO", "MD", "WI",
  "CO", "MN", "SC", "AL", "LA", "KY", "OR", "OK", "CT", "UT", nullptr
};

// RST reports
const char* inter_qso_rst[] = {
  "599", "589", "579", "569", "559", "549", "539",
  "449", "339", "229", "119", nullptr
};

// ===========================================
// POTA Exchange Data
// ===========================================

// Park designators
const char* inter_pota_parks[] = {
  "K-0001", "K-0042", "K-0156", "K-0234", "K-0345",
  "K-0456", "K-0567", "K-0678", "K-0789", "K-0890",
  "K-1001", "K-1234", "K-2345", "K-3456", "K-4567",
  "K-5678", "K-6789", "K-7890", "K-8901", "K-9012", nullptr
};

// ===========================================
// CWT Contest Exchange Data (Sessions 12+)
// ===========================================

// CWT names (short, common contest names)
const char* inter_cwt_names[] = {
  "BOB", "JIM", "ROB", "TOM", "DAN", "SAM", "JOE", "RON",
  "ED", "AL", "KEN", "DON", "RAY", "LEE", "BEN", "TED",
  "VIC", "ART", "MAX", "GUS", nullptr
};

// CWT member numbers (range 1-3000+)
const char* inter_cwt_numbers[] = {
  "3", "12", "22", "45", "67", "89", "123", "234", "345",
  "456", "567", "678", "789", "890", "1001", "1234", "1567",
  "1890", "2123", "2456", "2789", "3012", nullptr
};

// ===========================================
// Pointer arrays for session-based lookup
// ===========================================

// Words by series (use session number to index)
const char** inter_words_by_series[] = {
  inter_words_101,  // Sessions 1-3 use 101-103
  inter_words_102,
  inter_words_103,
  inter_words_202,  // Sessions 4-6 use 202-205
  inter_words_203,
  inter_words_205,
  inter_words_301,  // Sessions 7-10 use 301-304
  inter_words_302,
  inter_words_303,
  inter_words_304,
  inter_words_401,  // Sessions 11-16 use 401-405
  inter_words_402,
  inter_words_403,
  inter_words_404,
  inter_words_405,
  inter_words_405   // Session 16 reuses 405
};

// All prefix arrays
const char** inter_prefix_arrays[] = {
  inter_prefix_dis,
  inter_prefix_im,
  inter_prefix_in,
  inter_prefix_ir,
  inter_prefix_re,
  inter_prefix_un
};

const int INTER_PREFIX_COUNT = 6;

// All suffix arrays
const char** inter_suffix_arrays[] = {
  inter_suffix_ed,
  inter_suffix_es,
  inter_suffix_ing,
  inter_suffix_ly
};

const int INTER_SUFFIX_COUNT = 4;

// Suffix names for display
const char* inter_suffix_names[] = {
  "-ED", "-ES", "-ING", "-LY"
};

// Prefix names for display
const char* inter_prefix_names[] = {
  "DIS-", "IM-", "IN-", "IR-", "RE-", "UN-"
};

#endif // TRAINING_CWA_INTERMEDIATE_DATA_H
