/*
 * Morse Story Time Game
 *
 * An educational game where players listen to stories played as Morse code
 * and answer comprehension questions. Ported from the web version.
 *
 * Design principles:
 * - Simple state machine: MENU, LISTENING, QUIZ, RESULTS
 * - Keyboard navigation for menus and quizzes
 * - Morse playback using existing audio infrastructure
 * - Progress persistence via Preferences
 */

#ifndef GAME_STORY_TIME_H
#define GAME_STORY_TIME_H

#include <Preferences.h>
#include "../core/config.h"
#include "../core/morse_code.h"
#include "../core/task_manager.h"  // For dual-core audio API
#include "../audio/i2s_audio.h"

// Forward declarations
extern void onLVGLBackNavigation();
extern int cwTone;
extern int cwSpeed;

// ============================================
// Constants
// ============================================

#define ST_MAX_QUESTIONS 5
#define ST_MAX_OPTIONS 4
#define ST_PLAYBACK_DELAY_MS 500    // Delay before playback starts
#define ST_FEEDBACK_DELAY_MS 800    // Show correct/wrong feedback

// ============================================
// Data Structures
// ============================================

// Difficulty levels
enum StoryDifficulty {
    STORY_TUTORIAL = 0,  // Very short (~25 words)
    STORY_EASY = 1,      // Short (~50-100 words)
    STORY_MEDIUM = 2,    // Moderate (~100-200 words)
    STORY_HARD = 3,      // Longer (~150-250 words)
    STORY_EXPERT = 4     // Complex (~200+ words)
};

// Get difficulty label
static const char* getDifficultyLabel(StoryDifficulty diff) {
    switch (diff) {
        case STORY_TUTORIAL: return "Tutorial";
        case STORY_EASY: return "Easy";
        case STORY_MEDIUM: return "Medium";
        case STORY_HARD: return "Hard";
        case STORY_EXPERT: return "Expert";
        default: return "Unknown";
    }
}

// Question structure
struct StoryQuestion {
    const char* question;
    const char* options[ST_MAX_OPTIONS];
    uint8_t correctIndex;
};

// Story structure (stored in PROGMEM)
struct StoryData {
    const char* id;
    const char* title;
    StoryDifficulty difficulty;
    uint16_t wordCount;
    const char* storyText;
    StoryQuestion questions[ST_MAX_QUESTIONS];
};

// Game state enum
enum StoryTimeState {
    ST_STATE_MENU,           // Main story time menu
    ST_STATE_DIFFICULTY,     // Selecting difficulty
    ST_STATE_STORY_LIST,     // Selecting a story
    ST_STATE_LISTENING,      // Playing story as morse
    ST_STATE_QUIZ,           // Answering questions
    ST_STATE_RESULTS,        // Showing results
    ST_STATE_SETTINGS,       // WPM/tone settings
    ST_STATE_PROGRESS        // View progress/stats
};

// Playback phase during listening
enum StoryPlayPhase {
    ST_PLAY_WAITING,         // Waiting to start
    ST_PLAY_PLAYING,         // Actively playing morse
    ST_PLAY_PAUSED,          // Paused by user
    ST_PLAY_COMPLETE         // Finished playing
};

// ============================================
// Session State
// ============================================

struct StoryTimeSession {
    // Current story info
    const StoryData* currentStory;
    int storyIndex;
    StoryDifficulty selectedDifficulty;

    // Playback state
    StoryPlayPhase playPhase;
    int playbackCharIndex;      // Current character position for pause/resume
    int playCount;              // How many times played this session
    bool hasListenedOnce;       // Must listen at least once before quiz

    // Playback settings
    int playbackWPM;            // Effective WPM (with Farnsworth spacing)
    int characterWPM;           // Character speed (for Farnsworth)
    bool useFarnsworth;
    int toneFrequency;

