/*
 * PSRAM-first ArduinoJson allocator
 *
 * The ESP32 core routes malloc() calls under 4KB to internal RAM, and
 * ArduinoJson 7 grows documents in ~1KB pools, so by default every
 * JsonDocument lives in internal RAM. Large or long-lived parses fragment
 * internal heap, which later starves TLS (needs a big contiguous block).
 *
 * Usage:
 *   JsonDocument doc(psramJsonAllocator());
 *
 * Falls back to regular malloc when PSRAM is unavailable (no-PSRAM builds).
 * Do not use for documents touched from audio/IRAM paths.
 */

#ifndef PSRAM_JSON_H
#define PSRAM_JSON_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

class PsramJsonAllocator : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t size) override {
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!p) p = malloc(size);
    return p;
  }

  void deallocate(void* ptr) override {
    free(ptr);  // heap_caps-aware free handles both PSRAM and internal
  }

  void* reallocate(void* ptr, size_t new_size) override {
    // heap_caps_realloc(ptr, 0) frees ptr and returns NULL - handle it here
    // so the fallback below never touches a freed pointer
    if (new_size == 0) {
      free(ptr);
      return nullptr;
    }
    void* p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    if (!p) p = realloc(ptr, new_size);
    return p;
  }
};

// Single shared allocator instance (stateless, safe to share)
inline ArduinoJson::Allocator* psramJsonAllocator() {
  static PsramJsonAllocator instance;
  return &instance;
}

#endif // PSRAM_JSON_H
