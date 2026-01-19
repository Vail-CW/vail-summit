/*
 * Web API - Screenshot Endpoint
 * Captures the display and serves as BMP image
 *
 * Uses LVGL canvas to render a full-screen snapshot
 */

#ifndef WEB_API_SCREENSHOT_H
#define WEB_API_SCREENSHOT_H

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
    bool active;
    uint8_t* bmpData;        // Complete BMP in PSRAM (header + BGR888 pixels)
    size_t bmpSize;          // Total size of BMP data
};

static ScreenshotState screenshotState = {false, nullptr, 0};

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

// Capture screen by reading from display with improved timing
bool captureScreenToBMP() {
    extern LGFX tft;

    // Calculate sizes
    size_t headerSize = 54;
    size_t pixelDataSize = SCREEN_WIDTH * SCREEN_HEIGHT * 3;  // BGR888
    screenshotState.bmpSize = headerSize + pixelDataSize;

    // Allocate BMP buffer in PSRAM
    screenshotState.bmpData = (uint8_t*)ps_malloc(screenshotState.bmpSize);
    if (!screenshotState.bmpData) {
        Serial.println("Screenshot: Failed to allocate BMP buffer in PSRAM");
        return false;
    }

    Serial.printf("Screenshot: Allocated %d bytes for BMP\n", screenshotState.bmpSize);

    // Write BMP header
    BMPHeader header = createBMPHeader(SCREEN_WIDTH, SCREEN_HEIGHT);
    memcpy(screenshotState.bmpData, &header, headerSize);

    // Allocate buffer for entire screen in RGB565 (307KB in PSRAM)
    size_t screenBufferSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);
    uint16_t* screenBuffer = (uint16_t*)ps_malloc(screenBufferSize);
    if (!screenBuffer) {
        Serial.println("Screenshot: Failed to allocate screen buffer");
        free(screenshotState.bmpData);
        screenshotState.bmpData = nullptr;
        return false;
    }

    Serial.printf("Screenshot: Allocated %d bytes for screen buffer\n", screenBufferSize);

    // Wait for any pending LVGL updates to complete
    lv_refr_now(NULL);
    delay(50);  // Give time for display writes to finish

    // Read entire screen at once using startWrite/endWrite for consistent timing
    Serial.println("Screenshot: Reading display...");
    tft.startWrite();
    tft.readRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenBuffer);
    tft.endWrite();

    Serial.println("Screenshot: Converting to BMP...");

    // Convert to BGR888 (BMP format, bottom-up)
    uint8_t* pixelPtr = screenshotState.bmpData + headerSize;

    for (int row = SCREEN_HEIGHT - 1; row >= 0; row--) {
        uint16_t* srcRow = screenBuffer + (row * SCREEN_WIDTH);

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint16_t pixel = srcRow[x];

            // The display returns data in big-endian format, swap bytes
            pixel = (pixel >> 8) | (pixel << 8);

            // Extract RGB565 components and expand to 8 bits
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;

            // Improve color accuracy by replicating high bits into low bits
            r |= (r >> 5);
            g |= (g >> 6);
            b |= (b >> 5);

            // BMP stores as BGR
            *pixelPtr++ = b;
            *pixelPtr++ = g;
            *pixelPtr++ = r;
        }

        // Yield every 64 rows to prevent watchdog
        if ((row & 0x3F) == 0) {
            yield();
        }
    }

    // Free screen buffer
    free(screenBuffer);

    Serial.println("Screenshot: Capture complete");
    return true;
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

// Cleanup function called when response completes
void cleanupScreenshotBuffers() {
    if (screenshotState.bmpData) {
        free(screenshotState.bmpData);
        screenshotState.bmpData = nullptr;
    }
    screenshotState.bmpSize = 0;
    screenshotState.active = false;
    Serial.println("Screenshot: Buffers freed");
}

// Register screenshot API endpoint
void registerScreenshotAPI(AsyncWebServer* server) {
    server->on("/api/screenshot", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Check authentication
        extern bool checkWebAuth(AsyncWebServerRequest *request);
        if (!checkWebAuth(request)) return;

        // Prevent concurrent screenshots
        if (screenshotState.active) {
            request->send(503, "text/plain", "Screenshot already in progress");
            return;
        }

        screenshotState.active = true;

        // Capture screen
        if (!captureScreenToBMP()) {
            screenshotState.active = false;
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

        // Set cleanup callback for when response completes
        request->onDisconnect([]() {
            cleanupScreenshotBuffers();
        });

        request->send(response);
    });
}

#endif // WEB_API_SCREENSHOT_H