    // Quiz state
    int currentQuestion;        // 0-4
    int correctAnswers;         // Running count
    int selectedAnswers[ST_MAX_QUESTIONS];  // -1 if not answered
    bool questionAnswered[ST_MAX_QUESTIONS];

    // Timing
    unsigned long stateStartTime;
    unsigned long lastPlaybackTime;

    // Volatile flag for playback cancellation
    volatile bool stopPlayback;
};

// ============================================
// Progress Tracking
// ============================================

struct StoryProgress {
    bool completed;
    uint8_t bestScore;     // 0-5
    uint8_t attempts;      // Total attempts (caps at 255)
};

struct StoryTimeGlobalProgress {
    // Overall stats
    int totalStoriesCompleted;
    int totalPerfectScores;
    int totalQuestionsCorrect;
    int totalQuestionsAttempted;

    // Per-difficulty completion counts
    int completedByDifficulty[5];
    int perfectByDifficulty[5];

    // Settings
    int preferredWPM;
    int preferredCharWPM;
    int preferredTone;
    bool useFarnsworth;
};

// ============================================
// Global State
// ============================================

static StoryTimeSession stSession;
static StoryTimeGlobalProgress stProgress;
static Preferences stPrefs;

// ============================================
// Forward Declarations (from data file)
// ============================================

extern const StoryData* getStoryByIndex(int index);
extern int getStoryCount();
extern int getStoryCountByDifficulty(StoryDifficulty diff);
extern const StoryData* getStoryByDifficultyAndIndex(StoryDifficulty diff, int index);
extern int getStoryIndexInDifficulty(StoryDifficulty diff, int globalIndex);

// ============================================
// Preferences Functions
// ============================================

void stLoadProgress() {
    stPrefs.begin("storytime", true);  // Read-only

    stProgress.totalStoriesCompleted = stPrefs.getInt("completed", 0);
    stProgress.totalPerfectScores = stPrefs.getInt("perfect", 0);
    stProgress.totalQuestionsCorrect = stPrefs.getInt("qcorrect", 0);
    stProgress.totalQuestionsAttempted = stPrefs.getInt("qattempt", 0);
    stProgress.preferredWPM = stPrefs.getInt("wpm", 15);
    stProgress.preferredCharWPM = stPrefs.getInt("charwpm", 20);
    stProgress.preferredTone = stPrefs.getInt("tone", 600);
    stProgress.useFarnsworth = stPrefs.getBool("farnsworth", true);

    // Load per-difficulty stats
    for (int i = 0; i < 5; i++) {
        char key[16];
        snprintf(key, sizeof(key), "diff_%d", i);
        stProgress.completedByDifficulty[i] = stPrefs.getInt(key, 0);
        snprintf(key, sizeof(key), "perf_%d", i);
        stProgress.perfectByDifficulty[i] = stPrefs.getInt(key, 0);
    }

    stPrefs.end();

    Serial.printf("[StoryTime] Loaded progress: %d completed, %d perfect\n",
                  stProgress.totalStoriesCompleted, stProgress.totalPerfectScores);
}

void stSaveProgress() {
    stPrefs.begin("storytime", false);  // Read-write

    stPrefs.putInt("completed", stProgress.totalStoriesCompleted);
    stPrefs.putInt("perfect", stProgress.totalPerfectScores);
    stPrefs.putInt("qcorrect", stProgress.totalQuestionsCorrect);
    stPrefs.putInt("qattempt", stProgress.totalQuestionsAttempted);
    stPrefs.putInt("wpm", stProgress.preferredWPM);
    stPrefs.putInt("charwpm", stProgress.preferredCharWPM);
    stPrefs.putInt("tone", stProgress.preferredTone);
    stPrefs.putBool("farnsworth", stProgress.useFarnsworth);

    // Save per-difficulty stats
    for (int i = 0; i < 5; i++) {
        char key[16];
        snprintf(key, sizeof(key), "diff_%d", i);
        stPrefs.putInt(key, stProgress.completedByDifficulty[i]);
        snprintf(key, sizeof(key), "perf_%d", i);
        stPrefs.putInt(key, stProgress.perfectByDifficulty[i]);
    }

    stPrefs.end();
}

