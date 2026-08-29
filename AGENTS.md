# AGENTS.md

## Do NOT compile or upload

The user builds and flashes on a faster remote server. Make code changes, commit, and push only. Never run `esphome compile` or `esphome upload`.

## Architecture

ESPHome project for a standalone Jewish calendar display on a Sunton ESP32-3248S035R ("Yellow CYD" 3.5", 320x480 ST7796, no PSRAM).

```
StandaloneShabbosClock.yaml   — Main config: globals, scripts, fonts, image, API, interval
board_cyd35.yaml              — Board pins, SPI bus (included as a package)
display_cyd35.yaml            — Display + touchscreen config, render lambda
include/shabbos_clock_helpers.h — All logic: URL builders, JSON parsing, data extraction, render functions
background.jpg                — 480x320 Kotel image (RGB565)
```

## Data flow (refactored)

Data fetching and rendering are decoupled:
1. `interval: 2h` triggers `get_jdate` script
2. `get_jdate` fetches HebCal API (60-day window), parses JSON, calls `extract_display_data()` to populate `g_*` globals, then triggers `get_zmanim`
3. `get_zmanim` fetches zmanim API, re-runs `extract_display_data()` to merge zmanim, sets `data_ready = true`, calls `display.update()`
4. Display lambda reads from `g_*` globals — **no JSON parsing during render**. Shows "Loading..." until `data_ready` is true.
5. Touch/swipe events call `display.update()` directly (no fetch)

Do NOT add `update_interval` to the display — it causes periodic screen blanks (the bug this refactor fixed).

## HebCal API

- Main endpoint: `/hebcal` with 60-day window, ~32KB response buffer
- Zmanim endpoint: `/zmanim` for a single date, ~4KB response buffer
- Zmanim API uses **camelCase** field names: `minchaGedola`, `plagHaMincha`, `sunset` (not snake_case)
- Daily study categories: `dafyomi`, `dailyPsalms`, `dailyRambam3` (not `tehillim`/`rambam`)
- Parsha: capture the **first** `parashat` item (via `found_parsha` flag) — the API returns them chronologically, so the first is the current week's
- Filter daily items (candles, havdalah, omer, dafyomi, etc.) by today's date using `matches_today()`

## Time formatting gotcha

`fmt_time_12h()` uses `sscanf(iso, "%*[^T]T%d:%d")` — the `%*[^T]` requires at least one non-`T` character before the `T`. Do NOT pass strings starting with `T` (like `"T12:26:00"`) — the parse will fail silently and return `"--:--"`. Use `format_12h(h, m)` directly instead.

## Display rendering

- 4-quadrant layout on 480x320: Top-Left (times), Top-Right (parsha), Bottom-Left (today/daily study), Bottom-Right (upcoming events)
- Top boxes are asymmetric (left=190px, right=290px); bottom boxes are equal (240px each)
- Box backgrounds are solid black `Color(0,0,0)` — the MIPI SPI driver does NOT support alpha blending in `filled_rectangle`
- `auto_clear_enabled: false` — boxes must extend edge-to-edge or old render artifacts show through gaps
- `rotation: 90` with `transform: {mirror_y: true}` — do NOT use `transform: {swap_xy: true}` (causes right-1/3 screen noise)

## Touch/swipe

Touch has `swap_xy: true` + `mirror_x: true`. Physical horizontal swapes produce `dy` changes (not `dx`). Swipe detection checks `abs(dy) > abs(dx)`.

## Hebrew text

- ESPHome renders L-to-R only. Hebrew text must be reversed via `rtl()` for display.
- Geresh (U+05F3) and gershayim (U+05F4) must be in font glyph lists or Hebrew dates render blank.
- Maqaf (U+05BE) and em dash (U+2014) also required in glyphs.
- Upcoming events: strip the Hebrew year (last word) from both English and Hebrew date strings.

## ESPTime

`ESPTime::day_of_week`: 1=Sunday, 2=Monday, ... 7=Saturday. The code converts to 0=Mon...6=Sun via `(dow + 5) % 7`.

## Build

- Framework: Arduino (not ESP-IDF), despite ESPHome offering both
- No PSRAM — all buffers in internal DRAM. Keep response buffers small.
- Flash usage ~80%, RAM ~17%
