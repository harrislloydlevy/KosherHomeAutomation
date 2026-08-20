# Kosher Home Automation — Shabbos Clock

A standalone ESPHome-powered Jewish Shabbos/Holiday information display for the Sunton ESP32-3248S035R ("Yellow CYD" 3.5").

## Hardware: Sunton ESP32-3248S035R ("Yellow CYD" 3.5")

| Component | Detail |
|-----------|--------|
| SoC | ESP32-WROOM-32 (no PSRAM, 4 MB flash) |
| Display | ST7796S 3.5" 320×480 TFT (RGB565, 16-bit) |
| Touch | XPT2046 resistive, shared SPI bus |
| Backlight | GPIO27, active high |
| SPI bus | SCK=GPIO14, MOSI=GPIO13, MISO=GPIO12 |
| LCD CS | GPIO15 |
| LCD DC | GPIO2 |
| Touch CS | GPIO33 |
| Touch IRQ | GPIO36 (⚠️ NOT used — see below) |
| Boot button | GPIO0 (inverted, pullup) |

**Pinout source:** <https://macsbug.wordpress.com/2022/10/02/esp32-3248s035/>

### ⚠️ GPIO36 (Touch IRQ) — DO NOT USE

GPIO36 (and GPIO39) share an ADC2 peripheral with the WiFi radio. When WiFi is active, reading these pins causes a hardware glitch that crashes the ESP32. The XPT2046 touch is therefore used in **polling mode** (no `interrupt_pin`).

## Configuration Architecture

```
StandaloneShabbosClock.yaml    — Main entry: data fetching, fonts, image
board_cyd35.yaml               — Board definition: ESP32, SPI bus, boot button
                                 (includes display_cyd35.yaml as a package)
display_cyd35.yaml             — Display + touchscreen hardware + rendering lambda
include/shabbos_clock_helpers.h — Data extraction, URL builders, template renderer
background.png                 — Kotel (Western Wall) background image, 480×320
```

## Display Approach: NO LVGL

**LVGL was removed after extensive testing.** On this no-PSRAM board, LVGL causes washed-out/flickering text due to software rotation with a tiny draw buffer (1/8 screen). The display uses **ESPHome's direct `display.lambda` rendering** — `it.print()`, `it.filled_rectangle()`, `it.image()` — which draws text directly without intermediate rotation.

All rendering is in the `render_shabbos()` template function in `shabbos_clock_helpers.h`. The YAML lambda is a single call:

```yaml
lambda: |-
    auto t = id(sntp_time).now();
    render_shabbos(it, id(doc), id(alef), id(alef_mid), id(mdi), id(background),
        t.year, t.month, t.day_of_month,
        t.hour, t.minute, t.day_of_week, id(geoname_idx));
```

## Rotation: CRITICAL — `rotation:` NOT `transform:`

The display is physically mounted in landscape. Two approaches exist:

| Approach | CASET/PASET timing | Right-1/3 noise | Works? |
|----------|-------------------|-----------------|--------|
| `transform: {swap_xy: true}` | Sent **before** MADCTL bits | **YES** — noise on right 1/3 | ❌ |
| `rotation: 90` | Sent **after** MADCTL bits | **NO** — clean display | ✅ |

**The `rotation:` parameter sends column/page address commands AFTER updating the MADCTL register. `transform:` does not.** Using `transform: {swap_xy: true}` causes the display to receive wrong column/page boundaries, corrupting the right 1/3 of the screen.

**Current working config:**

```yaml
rotation: 90
transform:
  mirror_y: true
  mirror_x: false
  swap_xy: false
dimensions:
  width: 320
  height: 480
```

This uses `rotation: 90` for proper CASET/PASET timing (no noise), and `transform: {mirror_y: true}` to flip the orientation right-side up. Note: `rotation: 270` causes a **blank screen** (unknown why — likely an ST7796 model limitation).

## The Flicker Issue (UNRESOLVED)

A persistent screen flicker remains despite 14+ attempts. The factory demo (LovyanGFX with DMA) ran without flicker on the same hardware, so this is not a hardware defect.

### What has been tested (none resolved the flicker):

| Attempt | Result |
|---------|--------|
| LVGL software rotation 90°/270° | Flicker present |
| LVGL removed; direct `lambda:` rendering | Flicker present |
| Backlight: GPIO switch (always-on, no PWM) | Flicker present |
| Backlight: LEDC output + monochromatic light | Flicker present |
| Touch: removed entirely (no SPI contention) | Flicker present |
| `transform: {swap_xy: true}` | Flicker present, plus right-1/3 noise |
| `rotation: 90` | Flicker present |
| `rotation: 270` | Blank screen (no flicker to observe) |
| `buffer_size: 0.25` (mipi_spi) | Flicker present |
| `update_interval: 30s / 3600s / never` | Flicker present |
| `auto_clear_enabled: false` | Flicker present |
| SPI data rate: 40MHz → 26.67MHz → 20MHz | Flicker present |
| Board 1 vs Board 2 (two identical CYD units) | Both flicker identically |

### Suspected root cause

The factory demo runs LovyanGFX with DMA and a full frame buffer, sending pixel data at high speed with minimal CPU intervention. ESPHome's mipi_spi driver does not use DMA for this display, and on a no-PSRAM ESP32 it flushes in small tiles. The **SPI transaction timing** or **display-side IC behaviour** during these partial flushes may cause the visible flicker.

A potential fix path: implement custom SPI writes via the `spi:` component directly, or switch to a non-ESPHome framework (TFT_eSPI, LovyanGFX) for the display while using ESPHome only for WiFi / NTP / HebCal fetching. On newer S3-based boards (with PSRAM) the flicker may not occur because the entire frame buffer fits in PSRAM and DMA flushes work correctly.

## Data Source: HebCal JSON API

Fetched every 30 minutes. 14-day window starting today.

```
https://www.hebcal.com/hebcal?v=1&cfg=json&maj=on&min=on&mod=on&nx=on&d=on&o=on&
molad=on&ss=on&mf=on&c=on&geo=geoname&M=on&s=on&leyning=on&
dps=on&dr1=on&dr3=on&F=on&geonameid=2147714&start=...&end=...
```

Key JSON categories used:

| Category | Content |
|----------|---------|
| `hebdate` | Hebrew date for each day |
| `candles` | Candle-lighting time (Friday), parasha name in `memo` |
| `havdalah` | Shabbat-end time (Saturday night) |
| `parashat` | Weekly Torah portion — `title` (English), `hebrew` (Hebrew name), `leyning.torah`, `leyning.haftarah` |
| `holiday` | Upcoming holidays — `title`, `hdate`, `hebrew`, `yomtov` flag, `subcat` |
| `dafyomi` | Daily Daf Yomi (if available in range) |

## Display Layout (480×320 landscape)

```
┌─── Header (Heb date | Day Date | City) ───────────────────────┐  y=0, h=28
├───────────────────────────────────────────────────────────────┤
│ ┌─── Left panel (w=234) ───┐ ┌─── Right panel (w=234) ────┐  │  y=32, h=252
│ │ [icon] 5:11 PM (gold)    │ │ Hebrew parasha name (large)  │  │
│ │ [icon] 6:08 PM (gold)    │ │ Torah: Devarim 21:10-25:19  │  │
│ │ ──────── divider ────────│ │ Haftarah: Isaiah 54:1-10    │  │
│ │ Today                    │ │ ──────── divider ──────────│  │
│ │ Hebrew date (large)      │ │ Upcoming                    │  │
│ │ English date             │ │ Rosh Hashana  1 Tishrei...  │  │
│ │ Melachot status          │ │ Yom Kippur   10 Tishrei...  │  │
│ │ Shofar / Daily study     │ │                             │  │
│ └──────────────────────────┘ └──────────────────────────────┘  │
├───────────────────────────────────────────────────────────────┤
│ Elul | Sydney             Swipe L: location  R: WiFi          │  y=288, h=32
└───────────────────────────────────────────────────────────────┘
```

## Key Design Decisions

### No LVGL
LVGL was removed because it renders text through a software rotation layer with a small buffer on no-PSRAM, causing washed-out/flickering text. Direct `display.lambda` rendering is used instead.

### `rotation:` over `transform:`
The `rotation:` parameter sends CASET/PASET **after** updating MADCTL. `transform:` sends them **before**. This makes `rotation:` the correct choice for this display. `rotation: 270` causes a blank screen (unknown why); `rotation: 90` + `transform: {mirror_y: true}` works correctly.

### Hebrew RTL
ESPHome's `it.print()` renders left-to-right. Hebrew text is reversed via `rtl()` before printing: `std::string(s.rbegin(), s.rend())`. This makes Hebrew readable when rendered LTR on the display.

### ESPTime day_of_week Convention
`ESPTime::day_of_week` uses: **1 = Sunday**, 2 = Monday, ... 7 = Saturday. (This differs from the comment in ESPHome source which says 1=Monday.)

### JsonDocument size
The 14-day HebCal response with `leyning=on` is ~3-4 KB. Stored in a `JsonDocument` global with `max_response_buffer_size: 16384`.

## Touch: Geoname Cycling

Tap the **top-left corner** to cycle through 5 preset geoname locations (silent — no visual indicator):

| # | City | Geoname ID |
|---|------|-----------|
| 0 | Sydney | 2147714 |
| 1 | New York | 5128581 |
| 2 | Jerusalem | 281184 |
| 3 | London | 2643743 |
| 4 | Tel Aviv | 293397 |

### Touch Coordinate Pipeline

XPT2046 raw → `swap_xy` → `mirror_x` (invert 4095−raw) → calibration → display coords.

Calibration from corner taps:
- TL: (851, 3469), TR: (1373, 712), BL: (3338, 3506), BR: (3420, 764)
- `swap_xy`: true (XPT2046 axes don't align with display axes)
- `mirror_x`: true (y_raw decreases left→right)
- calibration `x: 712-3506, y: 851-3420`

## Fonts

| ID | Source | Size | Usage |
|----|--------|------|-------|
| `alef` | Noto Sans Hebrew | 12 | Body text, labels, metadata |
| `alef_mid` | Noto Sans Hebrew | 16 | Time values, Hebrew date, parasha name |
| `mdi` | Material Design Icons | 16 | Candle (U+F17D3) and sunset (U+F059B) icons |

## NTP / Timezone

- Timezone: `Australia/Sydney` (adjust for your location)
- Sync: NTP, auto-adjusts for DST
- `sntp_time.now()` returns local time
- First render shows placeholder data until NTP syncs (~30s after boot)
- HebCal fetch triggered on `on_time_sync` and every 30 minutes

## Build Notes

- Flash usage: ~88% (1,616,659 / 1,835,008 bytes)
- RAM usage: ~16% (52,012 / 327,680 bytes)
- ESPHome version: 2026.6.2
- Framework: Arduino
- No PSRAM — all buffers in internal DRAM

## Key Files

- `StandaloneShabbosClock.yaml` — Main entry: data fetch, fonts, image, API
- `include/shabbos_clock_helpers.h` — All logic: URL builders, JSON parsing, data extraction, template `render_shabbos()` function
- `display_cyd35.yaml` — Display + touch hw config, single-line rendering lambda
- `board_cyd35.yaml` — Board pins, SPI bus, boot button (includes display_cyd35.yaml)
- `background.png` — 480×320 Kotel image

## Hebrew Text Issue (UNRESOLVED)

Hebrew text is not appearing on screen. The `rtl()` function reverses the string for RTL display, but Hebrew characters may not be rendering. Possible causes:
- The `alef_mid` font at size 16 might not include the Hebrew glyphs needed
- The Unicode escapes in the YAML might not match the actual HebCal response characters
- The reversed string might contain control characters or incorrect UTF-8 sequences
