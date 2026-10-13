/*
 * Lazy mode-scoped allocation helpers
 *
 * Large tables that only one mode uses are allocated when that mode is
 * entered and freed in its cleanupTable entry, instead of living in .bss
 * for the whole session. PSRAM is used when fitted; otherwise the internal
 * heap, which still beats a permanent .bss reservation on non-PSRAM boards
 * because the memory is returned when the mode exits.
 */

#ifndef LAZY_ALLOC_H
#define LAZY_ALLOC_H

#include <Arduino.h>
#include <esp_heap_caps.h>

// Zeroed block, PSRAM first when available, internal heap otherwise.
static inline void* lazyAlloc(size_t bytes) {
    void* p = nullptr;
    if (psramFound()) p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!p) p = malloc(bytes);
    if (p) memset(p, 0, bytes);
    return p;
}

// free() is heap_caps-aware on ESP32 and handles both regions.
template <typename T>
static inline void lazyFree(T*& p) {
    if (p) {
        free((void*)p);
        p = nullptr;
    }
}

#endif // LAZY_ALLOC_H
