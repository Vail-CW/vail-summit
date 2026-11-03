/*
 * Morse Text Player - non-blocking playback of long text
 *
 * Plays text of any length as morse without blocking the main loop. The
 * element/letter/word timing and the tone itself run on the Core 0 audio task
 * (requestPlayMorseStringFarnsworth); this module only feeds it the text in
 * word-aligned chunks that fit MORSE_PLAYBACK_MAX_LENGTH, waits a word gap
 * between chunks, and tracks the position for pause/resume.
 *
 * A small LVGL timer ticks the player while it is active and deletes itself
 * when playback finishes, pauses or stops, so nothing is left running once a
 * screen is gone. The text pointer is not copied and must outlive playback
 * (story/challenge data and static game buffers all do).
 *
 * Used by Story Time, Spark Watch and Memory Chain.
 */

#ifndef MORSE_TEXT_PLAYER_H
#define MORSE_TEXT_PLAYER_H

#include <lvgl.h>
#include "../core/morse_code.h"
#include "../core/task_manager.h"

#define MTP_CHUNK_MAX   (MORSE_PLAYBACK_MAX_LENGTH - 1)
#define MTP_TICK_MS     10

enum MorseTextPlayerState {
    MTP_IDLE = 0,     // Never started or stopped
    MTP_PLAYING,      // A chunk is playing on the audio task
    MTP_CHUNK_GAP,    // Waiting the gap between two chunks
    MTP_PAUSED,       // Paused, resumeIndex holds the restart point
    MTP_DONE          // Played to the end
};

struct MorseTextPlayer {
    const char* text;
    int length;
    int chunkStart;          // Text offset of the chunk on the audio task
    int chunkLen;
    bool chunkEndsAtSpace;   // Next chunk starts a new word (word gap, not letter gap)
    int resumeIndex;         // Where resume picks up after a pause
    int charWPM;             // Element speed
    int effWPM;              // Spacing speed (Farnsworth when lower than charWPM)
    int toneHz;
    MorseTextPlayerState state;
    unsigned long gapEnd;
    lv_timer_t* timer;
    void (*onDone)();        // Called once when the text has been played to the end
};

static void mtpTimerCb(lv_timer_t* t);

static void mtpDeleteTimer(MorseTextPlayer* p) {
    if (p->timer != NULL) {
        lv_timer_del(p->timer);
        p->timer = NULL;
    }
}

static void mtpEnsureTimer(MorseTextPlayer* p) {
    if (p->timer == NULL) {
        p->timer = lv_timer_create(mtpTimerCb, MTP_TICK_MS, p);
    }
}

static void mtpFinish(MorseTextPlayer* p) {
    p->state = MTP_DONE;
    p->chunkLen = 0;
    p->resumeIndex = 0;
    mtpDeleteTimer(p);
    if (p->onDone) p->onDone();
}

// Hand the next word-aligned chunk (starting at chunkStart) to the audio task
static void mtpSendChunk(MorseTextPlayer* p) {
    while (p->chunkStart < p->length && p->text[p->chunkStart] == ' ') p->chunkStart++;
    if (p->chunkStart >= p->length) {
        mtpFinish(p);
        return;
    }

    int n = p->length - p->chunkStart;
    if (n > MTP_CHUNK_MAX) {
        // Break at the last space inside the window so words stay whole
        int cut = MTP_CHUNK_MAX;
        while (cut > 0 && p->text[p->chunkStart + cut] != ' ') cut--;
        n = (cut > 0) ? cut : MTP_CHUNK_MAX;
    }
    // A trailing space would make the audio task add its own word gap
    while (n > 0 && p->text[p->chunkStart + n - 1] == ' ') n--;

    p->chunkLen = n;
    int next = p->chunkStart + n;
    p->chunkEndsAtSpace = (next < p->length && p->text[next] == ' ');

    char buf[MORSE_PLAYBACK_MAX_LENGTH];
    memcpy(buf, p->text + p->chunkStart, n);
    buf[n] = '\0';

    p->state = MTP_PLAYING;
    mtpEnsureTimer(p);
    requestPlayMorseStringFarnsworth(buf, p->charWPM, p->effWPM, p->toneHz);
}

