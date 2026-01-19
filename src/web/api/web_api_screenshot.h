/*
 * Web API - Screenshot Endpoint
 * Captures the display and serves as BMP image
 */

#ifndef WEB_API_SCREENSHOT_H
#define WEB_API_SCREENSHOT_H

#include <ESPAsyncWebServer.h>
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
    uint16_t* readBuffer;    // Buffer for one row of RGB565 pixels
    uint8_t* rowBuffer;      // Buffer for one row of BGR888 pixels
    BMPHeader header;        // Pre-computed header
};

static ScreenshotState screenshotState = {false, nullptr, nullptr, {}};

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

// Response filler callback - streams BMP data row by row
size_t screenshotFiller(uint8_t* buffer, size_t maxLen, size_t index) {
    extern LGFX tft;

    // First 54 bytes: BMP header
    if (index < 54) {
        size_t headerBytes = min(maxLen, (size_t)(54 - index));
        memcpy(buffer, ((uint8_t*)&screenshotState.header) + index, headerBytes);
        return headerBytes;
    }

    // Pixel data section
    size_t pixelOffset = index - 54;
    size_t rowBytes = SCREEN_WIDTH * 3;  // BGR888, 3 bytes per pixel

    // Calculate which row we're on (BMP is bottom-up, so we read from bottom)
    int row = SCREEN_HEIGHT - 1 - (pixelOffset / rowBytes);
    size_t colOffset = pixelOffset % rowBytes;

    // Check if we've finished all rows
    if (row < 0) {
        return 0;
    }

    // If starting a new row, read from display and convert
    if (colOffset == 0) {
        // Read one row of RGB565 pixels from display
        tft.readRect(0, row, SCREEN_WIDTH, 1, screenshotState.readBuffer);

        // Convert RGB565 to BGR888 (BMP uses BGR order)
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            uint16_t pixel = screenshotState.readBuffer[x];
            // RGB565: RRRRRGGGGGGBBBBB
            screenshotState.rowBuffer[x * 3 + 0] = (pixel & 0x1F) << 3;          // B: 5 bits -> 8 bits
            screenshotState.rowBuffer[x * 3 + 1] = ((pixel >> 5) & 0x3F) << 2;   // G: 6 bits -> 8 bits
            screenshotState.rowBuffer[x * 3 + 2] = ((pixel >> 11) & 0x1F) << 3;  // R: 5 bits -> 8 bits
        }
    }

    // Copy as much row data as fits in the buffer
    size_t remaining = rowBytes - colOffset;
    size_t toCopy = min(maxLen, remaining);
    memcpy(buffer, screenshotState.rowBuffer + colOffset, toCopy);

    return toCopy;
}

// Cleanup function called when response completes
void cleanupScreenshotBuffers() {
    if (screenshotState.readBuffer) {
        free(screenshotState.readBuffer);
        screenshotState.readBuffer = nullptr;
    }
    if (screenshotState.rowBuffer) {
        free(screenshotState.rowBuffer);
        screenshotState.rowBuffer = nullptr;
    }
    screenshotState.active = false;
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

        // Allocate buffers in PSRAM if available
        screenshotState.readBuffer = (uint16_t*)ps_malloc(SCREEN_WIDTH * sizeof(uint16_t));
        if (!screenshotState.readBuffer) {
            // Fallback to regular malloc
            screenshotState.readBuffer = (uint16_t*)malloc(SCREEN_WIDTH * sizeof(uint16_t));
        }

        screenshotState.rowBuffer = (uint8_t*)ps_malloc(SCREEN_WIDTH * 3);
        if (!screenshotState.rowBuffer) {
            // Fallback to regular malloc
            screenshotState.rowBuffer = (uint8_t*)malloc(SCREEN_WIDTH * 3);
        }

        // Check allocation success
        if (!screenshotState.readBuffer || !screenshotState.rowBuffer) {
            cleanupScreenshotBuffers();
            request->send(500, "text/plain", "Memory allocation failed");
            return;
        }

        // Pre-compute BMP header
        screenshotState.header = createBMPHeader(SCREEN_WIDTH, SCREEN_HEIGHT);
        screenshotState.active = true;

        // Calculate total response size
        size_t totalSize = 54 + (SCREEN_WIDTH * SCREEN_HEIGHT * 3);

        // Create streaming response
        AsyncWebServerResponse *response = request->beginResponse(
            "image/bmp",
            totalSize,
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
