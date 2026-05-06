/**
 * Stable shell for ExtraFontAwesomeIcons (LVGL 8.3.x).
 *
 * Do NOT replace this file from the LVGL / lvgl.io font converter.
 * After regenerating, paste only into: extra_font_awesome_icons_generated.h
 *
 * Full workflow (what to strip from converter output, -r vs --symbols, etc.):
 *   docs/EXTRA_FONT_AWESOME_ICONS.md
 *
 * UTF-8 string macros for labels: extra_font_awesome_icons.h
 */

#pragma once

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef EXTRAFONTAWESOMEICONS
#define EXTRAFONTAWESOMEICONS 1
#endif

/* Fragment extra_font_awesome_icons_generated.h is #included from extra_font_awesome_icons.c
 * (same directory) so Arduino / esp32 quote-include resolves reliably. */
