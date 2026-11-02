/*
 * Web API - Screenshot Endpoint
 * Captures the screen and serves it as a BMP image
 *
 * The pixels come from LVGL, not from reading the display back: the main loop
 * forces one full redraw and lvgl_disp_flush() copies each flushed area into a
 * PSRAM buffer (lvgl_capture_buf in lv_init.h). Reading the display needs its
 * MISO pin, which some boards have cut because the display holds the SD card's
 * data line low.
 */

#ifndef WEB_API_SCREENSHOT_H
#define WEB_API_SCREENSHOT_H

#include <atomic>
#include <ESPAsyncWebServer.h>
#include <lvgl.h>
#include "../../core/config.h"
#include "../../core/hardware_init.h"

// BMP file header structure (54 bytes total)
#pragma pack(push, 1)
struct BMPHeader {
    // File header (14 bytes)
    uint16_t signature;       // 'BM' = 0x4D42
    uint32_t fileSize;        // Total file size
    uint16_t reserved1;       // 0
    uint16_t reserved2;       // 0
    uint32_t dataOffset;      // 54 (header size)

    // DIB header - BITMAPINFOHEADER (40 bytes)
    uint32_t headerSize;      // 40
    int32_t  width;           // Image width
    int32_t  height;          // Image height (positive = bottom-up)
    uint16_t planes;          // 1
    uint16_t bitsPerPixel;    // 24
    uint32_t compression;     // 0 (BI_RGB, no compression)
    uint32_t imageSize;       // Width * Height * 3
    int32_t  xPixelsPerMeter; // 0
    int32_t  yPixelsPerMeter; // 0
    uint32_t colorsUsed;      // 0
    uint32_t colorsImportant; // 0
};
#pragma pack(pop)

// Screenshot state for streaming response
struct ScreenshotState {
    uint8_t* bmpData;        // Complete BMP in PSRAM (header + BGR888 pixels)
    size_t bmpSize;          // Total size of BMP data
};

static ScreenshotState screenshotState = {nullptr, 0};

// The capture runs in the main loop (LVGL isn't safe from the web server
// task). The handler requests it and waits; the phase hands the result back,
// and lets the main loop clean up if the handler stopped waiting.
enum ScreenshotPhase { SHOT_IDLE, SHOT_REQUESTED, SHOT_CAPTURING, SHOT_READY, SHOT_FAILED, SHOT_ABANDONED };
static std::atomic<int> screenshotPhase(SHOT_IDLE);
#define SCREENSHOT_WAIT_MS 3000

// Create BMP header for given dimensions
BMPHeader createBMPHeader(int32_t width, int32_t height) {
    BMPHeader header;
    uint32_t imageSize = width * height * 3;

    // File header
    header.signature = 0x4D42;  // 'BM'
    header.fileSize = 54 + imageSize;
    header.reserved1 = 0;
    header.reserved2 = 0;
    header.dataOffset = 54;

    // DIB header
    header.headerSize = 40;
    header.width = width;
    header.height = height;  // Positive = bottom-up (standard BMP)
    header.planes = 1;
    header.bitsPerPixel = 24;
    header.compression = 0;
    header.imageSize = imageSize;
    header.xPixelsPerMeter = 0;
    header.yPixelsPerMeter = 0;
    header.colorsUsed = 0;
    header.colorsImportant = 0;

    return header;
}

void cleanupScreenshotBuffers() {
    if (screenshotState.bmpData) {
        free(screenshotState.bmpData);
        screenshotState.bmpData = nullptr;
    }
    screenshotState.bmpSize = 0;
}

