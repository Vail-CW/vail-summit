/*
 * VAIL SUMMIT - Captive Portal Detection
 *
 * Detects captive portals (hotel/coffee shop WiFi login pages) by probing
 * a known HTTP endpoint and classifying the response:
 *   - Expected 204        -> real internet access
 *   - Redirect / hijacked -> captive portal intercepting traffic
 *   - No response         -> no internet at all
 *
 * The device has no browser, so a portal cannot be completed on-device.
 * When one is detected, the WiFi screen shows the device MAC address so the
 * user can authorize it via the network's "add a device" page, or fall back
 * to a phone hotspot.
 */

#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <WiFi.h>
#include <HTTPClient.h>

// Result of a single captive portal probe
enum CaptivePortalCheckResult {
    CPORTAL_INTERNET_OK = 0,   // Probe returned expected response - no portal
    CPORTAL_PORTAL_DETECTED,   // Response was hijacked (redirect or wrong body)
    CPORTAL_OFFLINE            // No HTTP response at all - no internet, no portal
};

#define CPORTAL_PROBE_TIMEOUT 4000   // Per-probe HTTP timeout (ms)
#define CPORTAL_URL_MAX 160
#define CPORTAL_HOST_MAX 64

// Probe endpoints that return HTTP 204 with an empty body on the open internet.
// A captive portal will instead answer with a 30x redirect to its login page
// (or serve the login page inline as a 200).
static const char* CPORTAL_PROBE_URLS[] = {
    "http://connectivitycheck.gstatic.com/generate_204",
    "http://clients3.google.com/generate_204"
};
#define CPORTAL_PROBE_URL_COUNT 2

// Set true whenever any connectivity check sees portal-like interception
// (also set by internet_check.h during periodic background checks).
// Cleared when a check confirms real internet access.
static bool captivePortalSuspected = false;

// Login page URL captured from the portal's redirect (empty if unknown)
static char captivePortalURL[CPORTAL_URL_MAX] = "";
static char captivePortalHost[CPORTAL_HOST_MAX] = "";

// Forward declarations
CaptivePortalCheckResult runCaptivePortalCheck();
bool isCaptivePortalSuspected();
const char* getCaptivePortalHost();
void noteCaptivePortalRedirect(const char* location);
void clearCaptivePortalSuspicion();
void getDeviceMACString(char* buf, size_t bufSize);

// Copy the device station MAC into buf (format AA:BB:CC:DD:EE:FF)
void getDeviceMACString(char* buf, size_t bufSize) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(buf, bufSize, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool isCaptivePortalSuspected() {
    return captivePortalSuspected;
}

// Host part of the portal login URL for display ("" if unknown)
const char* getCaptivePortalHost() {
    return captivePortalHost;
}

void clearCaptivePortalSuspicion() {
    captivePortalSuspected = false;
    captivePortalURL[0] = '\0';
    captivePortalHost[0] = '\0';
}

// Record a portal redirect target and extract its host for display
void noteCaptivePortalRedirect(const char* location) {
    captivePortalSuspected = true;

    if (!location || location[0] == '\0') return;

    strncpy(captivePortalURL, location, sizeof(captivePortalURL) - 1);
    captivePortalURL[sizeof(captivePortalURL) - 1] = '\0';

    // Extract host: skip "scheme://", copy until '/', ':' or end
    const char* host = strstr(location, "://");
    host = host ? host + 3 : location;
    size_t i = 0;
    while (host[i] != '\0' && host[i] != '/' && host[i] != ':' &&
           i < sizeof(captivePortalHost) - 1) {
        captivePortalHost[i] = host[i];
        i++;
    }
    captivePortalHost[i] = '\0';

    Serial.printf("[Portal] Redirect to: %s (host: %s)\n", captivePortalURL, captivePortalHost);
}

/*
 * Probe for a captive portal (blocking, up to ~8s worst case).
 * Call from a deferred LVGL timer or other UI-safe context, never from
 * audio-critical code paths.
 */
CaptivePortalCheckResult runCaptivePortalCheck() {
    if (WiFi.status() != WL_CONNECTED) {
        return CPORTAL_OFFLINE;
    }

    for (int i = 0; i < CPORTAL_PROBE_URL_COUNT; i++) {
        HTTPClient http;
        http.setTimeout(CPORTAL_PROBE_TIMEOUT);
        http.setConnectTimeout(CPORTAL_PROBE_TIMEOUT);
        // Do not follow redirects - a redirect IS the portal signal
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

        if (!http.begin(CPORTAL_PROBE_URLS[i])) {
            continue;
        }

        const char* headerKeys[] = { "Location" };
        http.collectHeaders(headerKeys, 1);

        int code = http.GET();
        Serial.printf("[Portal] Probe %s -> HTTP %d\n", CPORTAL_PROBE_URLS[i], code);

        if (code == 204) {
            http.end();
            clearCaptivePortalSuspicion();
            return CPORTAL_INTERNET_OK;
        }

        if (code >= 300 && code < 400) {
            // Redirect to the portal login page
            String location = http.header("Location");
            http.end();
            noteCaptivePortalRedirect(location.c_str());
            return CPORTAL_PORTAL_DETECTED;
        }

        if (code > 0) {
            // Got some other response (e.g. portal serving its page as 200)
            http.end();
            captivePortalSuspected = true;
            Serial.println("[Portal] Unexpected response - portal serving content inline");
            return CPORTAL_PORTAL_DETECTED;
        }

        // code <= 0: no response from this endpoint, try the next one
        http.end();
    }

    // No endpoint answered at all - associated but no internet, no portal seen
    return CPORTAL_OFFLINE;
}

#endif // CAPTIVE_PORTAL_H
