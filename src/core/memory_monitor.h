/*
 * Memory Monitor Utility
 * Provides heap monitoring and diagnostics for ESP32-S3
 */

#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <Arduino.h>
#include <esp_heap_caps.h>

// Lowest largest-internal-block seen since boot - the number that predicts
// WiFi/TLS trouble better than total free heap
static uint32_t lowestMaxInternalBlock = UINT32_MAX;

// Memory snapshot structure
struct MemorySnapshot {
    uint32_t freeHeap;
    uint32_t minFreeHeap;
    uint32_t maxAllocHeap;
    uint32_t freePsram;
    uint32_t totalPsram;
};

// Get current memory snapshot
MemorySnapshot getMemorySnapshot() {
    MemorySnapshot snap;
    snap.freeHeap = ESP.getFreeHeap();
    snap.minFreeHeap = ESP.getMinFreeHeap();
    snap.maxAllocHeap = ESP.getMaxAllocHeap();
    if (psramFound()) {
        snap.freePsram = ESP.getFreePsram();
        snap.totalPsram = ESP.getPsramSize();
    } else {
        snap.freePsram = 0;
        snap.totalPsram = 0;
    }
    return snap;
}

// Log memory status with optional tag
void logMemoryStatus(const char* tag = nullptr) {
    MemorySnapshot snap = getMemorySnapshot();

    if (tag) {
        Serial.printf("[%s] ", tag);
    }

    Serial.printf("Heap: %d free, %d min, %d max-block (lowest max-block %u)",
        snap.freeHeap, snap.minFreeHeap, snap.maxAllocHeap, (unsigned)lowestMaxInternalBlock);

    if (snap.totalPsram > 0) {
        Serial.printf(", PSRAM: %d/%d free", snap.freePsram, snap.totalPsram);
    }
    Serial.println();
}

// Check if heap is critically low
bool isHeapLow(uint32_t threshold = 20000) {
    return ESP.getFreeHeap() < threshold;
}

// Check if heap is getting fragmented (max alloc block much smaller than free heap)
bool isHeapFragmented() {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxAllocHeap();
    // If largest block is less than 50% of free heap, it's fragmented
    return (maxBlock < freeHeap / 2) && (freeHeap > 30000);
}

// Largest contiguous internal block a TLS handshake needs (mbedTLS buffers are
// internal-only on this core). Below this, HTTPS / WSS connects start failing.
#define TLS_INTERNAL_BLOCK_NEEDED 45000

// True when a TLS connection can reasonably be attempted right now
bool hasTLSHeadroom() {
    return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= TLS_INTERNAL_BLOCK_NEEDED;
}

// Periodic health check - call in main loop
void checkMemoryHealth() {
    static unsigned long lastCheck = 0;
    static unsigned long lastWarning = 0;
    unsigned long now = millis();

    // Check every 30 seconds
    if (now - lastCheck < 30000) return;
    lastCheck = now;

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (maxInternal < lowestMaxInternalBlock) lowestMaxInternalBlock = maxInternal;

    // TLS can't fit - worth knowing when WiFi features start failing
    if (maxInternal < TLS_INTERNAL_BLOCK_NEEDED && now - lastWarning > 300000) {
        Serial.printf("WARNING: largest internal block %u < %u needed for TLS (lowest since boot %u)\n",
            (unsigned)maxInternal, (unsigned)TLS_INTERNAL_BLOCK_NEEDED, (unsigned)lowestMaxInternalBlock);
        logMemoryStatus("TLS_ROOM");
        lastWarning = now;
    }

    // Log if heap is low or has dropped significantly
    if (freeHeap < 30000) {
        // Only warn every 5 minutes to avoid spam
        if (now - lastWarning > 300000) {
            Serial.println("WARNING: Low heap memory!");
            logMemoryStatus("LOW_MEM");
            lastWarning = now;
        }
    }

    // Check for fragmentation
    if (isHeapFragmented()) {
        if (now - lastWarning > 300000) {
            Serial.println("WARNING: Heap fragmentation detected!");
            logMemoryStatus("FRAG");
            lastWarning = now;
        }
    }
}

#endif // MEMORY_MONITOR_H