// Current text position (character index) of the playback
static int mtpPosition(const MorseTextPlayer* p) {
    switch (p->state) {
        case MTP_PLAYING: {
            int idx = morsePlayback.charIndex;
            if (idx < 0) idx = 0;
            if (idx > p->chunkLen) idx = p->chunkLen;
            return p->chunkStart + idx;
        }
        case MTP_CHUNK_GAP:
            return p->chunkStart;
        case MTP_PAUSED:
            return p->resumeIndex;
        default:
            return 0;
    }
}

static bool mtpIsActive(const MorseTextPlayer* p) {
    return p->state == MTP_PLAYING || p->state == MTP_CHUNK_GAP;
}

// Advance the player. Normally driven by its own timer.
static void mtpUpdate(MorseTextPlayer* p) {
    unsigned long now = millis();

    if (p->state == MTP_PLAYING) {
        if (isMorsePlaybackActive()) return;
        int next = p->chunkStart + p->chunkLen;
        if (next >= p->length) {
            mtpFinish(p);
            return;
        }
        // The audio task ends a chunk right after its last element, so the
        // gap that would have followed is timed here
        int spaceUnit = 1200 / (p->effWPM > 0 ? p->effWPM : 1);
        p->gapEnd = now + (p->chunkEndsAtSpace ? spaceUnit * 7 : spaceUnit * 3);
        p->chunkStart = next;
        p->state = MTP_CHUNK_GAP;
    } else if (p->state == MTP_CHUNK_GAP) {
        if ((long)(now - p->gapEnd) >= 0) {
            mtpSendChunk(p);
        }
    } else {
        // Nothing to tick
        mtpDeleteTimer(p);
    }
}

static void mtpTimerCb(lv_timer_t* t) {
    MorseTextPlayer* p = (MorseTextPlayer*)t->user_data;
    if (p == NULL) return;
    mtpUpdate(p);
}

// Start playing text from startIndex (0 = beginning)
static void mtpBegin(MorseTextPlayer* p, const char* text, int startIndex,
                     int charWPM, int effWPM, int toneHz) {
    if (mtpIsActive(p)) cancelMorsePlayback();

    p->text = text;
    p->length = text ? (int)strlen(text) : 0;
    if (charWPM < 1) charWPM = 1;
    if (effWPM < 1 || effWPM > charWPM) effWPM = charWPM;
    p->charWPM = charWPM;
    p->effWPM = effWPM;
    p->toneHz = toneHz;
    if (startIndex < 0 || startIndex >= p->length) startIndex = 0;
    p->chunkStart = startIndex;
    p->chunkLen = 0;
    p->resumeIndex = 0;

    if (p->length == 0) {
        mtpFinish(p);
        return;
    }
    mtpSendChunk(p);
}

// Pause; resume restarts at the beginning of the word that was playing
static void mtpPause(MorseTextPlayer* p) {
    if (!mtpIsActive(p)) return;
    int pos = mtpPosition(p);
    if (p->state == MTP_PLAYING) cancelMorsePlayback();
    while (pos > 0 && pos < p->length && p->text[pos - 1] != ' ') pos--;
    if (pos >= p->length) pos = 0;
    p->resumeIndex = pos;
    p->state = MTP_PAUSED;
    mtpDeleteTimer(p);
}

static void mtpResume(MorseTextPlayer* p) {
    if (p->state != MTP_PAUSED) return;
    p->chunkStart = p->resumeIndex;
    mtpSendChunk(p);
}

// Stop and silence. Safe to call at any time, including when idle.
static void mtpStop(MorseTextPlayer* p) {
    if (p->state == MTP_PLAYING) cancelMorsePlayback();
    p->state = MTP_IDLE;
    p->chunkLen = 0;
    p->resumeIndex = 0;
    mtpDeleteTimer(p);
}

#endif // MORSE_TEXT_PLAYER_H