void stSaveSettings() {
    stPrefs.begin("storytime", false);
    stPrefs.putInt("wpm", stProgress.preferredWPM);
    stPrefs.putInt("charwpm", stProgress.preferredCharWPM);
    stPrefs.putInt("tone", stProgress.preferredTone);
    stPrefs.putBool("farnsworth", stProgress.useFarnsworth);
    stPrefs.end();
}

// Per-story progress (separate namespace due to many keys)
StoryProgress stGetStoryProgress(const char* storyId) {
    StoryProgress prog = {false, 0, 0};
    stPrefs.begin("st_prog", true);

    char key[32];
    snprintf(key, sizeof(key), "s_%s", storyId);
    uint8_t packed = stPrefs.getUChar(key, 0);

    if (packed != 0) {
        prog.completed = (packed & 0x80) != 0;
        prog.bestScore = (packed >> 4) & 0x07;
        prog.attempts = packed & 0x0F;
    }

    stPrefs.end();
    return prog;
}

void stSaveStoryProgress(const char* storyId, int score) {
    stPrefs.begin("st_prog", false);

    char key[32];
    snprintf(key, sizeof(key), "s_%s", storyId);

    // Get existing progress
    uint8_t existing = stPrefs.getUChar(key, 0);
    bool wasCompleted = (existing & 0x80) != 0;
    uint8_t bestScore = (existing >> 4) & 0x07;
    uint8_t attempts = existing & 0x0F;

    // Update
    bool nowCompleted = (score == ST_MAX_QUESTIONS);
    if (score > bestScore) bestScore = score;
    if (attempts < 15) attempts++;

    // Pack and save
    uint8_t packed = (nowCompleted || wasCompleted ? 0x80 : 0) |
                     ((bestScore & 0x07) << 4) |
                     (attempts & 0x0F);
    stPrefs.putUChar(key, packed);

    stPrefs.end();

    // Update global stats if newly completed
    if (nowCompleted && !wasCompleted && stSession.currentStory) {
        stProgress.totalStoriesCompleted++;
        stProgress.completedByDifficulty[stSession.currentStory->difficulty]++;
        if (score == ST_MAX_QUESTIONS) {
            stProgress.totalPerfectScores++;
            stProgress.perfectByDifficulty[stSession.currentStory->difficulty]++;
        }
        stSaveProgress();
    }
}

// ============================================
// Session Management
// ============================================

void stInitSession() {
    stSession.currentStory = NULL;
    stSession.storyIndex = 0;
    stSession.selectedDifficulty = STORY_EASY;
    stSession.playPhase = ST_PLAY_WAITING;
    stSession.playbackCharIndex = 0;
    stSession.playCount = 0;
    stSession.hasListenedOnce = false;
    stSession.playbackWPM = stProgress.preferredWPM;
    stSession.characterWPM = stProgress.preferredCharWPM;
    stSession.useFarnsworth = stProgress.useFarnsworth;
    stSession.toneFrequency = stProgress.preferredTone;
    stSession.currentQuestion = 0;
    stSession.correctAnswers = 0;
    stSession.stopPlayback = false;

    for (int i = 0; i < ST_MAX_QUESTIONS; i++) {
        stSession.selectedAnswers[i] = -1;
        stSession.questionAnswered[i] = false;
    }

    stSession.stateStartTime = millis();
}

void stSelectStory(const StoryData* story, int index) {
    stSession.currentStory = story;
    stSession.storyIndex = index;
    stSession.playPhase = ST_PLAY_WAITING;
    stSession.playbackCharIndex = 0;
    stSession.playCount = 0;
    stSession.hasListenedOnce = false;
    stSession.currentQuestion = 0;
    stSession.correctAnswers = 0;
    stSession.stopPlayback = false;

    for (int i = 0; i < ST_MAX_QUESTIONS; i++) {
        stSession.selectedAnswers[i] = -1;
        stSession.questionAnswered[i] = false;
    }

    Serial.printf("[StoryTime] Selected story: %s (%d words)\n",
                  story->title, story->wordCount);
}

