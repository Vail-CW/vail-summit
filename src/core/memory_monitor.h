/*
 * Memory Monitor Utility
 * Provides heap monitoring and diagnostics for ESP32-S3
 */

#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <Arduino.h>

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

    Serial.printf("Heap: %d free, %d min, %d max-block",
        snap.freeHeap, snap.minFreeHeap, snap.maxAllocHeap);

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

// Periodic health check - call in main loop
void checkMemoryHealth() {
    static unsigned long lastCheck = 0;
    static unsigned long lastWarning = 0;
    unsigned long now = millis();

    // Check every 30 seconds
    if (now - lastCheck < 30000) return;
    lastCheck = now;

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minHeap = ESP.getMinFreeHeap();

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
