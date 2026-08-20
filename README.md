# Kosher Home Automation — Shabbos Clock

A standalone ESPHome-powered Jewish Shabbos/Holiday information display.

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
StandaloneShabbosClock.yaml    — Main entry: data fetching, display rendering, fonts
board_cyd35.yaml               — Board definition: ESP32, SPI bus, boot button
display_cyd35.yaml             — Display + touchscreen hardware + rendering lambda
include/shabbos_clock_helpers.h — Data extraction & URL building helpers
background.png                 — Kotel (Western Wall) background image, 480×320
```

## Display Approach: NO LVGL

After extensive testing, LVGL was removed from the build. **LVGL causes washed-out/flickering text** on this no-PSRAM board due to software rotation with a tiny draw buffer. The display uses **ESPHome's direct `display.lambda` rendering** (`it.print()`, `it.filled_rectangle()`, `it.image()`) which draws text directly to the frame buffer without an intermediate rotation layer.

## Rotation Mechanism

The display is physically mounted in landscape (rotated 90° from the ST7796's native 320×480 portrait). Rotation is handled by the mipi_spi driver's `rotation:` parameter:

```yaml
rotation: 270
```

This sets the MADCTL MV bit (and appropriate MX/MY bits) in the display's register. Unlike `transform: {swap_xy: true}`, the `rotation:` property sends CASET/PASET **after** updating MADCTL, avoiding column/page addressing corruption that caused **right-1/3 screen noise**.

## The Flicker Issue (UNRESOLVED)

A persistent screen flicker remains despite all attempts. This is **not** a hardware defect — the factory demo (LovyanGFX) ran without flicker. The flicker appears as a visible periodic variation in brightness/display content.

### What has been tested (none resolved the flicker):

| Attempt | Result |
|---------|--------|
| LVGL software rotation 90°/270° | Flicker present |
| LVGL removed; direct `lambda:` rendering | Flicker present |
| Backlight: GPIO switch (always-on, no PWM) | Flicker present |
| Backlight: LEDC output + monochromatic light | Flicker present |
| Touch: removed entirely (no SPI contention) | Flicker present |
| Display: MADCTL via `transform: {swap_xy: true}` | Flicker present, plus right-1/3 noise |
| Display: MADCTL via `rotation: 90/270` | Flicker present |
| `buffer_size: 0.25` (mipi_spi) | Flicker present |
| `update_interval: 30s/3600s/never` | Flicker present |
| `auto_clear_enabled: false` | Flicker present |
| SPI data rate: 40MHz → 26.67MHz → 20MHz | Flicker present |
| Board 1 vs Board 2 (two identical CYD units) | Both flicker identically |

### Suspected root cause

The factory demo runs LovyanGFX with DMA and a full frame buffer, sending pixel data at high speed with minimal CPU intervention. ESPHome's mipi_spi driver does not use DMA for this display, and on a no-PSRAM ESP32 it flushes in small tiles. The **SPI transaction timing** or **display-side IC behaviour** during these partial flushes may cause the visible flicker.

A potential fix path: implement custom SPI writes via the `spi:` component directly, or switch to a non-ESPHome framework (TFT_eSPI, LovyanGFX) for the display while using ESPHome only for WiFi / NTP / HebCal fetching.

## Data Source: HebCal JSON API

Fetched every 30 minutes. URL:

```
https://www.hebcal.com/hebcal?v=1&cfg=json&maj=on&min=on&mod=on&nx=on&d=on&o=on&
molad=on&ss=on&mf=on&c=on&geo=geoname&M=on&s=on&leyning=on&
dps=on&dr1=on&dr3=on&F=on&geonameid=2147714&start=...&end=...
```

14-day window starting today. Key JSON categories used:

| Category | Content |
|----------|---------|
| `hebdate` | Hebrew date for each day |
| `candles` | Candle-lighting time (Friday), includes parasha name in `memo` |
| `havdalah` | Shabbat-end time (Saturday night) |
| `parashat` | Weekly Torah portion, with `leyning.torah` and `leyning.haftarah` |
| `holiday` | Upcoming holidays (major/minor/fast), with `yomtov` flag |
| `zmanim` | Fast start/end times (subcat: `fast`) |

## Display Layout (480×320 landscape)

```
┌─ "G" ──── Header (Heb date · Gregorian · City) ──────────────┐  y=0, h=28
├───────────────────────────────────────────────────────────────┤
│ ┌─── Left panel (w=234) ───┐ ┌─── Right panel (w=234) ────┐  │  y=32, h=252
│ │ 5:11 PM (gold, large)    │ │ Parashat HaShavua           │  │
│ │ Candles Ki Teitzei       │ │ Ki Teitzei (large)          │  │
│ │ 6:08 PM (gold, large)    │ │ Torah: Devarim 21:10-25:19 │  │
│ │ Havdalah                 │ │ Haftarah: Isaiah 54:1-10   │  │
│ │ ──────── divider ────────│ │ ──────── divider ──────────│  │
│ │ Today                    │ │ Next                       │  │
│ │ ט׳ אלול תשפ״ו (large)  │ │ Rosh Hashana (gold)         │  │
│ │ Saturday, 22 August 2026 │ │ 1 Tishrei 5787 · 11 Sep    │  │
│ │ Shabbat — Melachot ...   │ │                             │  │
│ │ Shofar blown today!      │ │                             │  │
│ └──────────────────────────┘ └──────────────────────────────┘  │
├───────────────────────────────────────────────────────────────┤
│ Elul | Sydney | Tap G to cycle                                 │  y=288, h=32
└───────────────────────────────────────────────────────────────┘
```

## Touch: Geoname Cycling

Tap the **"G"** area (top-left corner) to cycle through 5 preset geoname locations:

| # | City | Geoname ID |
|---|------|-----------|
| 0 | Sydney | 2147714 |
| 1 | New York | 5128581 |
| 2 | Jerusalem | 281184 |
| 3 | London | 2643743 |
| 4 | Tel Aviv | 293397 |

Touch uses `swap_xy: true, mirror_x: true` with calibration `x: 712-3506, y: 851-3420`.

## Touch Coordinate Pipeline

XPT2046 raw → `swap_xy` → `mirror_x` (invert via 4095−raw) → calibration → display coordinates.

Calibration was computed from user-provided corner taps:
- TL: (851, 3469), TR: (1373, 712), BL: (3338, 3506), BR: (3420, 764)
- x_raw increases top→bottom; y_raw decreases left→right
- `swap_xy` aligns raw axes with display axes
- `mirror_x` inverts the horizontal direction

## Fonts

| ID | Source | Size | Usage |
|----|--------|------|-------|
| `alef` | Noto Sans Hebrew | 12 | Body text, labels |
| `alef_mid` | Noto Sans Hebrew | 16 | Time values, titles, Hebrew dates |

## NTP / Timezone

- Timezone: `Australia/Sydney` (adjust for your location)
- Sync: NTP, auto-adjusts for DST
- Display uses `sntp_time.now()` which returns local time with correct `day_of_week` (1=Monday, 7=Sunday)
- First render may show incorrect date until NTP syncs (~30s after boot)

## Build Notes

- Flash usage: ~88% (1,616,659 / 1,835,008 bytes)
- RAM usage: ~16% (52,012 / 327,680 bytes)
- ESPHome version: 2026.6.2
- Framework: Arduino (idf)
- No PSRAM — all buffers in internal DRAM

## Key Files

- `include/shabbos_clock_helpers.h` — URL builders, JSON parsing, data extraction
- `display_cyd35.yaml` — Display + touch hw config and rendering lambda
- `board_cyd35.yaml` — Board pins, SPI bus, boot button
- `background.png` — 480×320 Kotel image