// ============================================
// Morse Playback
// ============================================

// Forward declare keyboard read function
extern char readKeyboardNonBlocking();

// Delay while keeping LVGL responsive and checking for keyboard input
// Returns false if playback should stop (ESC/SPACE pressed or stopPlayback flag set)
bool stDelayWithUI(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        if (stSession.stopPlayback) return false;

        // Check for keyboard input during playback
        char key = readKeyboardNonBlocking();
        if (key != 0) {
            if (key == 0x1B) {  // ESC
                stSession.stopPlayback = true;
                return false;
            } else if (key == ' ') {  // SPACE - pause
                stSession.stopPlayback = true;
                return false;
            } else if (key == 'r' || key == 'R') {  // R - restart
                stSession.stopPlayback = true;
                stSession.playbackCharIndex = 0;  // Signal restart
                return false;
            }
        }

        lv_timer_handler();
        delay(5);
    }
    return true;
}

// Interruptible version of playMorseChar for Story Time
// Returns false if interrupted by keyboard (ESC, SPACE, R)
bool stPlayMorseCharInterruptible(char c, int wpm, int toneFreq) {
    const char* pattern = getMorseCode(c);
    if (pattern == nullptr) {
        return true;  // Skip unknown characters
    }

    MorseTiming timing(wpm);

    // Play each element in the pattern
    for (int i = 0; pattern[i] != '\0'; i++) {
        // Check for keyboard interrupt before each element
        char key = readKeyboardNonBlocking();
        if (key != 0) {
            if (key == 0x1B || key == ' ' || key == 'r' || key == 'R') {
                stSession.stopPlayback = true;
                if (key == 'r' || key == 'R') {
                    stSession.playbackCharIndex = 0;
                }
                requestStopTone();
                return false;
            }
        }

        if (stSession.stopPlayback) {
            requestStopTone();
            return false;
        }

        // Play dit or dah
        int duration = (pattern[i] == '.') ? timing.ditDuration : (timing.ditDuration * 3);
        playTone(toneFreq, duration);

        // Gap between elements (unless last element)
        if (pattern[i + 1] != '\0') {
            if (!stDelayWithUI(timing.elementGap)) {
                return false;
            }
        }
    }
    return true;
}

// Play the current story as morse code
// Returns true if completed, false if cancelled
bool stPlayStoryMorse() {
    if (!stSession.currentStory) return false;

    const char* text = stSession.currentStory->storyText;
    int charWPM = stSession.useFarnsworth ? stSession.characterWPM : stSession.playbackWPM;
    int effectiveWPM = stSession.playbackWPM;
    int tone = stSession.toneFrequency;

    stSession.playPhase = ST_PLAY_PLAYING;
    stSession.playCount++;

    Serial.printf("[StoryTime] Starting playback at %d/%d WPM, tone %d Hz\n",
                  effectiveWPM, charWPM, tone);

    // Calculate extra spacing for Farnsworth
    MorseTiming charTiming(charWPM);
    MorseTiming effectiveTiming(effectiveWPM);

    // Extra delay per unit for Farnsworth spacing
    int extraDelayPerUnit = 0;
    if (stSession.useFarnsworth && effectiveWPM < charWPM) {
        extraDelayPerUnit = (int)(effectiveTiming.ditDuration - charTiming.ditDuration);
    }

    int startIndex = stSession.playbackCharIndex;
    int len = strlen(text);

    for (int i = startIndex; i < len; i++) {
        // Check for stop/pause
        if (stSession.stopPlayback) {
            stSession.playbackCharIndex = i;
            stSession.playPhase = ST_PLAY_PAUSED;
            requestStopTone();  // Non-blocking via dual-core audio
            return false;
        }

        char c = toupper(text[i]);

        if (c == ' ') {
            // Word gap (7 units) with Farnsworth extra
            int wordGap = charTiming.wordGap + (extraDelayPerUnit * 7);
            if (!stDelayWithUI(wordGap)) {
                stSession.playbackCharIndex = i;
                return false;
            }
        } else {
            // Play character at character speed (interruptible)
            if (!stPlayMorseCharInterruptible(c, charWPM, tone)) {
                stSession.playbackCharIndex = i;
                return false;
            }

            // Inter-character gap (3 units) with Farnsworth extra
            if (i < len - 1 && text[i + 1] != ' ') {
                int letterGap = charTiming.letterGap + (extraDelayPerUnit * 3);
                if (!stDelayWithUI(letterGap)) {
                    stSession.playbackCharIndex = i + 1;
                    return false;
                }
            }
        }

        // Update progress periodically
        if (i % 10 == 0) {
            lv_timer_handler();
        }

        stSession.playbackCharIndex = i + 1;
    }

    stSession.playPhase = ST_PLAY_COMPLETE;
    stSession.hasListenedOnce = true;
    stSession.playbackCharIndex = 0;  // Reset for replay

    Serial.println("[StoryTime] Playback complete");
    return true;
}

