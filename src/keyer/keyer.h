/*
 * Unified Keyer Module
 *
 * Provides proper iambic A, iambic B, and ultimatic keying logic
 * based on the proven VAIL Adapter implementation.
 *
 * All keying modes across the Summit should use this module
 * with their own output callbacks.
 */

#ifndef KEYER_H
#define KEYER_H

#include <Arduino.h>

#define KEYER_QUEUE_SIZE 5
#define PADDLE_DIT 0
#define PADDLE_DAH 1

// Callback for tone/output control
// txOn: true = start transmitting, false = stop transmitting
// element: 0 = dit, 1 = dah (useful for some output modes)
typedef void (*KeyerTxCallback)(bool txOn, int element);

// ============================================
// QSet - FIFO Queue with Duplicate Prevention
// ============================================
// From VAIL Adapter keyers.cpp - this is critical for proper
// iambic/ultimatic behavior

class QSet {
private:
    int arr[KEYER_QUEUE_SIZE];
    unsigned int arrlen = 0;

public:
    // Dequeue first item (FIFO), returns -1 if empty
    int shift() {
        if (arrlen == 0) {
            return -1;
        }
        int ret = arr[0];
        arrlen--;
        for (unsigned int i = 0; i < arrlen; i++) {
            arr[i] = arr[i + 1];
        }
        return ret;
    }

    // Remove last item (LIFO), returns -1 if empty
    int pop() {
        if (arrlen == 0) {
            return -1;
        }
        return arr[--arrlen];
    }

    // Add item if not already present (duplicate prevention)
    void add(int val) {
        if (arrlen >= KEYER_QUEUE_SIZE) {
            return;
        }
        // Check for duplicates
        for (unsigned int i = 0; i < arrlen; i++) {
            if (arr[i] == val) {
                return;
            }
        }
        arr[arrlen++] = val;
    }

    // Clear the queue
    void clear() {
        arrlen = 0;
    }

    // Check if empty
    bool isEmpty() {
        return arrlen == 0;
    }
};

// ============================================
// StraightKeyer - Direct Passthrough
// ============================================
// No timing logic - paddle press immediately starts/stops tx

class StraightKeyer {
protected:
    bool txClosed = false;
    KeyerTxCallback txCallback = nullptr;

public:
    virtual ~StraightKeyer() {}  // Virtual destructor for polymorphism

    virtual void reset() {
        if (txClosed && txCallback) {
            txCallback(false, PADDLE_DIT);
        }
        txClosed = false;
    }

    virtual void setDitDuration(unsigned int duration) {
        // Straight key doesn't use duration
    }

    void setTxCallback(KeyerTxCallback cb) {
        txCallback = cb;
    }

    virtual void key(int paddle, bool pressed) {
        // Straight key uses DIT paddle for keying
        if (paddle == PADDLE_DIT) {
            if (pressed && !txClosed) {
                txClosed = true;
                if (txCallback) txCallback(true, PADDLE_DIT);
            } else if (!pressed && txClosed) {
                txClosed = false;
                if (txCallback) txCallback(false, PADDLE_DIT);
            }
        }
    }

    virtual void tick(unsigned long millis) {
        // Straight key has no timing logic
    }

    virtual bool isTxActive() {
        return txClosed;
    }

    virtual int getCurrentElement() {
        return txClosed ? PADDLE_DIT : -1;
    }
};

// ============================================
// ElBugKeyer - Electronic Bug (Base for Iambic)
// ============================================
// Auto-repeating dits, manual dahs
// This is the foundation for all iambic modes

class ElBugKeyer : public StraightKeyer {
protected:
    unsigned long nextPulse = 0;
    bool keyPressed[2] = {false, false};
    int nextRepeat = -1;
    int currentTransmittingElement = -1;
    unsigned int ditDuration = 100;

    // Return which key is currently pressed, -1 if none
    int whichKeyPressed() {
        for (int i = 0; i < 2; i++) {
            if (keyPressed[i]) return i;
        }
        return -1;
    }

    // Calculate element duration
    unsigned int elementDuration(int element) {
        if (element == PADDLE_DIT) return ditDuration;
        if (element == PADDLE_DAH) return ditDuration * 3;
        return ditDuration;
    }

    // Start pulsing if not already
    void beginPulsing() {
        if (nextPulse == 0) {
            nextPulse = 1;  // Will trigger on next tick
        }
    }

    // Determine what element to transmit next
    // Override this in subclasses for different behaviors
    virtual int nextTx() {
        if (whichKeyPressed() == -1) {
            return -1;
        }
        return nextRepeat;
    }

    // State machine pulse - called when nextPulse time is reached
    virtual void pulse(unsigned long millis) {
        unsigned int pulseDuration = 0;

        if (currentTransmittingElement >= 0) {
            // Currently transmitting - end element, start inter-element gap
            pulseDuration = ditDuration;  // Gap = 1 dit
            if (txCallback) txCallback(false, currentTransmittingElement);
            currentTransmittingElement = -1;
        } else {
            // Not transmitting - check what to send next
            int next = nextTx();
            if (next >= 0) {
                pulseDuration = elementDuration(next);
                currentTransmittingElement = next;
                if (txCallback) txCallback(true, next);
            }
        }

        if (pulseDuration > 0) {
            nextPulse = millis + pulseDuration;
        } else {
            nextPulse = 0;  // Stop pulsing
        }
    }

public:
    void reset() override {
        StraightKeyer::reset();
        nextPulse = 0;
        keyPressed[0] = false;
        keyPressed[1] = false;
        nextRepeat = -1;
        currentTransmittingElement = -1;
    }

