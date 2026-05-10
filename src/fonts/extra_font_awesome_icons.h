/*
 * UTF-8 macros for glyphs in extra_font_awesome_icons_* (LVGL 8.3.x only; not LVGL 9).
 * Regenerate bitmaps: paste into extra_font_awesome_icons_generated.h — docs/EXTRA_FONT_AWESOME_ICONS.md
 * LVGL 8.3 font/symbol usage: https://docs.lvgl.io/8.3/overview/font.html
 *
 * Runtime: lv_label_set_text(label, FA_EXTRA_*); lv_obj_set_style_text_font(label, &ExtraFontAwesomeIcons, 0);
 * When you add/change a codepoint here, regenerate extra_font_awesome_icons_generated.h and keep macros in sync.
 */

#ifndef EXTRA_FONT_AWESOME_ICONS_H
#define EXTRA_FONT_AWESOME_ICONS_H

#include <lvgl.h>

LV_FONT_DECLARE(ExtraFontAwesomeIcons);

/* U+F0AC — FA5 "globe" (Americas-style globe in older sets) */
#define FA_EXTRA_GLOBE "\xEF\x82\xAC"

/* U+F549 — school */
#define FA_EXTRA_SCHOOL "\xEF\x95\x89"

/* U+F501 — user-graduate */
#define FA_EXTRA_USER_GRADUATE "\xEF\x94\x81"

/* U+F19D — graduation-cap */
#define FA_EXTRA_GRADUATION_CAP "\xEF\x86\x9D"

/* U+F70E — scroll */
#define FA_EXTRA_SCROLL "\xEF\x9C\x8E"

/* U+F025 — microphone */
#define FA_EXTRA_MICROPHONE "\xEF\x80\xA5"

/* U+F2A2 — assistive-listening-systems */
#define FA_EXTRA_ASSISTIVE_LISTENING "\xEF\x8A\xA2"

/* U+F249 — sticky-note */
#define FA_EXTRA_STICKY_NOTE "\xEF\x89\x89"

/* U+F518 — book-open */
#define FA_EXTRA_BOOK_OPEN "\xEF\x94\x98"

/* U+F044 — edit */
#define FA_EXTRA_EDIT "\xEF\x81\x84"

/* U+F03D — video (LICW); cmap in extra_font_awesome_icons.c */
#define FA_EXTRA_VIDEO "\xEF\x80\xBD"

/* U+F086 — comments (speech bubbles) */
#define FA_EXTRA_COMMENTS "\xEF\x82\x86"

/* U+F44B — dumbbell */
#define FA_EXTRA_DUMBBELL "\xEF\x91\x8B"

/* U+F4C4 — hands-helping */
#define FA_EXTRA_HANDS_HELPING "\xEF\x93\x84"

/* U+F11B — gamepad */
#define FA_EXTRA_GAMEPAD "\xEF\x84\x9B"

/* U+F7D9 — tools */
#define FA_EXTRA_TOOLS "\xEF\x9F\x99"

/* U+F743 — cloud-sun-rain */
#define FA_EXTRA_CLOUD_SUN_RAIN "\xEF\x9D\x83"

/* U+F5A0 — map-marked-alt (POTA active spots) */
#define FA_EXTRA_MAP_MARKED_ALT "\xEF\x96\xA0"

/* U+F1BB — tree (POTA menu) */
#define FA_EXTRA_TREE "\xEF\x86\xBB"

/* U+F4D7 — route (activate a park) */
#define FA_EXTRA_ROUTE "\xEF\x93\x97"

#endif /* EXTRA_FONT_AWESOME_ICONS_H */
