# Regenerating `ExtraFontAwesomeIcons` (Font Awesome → LVGL 8.3)

This project embeds a subset of **Font Awesome** icons as an LVGL **bitmap font**. Source layout:

| File | Role |
|------|------|
| **`extra_font_awesome_icons_shell.h`** | **Stable** — Arduino/LVGL includes and `EXTRAFONTAWESOMEICONS`. **Do not replace** from the font converter. |
| **`extra_font_awesome_icons_generated.h`** | **Replaceable** — paste only the **font body** from [LVGL Font Converter](https://lvgl.io/tools/fontconverter) or `lv_font_conv` here (see [what to strip](#what-to-paste-into-extra_font_awesome_icons_generatedh) below). Must live **next to** `extra_font_awesome_icons.h`. Uses **`.h`** so Arduino’s sketch copy step picks it up (`.inc` was often missing in the build temp tree). |
| **`extra_font_awesome_icons.h`** | UTF-8 `FA_EXTRA_*` macros + `LV_FONT_DECLARE`, plus the include of `shell.h` and `generated.h` that instantiates the bitmap data. The whole font lives inside the `EXTRA_FONT_AWESOME_ICONS_H` include guard so the data is defined exactly once per translation unit (the project uses a single-TU header-only build). |

**Include rule:** `extra_font_awesome_icons.h` may be included transitively by any header that uses an `FA_EXTRA_*` macro or refers to `&ExtraFontAwesomeIcons`. The standard include guard prevents duplicate instantiation. **Do NOT add a `.c` file** alongside these headers — the project's single-TU build expects everything in headers.

**Canonical instructions** for regen live in this Markdown file.

---

## What “success” looks like

- Icons are **PUA** codepoints (e.g. `U+F0AC`), not ASCII.
- The generated font must **map those codepoints** in its cmap (typically `SPARSE_*` with `range_start` in the `0xF0xx` region), **not** only `'0'`–`'9'` or random ASCII letters.
- In firmware: **UTF-8** string on the label + **`lv_obj_set_style_text_font(..., &ExtraFontAwesomeIcons, 0)`** (LVGL 8). See [LVGL 8.3 — Fonts / add symbols](https://docs.lvgl.io/8.3/overview/font.html).
- UTF-8 `#define`s for each glyph live in **`src/fonts/extra_font_awesome_icons.h`** (`FA_EXTRA_*`). After adding a codepoint, add or update the macro there and use it in menus.

---

## LVGL / toolchain version

- Firmware uses **LVGL 8.3.x** (e.g. **8.3.11**), **not** LVGL 9. Font structs and docs differ; follow **8.3** only.
- `lv_conf.h`: **`LV_USE_FONT_COMPRESSED`** must match how you convert (this font uses **`--no-compress`** → compression **off** in config).

---

## Source font file

Use the same class of asset the LVGL docs use: **Font Awesome 5** combined WOFF, e.g. from the converter site / LVGL assets:

- Example name: **`FontAwesome5-Solid+Brands+Regular.woff`**
- Pick icons on [Font Awesome](https://fontawesome.com), note each icon’s **Unicode** (e.g. `0xf086`).

---

## Critical: how to invoke `lv_font_conv` (PUA icons)

### Use **one `-r` range per icon** (recommended, known-good here)

With **`lv_font_conv` 1.5.x** (npm package `lv_font_conv`), a **comma-separated `--symbols` list** of hex PUA values has produced **wrong output** (e.g. ASCII digit cmap only, or mismatched glyphs). The reliable approach for this project is **one explicit range per codepoint**:

```text
lv_font_conv ^
  --font fa-solid-900.ttf ^
  -r 0xf0ac-0xf0ac -r 0xf549-0xf549 -r 0xf501-0xf501 -r 0xf19d-0xf19d -r 0xf70e-0xf70e ^
  -r 0xf025-0xf025 -r 0xf2a2-0xf2a2 -r 0xf249-0xf249 -r 0xf518-0xf518 -r 0xf044-0xf044 ^
  -r 0xf03d-0xf03d -r 0xf086-0xf086 -r 0xf44b-0xf44b -r 0xf4c4-0xf4c4 -r 0xf11b-0xf11b ^
  -r 0xf7d9-0xf7d9 -r 0xf743-0xf743 -r 0xf5a0-0xf5a0 -r 0xf1bb-0xf1bb -r 0xf4d7-0xf4d7 ^
  --size 24 --bpp 1 --no-compress --format lvgl ^
  -o fa_conv_out.c
```

Use **Font Awesome Free 5.15.4** `webfonts/fa-solid-900.ttf` (e.g. from the npm package or jsDelivr). **lv_font_conv 1.5.x** does not support `--lv-font-name`; name the output font in the middle of the file (or find/replace the generated symbol) to **`ExtraFontAwesomeIcons`**, and strip the outer `#include` / `#if` / `#endif` per [what to paste](#what-to-paste-into-extra_font_awesome_icons_generatedh). The project’s checked-in font was built with a single **solid** TTF; U+F0AC (globe) is present there as the solid style glyph.

Then copy the **middle** of `fa_conv_out.c` into **`extra_font_awesome_icons_generated.h`** (see [what to strip](#what-to-paste-into-extra_font_awesome_icons_generatedh)). You can use a temp file name instead of `fa_conv_out.c`.

(On Unix shells, remove `^` and use `\` line continuation or one long line.)

- **`-r 0xf0ac-0xf0ac`**: include exactly one codepoint; repeat for each icon.
- **`--size` / `--bpp` / `--no-compress`**: must stay aligned with project needs and `lv_conf.h`.
- **`--lv-font-name ExtraFontAwesomeIcons`**: output symbol name must match **`LV_FONT_DECLARE(ExtraFontAwesomeIcons)`** and **`MENU_ITEM_FA`** usage.

### Online converter (https://lvgl.io/tools/fontconverter)

You *can* use it, but:

- Prefer **Range** like `0xf086-0xf086` (or multiple ranges if the UI allows), **not** a fragile “symbols string” that gets parsed oddly.
- If the downloaded `.c` only cmap-maps **ASCII**, the FA labels will show **boxes**—discard that output and fix inputs or use **offline `lv_font_conv`** with **`-r`** as above.

---

## What to paste into extra_font_awesome_icons_generated.h

`extra_font_awesome_icons_shell.h` provides `lvgl.h` and the `EXTRAFONTAWESOMEICONS` default. **`extra_font_awesome_icons.c`** wraps the fragment with **`#if EXTRAFONTAWESOMEICONS` … `#endif`**. Your paste must be **only the font implementation** — the same block the converter would put **inside** its `#if EXTRAFONTAWESOMEICONS`.

**Remove from the top of the converter output before pasting:**

- The generator comment block (optional; you may keep it for traceability).
- `#ifdef LV_LVGL_H_INCLUDE_SIMPLE` … `#include "lvgl.h"` … (duplicate of the shell).
- `#ifndef EXTRAFONTAWESOMEICONS` / `#define EXTRAFONTAWESOMEICONS 1` / `#if EXTRAFONTAWESOMEICONS` — the shell already opened `#if EXTRAFONTAWESOMEICONS`.

**Remove from the bottom after pasting:**

- The final **`#endif`** that matches the converter’s **`#if EXTRAFONTAWESOMEICONS`** (the **`.c` file** supplies the closing `#endif`, not the fragment).

**Keep in the paste:**

- From the **`BITMAPS`** section (or the first `static` glyph array) through the end of **`const lv_font_t ExtraFontAwesomeIcons = { ... };`** (including `cache` / `font_dsc` if the generator emitted them inside that block).

**Requirements:**

- **`--lv-font-name ExtraFontAwesomeIcons`** so the public symbol stays **`ExtraFontAwesomeIcons`** (matches `LV_FONT_DECLARE` and menus).
- LVGL **8.3**-compatible `lv_font_t` fields only (e.g. **`.fallback`** from 8.2+). Do not paste LVGL 9-only members.

### Update the header file

Edit **`src/fonts/extra_font_awesome_icons.h`** whenever codepoints change:

- For **each** icon: a **`#define FA_EXTRA_…`** with the correct **UTF-8** bytes ([LVGL 8.3 — add new symbols](https://docs.lvgl.io/8.3/overview/font.html#add-new-symbols)).
- **`LV_FONT_DECLARE(ExtraFontAwesomeIcons);`** stays as-is.

### Use in UI

- **`MENU_ITEM_FA(FA_EXTRA_…, "Title", MODE_…)`** in `lv_menu_screens.h` (or set **`icon_font`** to **`&ExtraFontAwesomeIcons`** and **`lv_obj_set_style_text_font`** on the icon label—same idea).

---

## Checklist before committing

- [ ] Open **`extra_font_awesome_icons_generated.h`**: bitmap comments or cmap show **U+F0…** (or correct PUA), **not** only `U+0030`–`U+0039` as the only mapped range for menu icons.
- [ ] **`lv_font_conv`** command line in git / notes uses **`-r`** per icon (or equivalent proven Range), not a broken **`--symbols`** comma list for PUA.
- [ ] **`extra_font_awesome_icons.h`** macros match every exported codepoint you use in C.
- [ ] Build with **LVGL 8.3.x**; `LV_USE_FONT_COMPRESSED` matches **`--no-compress`**.
- [ ] **`extra_font_awesome_icons_shell.h`** / **`extra_font_awesome_icons.c`** were not overwritten by a full converter dump, and **`extra_font_awesome_icons_generated.h`** exists beside the `.c` (otherwise you get “No such file or directory”).

---

## Reference links

- [LVGL Font Converter (online)](https://lvgl.io/tools/fontconverter)
- [LVGL 8.3 — Fonts](https://docs.lvgl.io/8.3/overview/font.html)
- [lv_font_conv (offline, GitHub)](https://github.com/lvgl/lv_font_conv)