    void setDitDuration(unsigned int duration) override {
        ditDuration = duration;
    }

    void key(int paddle, bool pressed) override {
        if (paddle < 0 || paddle > 1) return;

        keyPressed[paddle] = pressed;
        if (pressed) {
            nextRepeat = paddle;
            beginPulsing();
        } else {
            nextRepeat = whichKeyPressed();
        }
    }

    void tick(unsigned long millis) override {
        if (nextPulse > 0 && millis >= nextPulse) {
            pulse(millis);
        }
    }

    bool isTxActive() override {
        return currentTransmittingElement >= 0;
    }

    int getCurrentElement() override {
        return currentTransmittingElement;
    }
};

// ============================================
// IambicKeyer - Base Iambic (Mode B Squeeze)
// ============================================
// When both paddles squeezed, alternates between dit and dah

class IambicKeyer : public ElBugKeyer {
protected:
    int nextTx() override {
        int next = ElBugKeyer::nextTx();
        // When both paddles pressed, toggle for alternation
        if (keyPressed[PADDLE_DIT] && keyPressed[PADDLE_DAH]) {
            nextRepeat = 1 - nextRepeat;  // Toggle 0<->1
        }
        return next;
    }
};

// ============================================
// IambicAKeyer - Iambic Mode A (Dit Memory)
// ============================================
// Queues only dits on press - "dit insertion" behavior
// More deliberate feel, must explicitly press dit to queue it

class IambicAKeyer : public IambicKeyer {
private:
    QSet queue;

public:
    void reset() override {
        IambicKeyer::reset();
        queue.clear();
    }

    void key(int paddle, bool pressed) override {
        // Only queue dits on press (dit memory)
        if (pressed && paddle == PADDLE_DIT) {
            queue.add(paddle);
        }
        IambicKeyer::key(paddle, pressed);
    }

protected:
    int nextTx() override {
        // Get standard iambic behavior (handles alternation)
        int next = IambicKeyer::nextTx();
        // Check queue - if dit was queued, return it instead
        int queued = queue.shift();
        if (queued != -1) {
            return queued;
        }
        return next;
    }
};

// ============================================
// IambicBKeyer - Iambic Mode B (Full Memory)
// ============================================
// Queues both dits and dahs on press
// Re-adds currently pressed keys every pulse cycle
// Smooth continuous alternation on squeeze

class IambicBKeyer : public IambicKeyer {
private:
    QSet queue;

public:
    void reset() override {
        IambicKeyer::reset();
        queue.clear();
    }

    void key(int paddle, bool pressed) override {
        // Queue both dits and dahs on press
        if (pressed) {
            queue.add(paddle);
        }
        IambicKeyer::key(paddle, pressed);
    }

protected:
    int nextTx() override {
        // Re-add any currently pressed keys to queue
        // QSet prevents duplicates, so this just ensures held keys
        // continue to produce elements
        for (int k = 0; k < 2; k++) {
            if (keyPressed[k]) {
                queue.add(k);
            }
        }
        // Dequeue next element
        return queue.shift();
    }
};

// ============================================
// UltimaticKeyer - Ultimatic Mode
// ============================================
// FIFO queue preserves exact press order
// Last paddle pressed "wins" and continues

class UltimaticKeyer : public ElBugKeyer {
private:
    QSet queue;

public:
    void reset() override {
        ElBugKeyer::reset();
        queue.clear();
    }

    void key(int paddle, bool pressed) override {
        // Queue paddle on press
        if (pressed) {
            queue.add(paddle);
        }
        ElBugKeyer::key(paddle, pressed);
    }

protected:
    int nextTx() override {
        // Check queue first (FIFO)
        int queued = queue.shift();
        if (queued != -1) {
            return queued;
        }
        // Fall back to ElBugKeyer behavior
        return ElBugKeyer::nextTx();
    }
};

// ============================================
// Global Keyer Instances (Pre-allocated)
// ============================================
// Avoid heap allocation by using static instances

static StraightKeyer straightKeyerInstance;
static IambicAKeyer iambicAKeyerInstance;
static IambicBKeyer iambicBKeyerInstance;
static UltimaticKeyer ultimaticKeyerInstance;

// ============================================
// Keyer Factory Function
// ============================================
// Returns pointer to appropriate keyer based on KeyType enum
// KeyType values: 0=Straight, 1=IambicA, 2=IambicB, 3=Ultimatic

inline StraightKeyer* getKeyer(int keyType) {
    switch (keyType) {
        case 0: return &straightKeyerInstance;
        case 1: return (StraightKeyer*)&iambicAKeyerInstance;
        case 2: return (StraightKeyer*)&iambicBKeyerInstance;
        case 3: return (StraightKeyer*)&ultimaticKeyerInstance;
        default: return &straightKeyerInstance;
    }
}

#endif // KEYER_H
