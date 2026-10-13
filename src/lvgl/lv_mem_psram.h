/*
 * LVGL heap wrappers: PSRAM first, internal heap as fallback.
 *
 * Selected by LVGL_MEM_IN_PSRAM in lv_conf.h (default 1). On boards without
 * PSRAM every call falls straight through to the internal heap, so this is
 * a no-op there. The LVGL draw buffer is allocated elsewhere (lv_init.h) and
 * deliberately stays internal.
 */

#ifndef LV_MEM_PSRAM_H
#define LV_MEM_PSRAM_H

#include <Arduino.h>
#include <esp_heap_caps.h>

extern "C" {

void* lv_psram_malloc(size_t size) {
    void* p = psramFound() ? heap_caps_malloc(size, MALLOC_CAP_SPIRAM) : nullptr;
    if (!p) p = malloc(size);
    return p;
}

void lv_psram_free(void* p) {
    free(p);  // heap_caps-aware: handles both regions
}

void* lv_psram_realloc(void* p, size_t size) {
    if (size == 0) {
        free(p);
        return nullptr;
    }
    void* n = psramFound() ? heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM) : nullptr;
    if (!n) n = realloc(p, size);
    return n;
}

}  // extern "C"

#endif // LV_MEM_PSRAM_H