// Main loop only: redraw the whole screen once, copying every flush
static bool captureScreenToBMP() {
    size_t headerSize = 54;
    size_t pixelDataSize = SCREEN_WIDTH * SCREEN_HEIGHT * 3;  // BGR888
    screenshotState.bmpSize = headerSize + pixelDataSize;

    screenshotState.bmpData = (uint8_t*)ps_malloc(screenshotState.bmpSize);
    if (!screenshotState.bmpData) {
        Serial.println("Screenshot: Failed to allocate BMP buffer in PSRAM");
        screenshotState.bmpSize = 0;
        return false;
    }

    uint16_t* screenBuffer = (uint16_t*)ps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));
    if (!screenBuffer) {
        Serial.println("Screenshot: Failed to allocate screen buffer");
        cleanupScreenshotBuffers();
        return false;
    }

    BMPHeader header = createBMPHeader(SCREEN_WIDTH, SCREEN_HEIGHT);
    memcpy(screenshotState.bmpData, &header, headerSize);

    // Finish anything pending first, then redraw every pixel into the copy
    lv_refr_now(NULL);
    lvgl_capture_buf = screenBuffer;
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    lvgl_capture_buf = NULL;

    // RGB565 (native order, LV_COLOR_16_SWAP = 0) to BGR888, bottom-up
    uint8_t* pixelPtr = screenshotState.bmpData + headerSize;
    for (int row = SCREEN_HEIGHT - 1; row >= 0; row--) {
        uint16_t* srcRow = screenBuffer + (row * SCREEN_WIDTH);
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint16_t pixel = srcRow[x];
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;
            // Replicate high bits into low bits so white is 255, not 248
            r |= (r >> 5);
            g |= (g >> 6);
            b |= (b >> 5);
            *pixelPtr++ = b;
            *pixelPtr++ = g;
            *pixelPtr++ = r;
        }
    }

    free(screenBuffer);
    return true;
}

// Called from loop(): runs a capture the web handler asked for
void processScreenshotRequest() {
    int expected = SHOT_REQUESTED;
    if (!screenshotPhase.compare_exchange_strong(expected, SHOT_CAPTURING)) return;

    bool ok = captureScreenToBMP();
    expected = SHOT_CAPTURING;
    if (!screenshotPhase.compare_exchange_strong(expected, ok ? SHOT_READY : SHOT_FAILED)) {
        // The handler gave up waiting
        cleanupScreenshotBuffers();
        screenshotPhase = SHOT_IDLE;
    }
}

// Response filler callback - streams from pre-captured buffer
size_t screenshotFiller(uint8_t* buffer, size_t maxLen, size_t index) {
    if (!screenshotState.bmpData || index >= screenshotState.bmpSize) {
        return 0;
    }

    size_t remaining = screenshotState.bmpSize - index;
    size_t toCopy = min(maxLen, remaining);
    memcpy(buffer, screenshotState.bmpData + index, toCopy);

    return toCopy;
}

// Register screenshot API endpoint
void registerScreenshotAPI(AsyncWebServer* server) {
    server->on("/api/screenshot", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Check authentication
        extern bool checkWebAuth(AsyncWebServerRequest *request);
        if (!checkWebAuth(request)) return;

        // Prevent concurrent screenshots
        int expected = SHOT_IDLE;
        if (!screenshotPhase.compare_exchange_strong(expected, SHOT_REQUESTED)) {
            request->send(503, "text/plain", "Screenshot already in progress");
            return;
        }

        // Wait for the main loop to capture (normally well under 100 ms)
        unsigned long start = millis();
        while (millis() - start < SCREENSHOT_WAIT_MS) {
            int phase = screenshotPhase.load();
            if (phase == SHOT_READY || phase == SHOT_FAILED) break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        int phase = screenshotPhase.load();
        if (phase != SHOT_READY && phase != SHOT_FAILED) {
            // Not picked up yet: cancel. Mid-capture: the main loop cleans up.
            expected = SHOT_REQUESTED;
            if (!screenshotPhase.compare_exchange_strong(expected, SHOT_IDLE)) {
                expected = SHOT_CAPTURING;
                screenshotPhase.compare_exchange_strong(expected, SHOT_ABANDONED);
            }
            phase = screenshotPhase.load();
            if (phase != SHOT_READY && phase != SHOT_FAILED) {
                request->send(503, "text/plain", "Screenshot timed out");
                return;
            }
        }
        if (phase == SHOT_FAILED) {
            screenshotPhase = SHOT_IDLE;
            request->send(500, "text/plain", "Failed to capture screenshot");
            return;
        }

        // Create streaming response from captured buffer
        AsyncWebServerResponse *response = request->beginResponse(
            "image/bmp",
            screenshotState.bmpSize,
            screenshotFiller
        );

        response->addHeader("Content-Disposition", "attachment; filename=\"vail-summit-screenshot.bmp\"");
        response->addHeader("Cache-Control", "no-store");

        // Free the buffer once the response is done
        request->onDisconnect([]() {
            cleanupScreenshotBuffers();
            screenshotPhase = SHOT_IDLE;
        });

        request->send(response);
    });
}

#endif // WEB_API_SCREENSHOT_H
