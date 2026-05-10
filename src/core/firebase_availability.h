/*
 * Fork-friendly Firebase integration toggles.
 *
 * Upstream builds inject real Firebase Web API keys (secrets_local.h or CI -D flags).
 * Open/fork builds use placeholders from secrets.h only — cloud auth flows would fail,
 * so menus and link routes consult these helpers to avoid dead-end UX.
 */

#ifndef FIREBASE_AVAILABILITY_H
#define FIREBASE_AVAILABILITY_H

#include "secrets.h"
#include <string.h>

static inline bool morseMailboxFirebaseConfigured() {
    return strcmp(FIREBASE_MAILBOX_API_KEY, "PLACEHOLDER_MAILBOX_KEY") != 0;
}

static inline bool cwschoolFirebaseConfigured() {
    return strcmp(FIREBASE_CWSCHOOL_API_KEY, "PLACEHOLDER_CWSCHOOL_KEY") != 0;
}

#endif