void stStopPlayback() {
    stSession.stopPlayback = true;
    requestStopTone();  // Non-blocking via dual-core audio
}

void stPausePlayback() {
    stSession.stopPlayback = true;
    stSession.playPhase = ST_PLAY_PAUSED;
    requestStopTone();  // Non-blocking via dual-core audio
}

void stResumePlayback() {
    stSession.stopPlayback = false;
    // Continue from where we left off
    stPlayStoryMorse();
}

void stRestartPlayback() {
    stSession.stopPlayback = true;
    requestStopTone();  // Non-blocking via dual-core audio
    stSession.playbackCharIndex = 0;
    stSession.stopPlayback = false;
    stPlayStoryMorse();
}

// ============================================
// Quiz Functions
// ============================================

void stSubmitAnswer(int questionIndex, int selectedOption) {
    if (!stSession.currentStory || questionIndex >= ST_MAX_QUESTIONS) return;

    const StoryQuestion* q = &stSession.currentStory->questions[questionIndex];
    stSession.selectedAnswers[questionIndex] = selectedOption;
    stSession.questionAnswered[questionIndex] = true;

    bool correct = (selectedOption == q->correctIndex);
    if (correct) {
        stSession.correctAnswers++;
        beep(1000, 150);  // Success tone
        Serial.printf("[StoryTime] Q%d correct!\n", questionIndex + 1);
    } else {
        beep(300, 200);   // Error tone
        Serial.printf("[StoryTime] Q%d wrong - answered %d, correct was %d\n",
                      questionIndex + 1, selectedOption, q->correctIndex);
    }

    // Update global stats
    stProgress.totalQuestionsAttempted++;
    if (correct) stProgress.totalQuestionsCorrect++;
}

void stFinishQuiz() {
    if (!stSession.currentStory) return;

    int score = stSession.correctAnswers;
    Serial.printf("[StoryTime] Quiz complete: %d/%d\n", score, ST_MAX_QUESTIONS);

    // Save progress
    stSaveStoryProgress(stSession.currentStory->id, score);
    stSaveProgress();
}

// ============================================
// Game Start Function
// ============================================

void storyTimeStart() {
    Serial.println("[StoryTime] ========================================");
    Serial.println("[StoryTime] STARTING MORSE STORY TIME");
    Serial.println("[StoryTime] ========================================");

    // Load saved progress and settings
    stLoadProgress();

    // Initialize session
    stInitSession();

    Serial.printf("[StoryTime] Game initialized - %d WPM, %d stories total\n",
                  stProgress.preferredWPM, getStoryCount());
}

#endif // GAME_STORY_TIME_H
