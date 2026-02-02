/*
 * Secrets Configuration
 *
 * This file contains API keys and other secrets that should NOT be committed
 * to source control with real values.
 *
 * For local development: Use placeholder values (default) or create a local
 * secrets_local.h file (gitignored) with your own test keys.
 *
 * For production builds: GitHub Actions injects real values via build flags:
 *   --build-property "build.extra_flags=-DFIREBASE_MAILBOX_API_KEY=\"...\""
 */

#ifndef SECRETS_H
#define SECRETS_H

// ============================================
// Morse Mailbox - Firebase Configuration
// ============================================

// Firebase API Key for Morse Mailbox authentication
// This is used for Firebase Auth token exchange
#ifndef FIREBASE_MAILBOX_API_KEY
    // Placeholder - will be replaced by GitHub Actions for production builds
    #define FIREBASE_MAILBOX_API_KEY "PLACEHOLDER_MAILBOX_KEY"
#endif

// Morse Mailbox Cloud Functions base URL
#ifndef MAILBOX_FUNCTIONS_BASE_URL
    #define MAILBOX_FUNCTIONS_BASE_URL "https://us-central1-morse-mailbox.cloudfunctions.net"
#endif

// ============================================
// Vail CW School - Firebase Configuration
// ============================================

// Firebase API Key for Vail CW School authentication
#ifndef FIREBASE_CWSCHOOL_API_KEY
    // Placeholder - will be replaced by GitHub Actions for production builds
    #define FIREBASE_CWSCHOOL_API_KEY "PLACEHOLDER_CWSCHOOL_KEY"
#endif

// CW School Cloud Functions base URL
#ifndef CWSCHOOL_FUNCTIONS_BASE_URL
    #define CWSCHOOL_FUNCTIONS_BASE_URL "https://us-central1-vail-cw-school.cloudfunctions.net"
#endif

// CW School device linking URL (web page)
#ifndef CWSCHOOL_LINK_DEVICE_URL
    #define CWSCHOOL_LINK_DEVICE_URL "https://learncw.vailmorse.com/link-device"
#endif

// ============================================
// Build Information
// ============================================

// Check if we're using placeholder keys (for runtime warnings)
#if defined(FIREBASE_MAILBOX_API_KEY) && (strcmp(FIREBASE_MAILBOX_API_KEY, "PLACEHOLDER_MAILBOX_KEY") == 0)
    #define USING_PLACEHOLDER_MAILBOX_KEY 1
#else
    #define USING_PLACEHOLDER_MAILBOX_KEY 0
#endif

#if defined(FIREBASE_CWSCHOOL_API_KEY) && (strcmp(FIREBASE_CWSCHOOL_API_KEY, "PLACEHOLDER_CWSCHOOL_KEY") == 0)
    #define USING_PLACEHOLDER_CWSCHOOL_KEY 1
#else
    #define USING_PLACEHOLDER_CWSCHOOL_KEY 0
#endif

#endif // SECRETS_H
