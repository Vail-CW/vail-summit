/*
 * Onboarding completion tracking (first-run device setup wizard)
 */

#ifndef SETTINGS_ONBOARDING_H
#define SETTINGS_ONBOARDING_H

#include <Preferences.h>

static const char* ONBOARD_PREFS_NS = "onboard";
static const char* ONBOARD_KEY_COMPLETE = "complete";

inline bool isOnboardingComplete() {
    Preferences prefs;
    prefs.begin(ONBOARD_PREFS_NS, true);
    bool complete = prefs.getBool(ONBOARD_KEY_COMPLETE, false);
    prefs.end();
    return complete;
}

inline void markOnboardingComplete() {
    Preferences prefs;
    prefs.begin(ONBOARD_PREFS_NS, false);
    prefs.putBool(ONBOARD_KEY_COMPLETE, true);
    prefs.end();
    Serial.println("[Onboard] Marked complete");
}

inline void markOnboardingSkipped() {
    markOnboardingComplete();
}

#endif // SETTINGS_ONBOARDING_H
