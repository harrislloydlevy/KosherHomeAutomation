#pragma once

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include "esphome/core/helpers.h"
#include "esphome/components/json/json_util.h"

// ============================================================================
// Macros
// ============================================================================

// JSON_GET: safely access a JSON field, returning "" if missing/null.
// Usage: JSON_GET(obj, ["key"]["nested"])
#define JSON_GET(obj, key) ((obj)key | "")

// ============================================================================
// Time helpers
// ============================================================================

// Parse "HH:MM" from an ISO datetime like "2026-08-23T12:26:00+10:00".
// Returns -1 in both outputs if parsing fails.
static void parse_time_from_iso(const char *iso, int &hour, int &minute) {
  hour = minute = -1;
  if (!iso || !*iso) return;
  sscanf(iso, "%*[^T]T%d:%d", &hour, &minute);
}

// Format a 24h hour/minute pair into a 12h string like "12:26 PM".
// Used by both fmt_time_from_api and the zmanim maariv calculation.
static std::string format_12h(int h, int m) {
  const char *ampm = (h >= 12) ? "PM" : "AM";
  int h12 = h % 12;
  if (h12 == 0) h12 = 12;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d %s", h12, m, ampm);
  return std::string(buf);
}

// Format a full ISO datetime string ("2026-08-23T12:26:00+10:00") to 12h.
// Used for candle lighting and havdalah times from the HebCal API.
static std::string fmt_time_12h(const char *iso) {
  if (!iso || !*iso) return "--:--";
  int h = 0, m = 0;
  if (sscanf(iso, "%*[^T]T%d:%d", &h, &m) < 2) return "--:--";
  return format_12h(h, m);
}

// Format an ISO datetime from the zmanim API to 12h.
// Same parsing as fmt_time_12h but takes const char* directly.
static std::string fmt_time_from_api(const char *iso_dt) {
  if (!iso_dt || !*iso_dt) return "--:--";
  int h = 0, m = 0;
  if (sscanf(iso_dt, "%*[^T]T%d:%d", &h, &m) < 2) return "--:--";
  return format_12h(h, m);
}

// ============================================================================
// Date helpers
// ============================================================================

// ESPTime day_of_week: 1=Sunday, 2=Monday ... 7=Saturday
static const char *dow_name(int d) {
  static const char *n[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
  return n[(d + 5) % 7];
}
static const char *dow_full(int d) {
  static const char *n[] = {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
  return n[(d + 5) % 7];
}
static const char *mon_short(int m) {
  static const char *n[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  return (m >= 1 && m <= 12) ? n[m-1] : "?";
}
static const char *mon_full(int m) {
  static const char *n[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
  return (m >= 1 && m <= 12) ? n[m-1] : "?";
}

// ============================================================================
// String helpers
// ============================================================================

// Reverse a UTF-8 string by code-point (not by byte) for RTL display.
// ESPHome's display renderer draws characters left-to-right, so Hebrew
// text must be pre-reversed to appear correctly right-to-left on screen.
static std::string rtl(const std::string &s) {
  std::string result;
  const char *end = s.c_str() + s.size();
  const char *p = end;
  while (p > s.c_str()) {
    p--;
    while (p > s.c_str() && ((*p) & 0xC0) == 0x80) p--;
    unsigned char c = *p;
    int len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    result.append(p, len);
  }
  return result;
}

// Replace all occurrences of a UTF-8 substring with another.
// Used to sanitize Hebrew geresh/gershayim to ASCII for fonts that
// don't include those codepoints.
static void replace_utf8(std::string &s, const std::string &from, const std::string &to) {
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

// Strip embedded newlines from a string (replaces with spaces).
// HebCal API field values can contain \n which triggers font warnings
// when passed to it.print(). Used for single-line display fields only;
// multi-line fields (zmanim, upcoming, daily_study) must NOT be stripped.
static void strip_newlines(std::string &s) {
  for (size_t p = s.find('\n'); p != std::string::npos; p = s.find('\n', p))
    s.replace(p, 1, " ");
}

// Truncate a string at the first comma.
// Used for dailyRambam3 titles which contain multiple chapters separated
// by commas; we only show the first one to save space.
static std::string truncate_at_comma(const std::string &s) {
  size_t pos = s.find(',');
  if (pos != std::string::npos) return s.substr(0, pos);
  return s;
}

// Check if a HebCal item's date matches today's date (yr/mo/dy).
// Item date can be "YYYY-MM-DD" or "YYYY-MM-DDTHH:MM:SS".
static bool matches_today(const char *date_str, int yr, int mo, int dy) {
  if (!date_str || !*date_str) return false;
  int y = 0, m = 0, d = 0;
  if (sscanf(date_str, "%d-%d-%d", &y, &m, &d) >= 3)
    return y == yr && m == mo && d == dy;
  return false;
}

// ============================================================================
// Predefined locations
// ============================================================================

static const int GEONAME_PRESETS[] = {2147714, 5128581, 281184, 2643743, 293397};
static const char *GEONAME_CITIES[] = {"Sydney","New York","Jerusalem","London","Tel Aviv"};
static const int GEONAME_COUNT = 5;

// ============================================================================
// URL builders
// ============================================================================

// Build HebCal API URL for a 60-day window starting at the given date.
// The 60-day window provides enough data for upcoming events without
// requiring a second request. Includes leyning (Torah readings),
// daily study, holidays, and candle lighting times.
static std::string build_hebcal_url(int gid, int y, int m, int d) {
  int ey = y, em = m, ed = d;
  static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  for (int i = 0; i < 60; i++) {
    int lim = dim[em - 1];
    if (em == 2 && (ey % 4 == 0 && (ey % 100 != 0 || ey % 400 == 0))) lim = 29;
    if (++ed > lim) { ed = 1; if (++em > 12) { em = 1; ey++; } }
  }
  char buf[320];
  snprintf(buf, sizeof(buf),
    "http://www.hebcal.com/hebcal?v=1&cfg=json&"
    "maj=on&min=on&mod=on&nx=on&d=on&o=on&"
    "molad=on&ss=on&mf=on&c=on&geo=geoname&"
    "M=on&s=on&leyning=on&dps=on&dr1=on&dr3=on&F=on&"
    "geonameid=%d&start=%04d-%02d-%02d&end=%04d-%02d-%02d",
    gid, y, m, d, ey, em, ed);
  return std::string(buf);
}

// Build HebCal zmanim API URL for a single date.
// Returns detailed daily zmanim (mincha gedola, plag hamincha, sunset, etc.)
// from a separate endpoint than the main HebCal API.
static std::string build_zmanim_url(int gid, int y, int m, int d) {
  char buf[160];
  snprintf(buf, sizeof(buf),
    "https://www.hebcal.com/zmanim?cfg=json&geonameid=%d&date=%04d-%02d-%02d",
    gid, y, m, d);
  return std::string(buf);
}

// ============================================================================
// JSON response parsers
// ============================================================================

static bool parse_hebcal_response(JsonDocument &doc, const std::string &body) {
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    ESP_LOGE("hebcal", "JSON error: %s", err.c_str());
    return false;
  }
  ESP_LOGI("hebcal", "Parsed %d bytes OK", (int)body.length());
  return true;
}

static bool parse_zmanim_response(JsonDocument &doc, const std::string &body) {
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    ESP_LOGE("zmanim", "JSON error: %s", err.c_str());
    return false;
  }
  ESP_LOGI("zmanim", "Parsed %d bytes OK", (int)body.length());
  return true;
}

// ============================================================================
// Data extraction
// ============================================================================

// Build the zmanim display string from the zmanim API response.
// Extracts minchaGedola, plagHaMincha from the API and computes
// Maariv as sunset + 50 minutes.
// Returns empty string if required fields are missing.
static std::string build_zmanim_string(const JsonDocument &doc) {
  const char *mg   = JSON_GET(doc, ["times"]["minchaGedola"]);
  const char *plag = JSON_GET(doc, ["times"]["plagHaMincha"]);
  const char *sun  = JSON_GET(doc, ["times"]["sunset"]);
  if (!mg || !*mg || !plag || !*plag) return "";
  std::string mincha_s = fmt_time_from_api(mg);
  std::string plag_s   = fmt_time_from_api(plag);
  std::string maariv_s = "--:--";
  // Maariv = sunset + 50 min (standard ashkenazi practice)
  if (sun && *sun) {
    int h = 0, m = 0;
    if (sscanf(sun, "%*[^T]T%d:%d", &h, &m) >= 2) {
      m += 50;
      if (m >= 60) { h++; m -= 60; }
      maariv_s = format_12h(h, m);
    }
  }
  return str_sprintf("Mincha: %s\nPlag: %s\nMaariv: %s",
    mincha_s.c_str(), plag_s.c_str(), maariv_s.c_str());
}

// Extract all display data from the HebCal and zmanim JSON documents.
//
// This is called ONCE after each data fetch (not on every render) to
// pre-compute all display strings. The render functions then just draw
// from these strings without any JSON parsing, keeping render time fast.
//
// The HebCal API returns a 60-day window of items. We filter by today's
// date for daily items (candles, havdalah, dafyomi, etc.) and collect
// upcoming holidays/rosh chodesh for the events list.
static void extract_display_data(
    JsonDocument &doc,
    JsonDocument &zmanim_doc,
    int yr, int mo, int dy, int hr, int mi, int dow, int geoname_idx,
    // outputs:
    std::string &header,
    std::string &candles_time,
    std::string &candles_icon,
    std::string &havdalah_time,
    std::string &havdalah_icon,
    std::string &hebrew_date,
    std::string &english_date,
    std::string &parasha_hebrew,
    std::string &parasha_torah,
    std::string &parasha_haftarah,
    std::string &daily_study,
    std::string &parasha_english,
    std::string &zmanim,
    std::string &upcoming,
    std::string &today_notes,
    std::string &shofar_status,
    std::string &footer) {

  const char *city      = JSON_GET(doc, ["location"]["city"]);
  const char *first_hd  = JSON_GET(doc, ["items"][0]["hdate"]);

  std::string candles_dt, havdalah_dt;
  std::string p_name, p_hebrew, p_torah, p_haftarah;
  std::string heb_today_hebrew;
  bool found_today = false;
  bool found_parsha = false;

  bool is_yomtov = false, is_shabbat = false;
  const char *yt_name = "";

  JsonArray items = doc["items"];
  for (JsonObject obj : items) {
    const char *cat = JSON_GET(obj, ["category"]);

    // Candle lighting — only for today's date
    if (!strcmp(cat, "candles")) {
      if (matches_today(JSON_GET(obj, ["date"]), yr, mo, dy)) {
        candles_dt = JSON_GET(obj, ["date"]);
      }

    // Havdalah — only for today's date
    } else if (!strcmp(cat, "havdalah")) {
      if (matches_today(JSON_GET(obj, ["date"]), yr, mo, dy)) {
        havdalah_dt = JSON_GET(obj, ["date"]);
      }

    // Parsha — capture the FIRST one in the 60-day range (current week's)
    } else if (!strcmp(cat, "parashat")) {
      if (!found_parsha) {
        p_name    = JSON_GET(obj, ["title"]);
        p_hebrew  = JSON_GET(obj, ["hebrew"]);
        p_torah   = JSON_GET(obj, ["leyning"]["torah"]);
        p_haftarah = JSON_GET(obj, ["leyning"]["haftarah"]);
        found_parsha = true;
        ESP_LOGI("parsha", "Captured first parsha: %s (date=%s)",
          p_name.c_str(), JSON_GET(obj, ["date"]));
      } else {
        ESP_LOGD("parsha", "Skipping subsequent parsha: %s", JSON_GET(obj, ["title"]));
      }

    // Holiday — add to upcoming events list
    // Also detect Yom Tov (affects melachot/shofar status)
    } else if (!strcmp(cat, "holiday")) {
      if (JSON_GET(obj, ["yomtov"]) && *JSON_GET(obj, ["yomtov"])) {
        is_yomtov = true;
        yt_name = JSON_GET(obj, ["title"]);
      }
      if (!upcoming.empty()) upcoming += "\n";
      {
        const char *hd = JSON_GET(obj, ["hdate"]);
        const char *hb_raw = JSON_GET(obj, ["hebrew"]);
        // Strip Hebrew year from both hdate and hebrew (last word)
        std::string hd_short(hd);
        size_t sp = hd_short.rfind(' ');
        if (sp != std::string::npos) hd_short.resize(sp);
        std::string hb(hb_raw);
        sp = hb.rfind(' ');
        if (sp != std::string::npos) hb.resize(sp);
        // Gregorian date for display
        const char *gd = JSON_GET(obj, ["date"]);
        int gy = 0, gm = 0, gd_n = 0;
        char gb[16] = "";
        if (sscanf(gd, "%d-%d-%d", &gy, &gm, &gd_n) >= 3)
          snprintf(gb, sizeof(gb), "%d %s", gd_n, mon_short(gm));
        // Format: title|hd_short|hebrew|greg_date
        upcoming += str_sprintf("%s|%s|%s|%s",
          JSON_GET(obj, ["title"]), hd_short.c_str(), hb.c_str(), gb);
      }

    // Rosh Chodesh — same format as holidays
    } else if (!strcmp(cat, "roshchodesh")) {
      if (!upcoming.empty()) upcoming += "\n";
      {
        const char *hd = JSON_GET(obj, ["hdate"]);
        const char *hb_raw = JSON_GET(obj, ["hebrew"]);
        std::string hd_short(hd);
        size_t sp = hd_short.rfind(' ');
        if (sp != std::string::npos) hd_short.resize(sp);
        std::string hb(hb_raw);
        sp = hb.rfind(' ');
        if (sp != std::string::npos) hb.resize(sp);
        const char *gd = JSON_GET(obj, ["date"]);
        int gy = 0, gm = 0, gd_n = 0;
        char gb[16] = "";
        if (sscanf(gd, "%d-%d-%d", &gy, &gm, &gd_n) >= 3)
          snprintf(gb, sizeof(gb), "%d %s", gd_n, mon_short(gm));
        upcoming += str_sprintf("%s|%s|%s|%s",
          JSON_GET(obj, ["title"]), hd_short.c_str(), hb.c_str(), gb);
      }

    // Omer counting — only for today, shown in today_notes
    } else if (!strcmp(cat, "omer")) {
      if (matches_today(JSON_GET(obj, ["date"]), yr, mo, dy)) {
        if (!today_notes.empty()) today_notes += "\n";
        today_notes += JSON_GET(obj, ["title"]);
      }

    // Hebrew date — find today's
    } else if (!strcmp(cat, "hebdate")) {
      if (!found_today) {
        int y2, m2, d2;
        if (sscanf(JSON_GET(obj, ["date"]), "%d-%d-%d", &y2, &m2, &d2) == 3) {
          if (y2 == yr && m2 == mo && d2 == dy) {
            heb_today_hebrew = JSON_GET(obj, ["hebrew"]);
            found_today = true;
          }
        }
      }
    }
  }
  // Fallback if today's hebdate wasn't found in the response
  if (!found_today) heb_today_hebrew = JSON_GET(doc, ["items"][0]["hebrew"]);

  // Determine if currently Shabbat (for shofar logic only).
  // Candle/havdalah times are already filtered to today, so we just
  // check if we're in the Shabbat window based on day of week + times.
  int wd = (dow + 5) % 7;  // 0=Mon ... 5=Sat, 6=Sun
  int c_h = -1, c_m = -1, h_h = -1, h_m = -1;
  parse_time_from_iso(candles_dt.c_str(), c_h, c_m);
  parse_time_from_iso(havdalah_dt.c_str(), h_h, h_m);
  if (wd == 4 && c_h >= 0 && hr * 60 + mi >= c_h * 60 + c_m) is_shabbat = true;
  else if (wd == 5 && h_h >= 0 && hr * 60 + mi < h_h * 60 + h_m) is_shabbat = true;

  // Detect Hebrew month for seasonal notes (Selichot, shofar)
  const char *heb_mon_ptr = strrchr(first_hd, ' ');
  const char *heb_mon = heb_mon_ptr ? heb_mon_ptr + 1 : "";
  bool is_elul = strstr(heb_mon, "Elul") || strstr(heb_mon, "\u05D0\u05DC\u05D5\u05DC");
  bool blow_shofar = is_elul && wd != 4 && wd != 5;
  bool is_tishrei = strstr(heb_mon, "Tishrei") || strstr(heb_mon, "\u05EA\u05E9\u05E8\u05D9");

  // ---- Format header ----
  header = str_sprintf("%s | %s %d %s %d | %s",
    first_hd, dow_name(dow), dy, mon_short(mo), yr, city);

  // ---- Format candle/havdalah times ----
  candles_time = candles_dt.empty() ? "--:--" : fmt_time_12h(candles_dt.c_str());
  candles_icon = "\U000F17D3";  // Material Design candle icon
  havdalah_time = havdalah_dt.empty() ? "--:--" : fmt_time_12h(havdalah_dt.c_str());
  havdalah_icon = "\U000F059B";  // Material Design sunset icon

  // ---- Format Hebrew date ----
  // Remove "of " prefix if present (e.g. "7th of Elul" -> "7th Elul")
  hebrew_date = rtl(heb_today_hebrew);
  // Replace geresh/gershayim with ASCII equivalents for font compatibility
  replace_utf8(hebrew_date, "\u05F3", "'");
  replace_utf8(hebrew_date, "\u05F4", "\"");

  // ---- Format English date ----
  english_date = str_sprintf("%s, %d %s %d", dow_full(dow), dy, mon_full(mo), yr);

  // ---- Format parsha ----
  // Strip "פרשת " prefix from Hebrew name, then RTL for display
  std::string p_heb_display = p_hebrew;
  if (p_heb_display.find("\u05E4\u05E8\u05E9\u05EA") == 0)
    p_heb_display = p_heb_display.substr(8);  // "פרשת " = 5 chars + 1 space = 6 bytes... actually 8 UTF-8 bytes
  parasha_hebrew = rtl(p_heb_display);
  parasha_torah  = p_torah;
  parasha_english = p_name;
  parasha_haftarah = p_haftarah;

  // ---- Daily study (daf yomi, psalms, rambam) — today only ----
  daily_study.clear();
  for (JsonObject obj : items) {
    if (!matches_today(JSON_GET(obj, ["date"]), yr, mo, dy)) continue;
    const char *cat = JSON_GET(obj, ["category"]);
    if (!strcmp(cat, "dafyomi")) {
      if (!daily_study.empty()) daily_study += "\n";
      daily_study += JSON_GET(obj, ["title"]);
    } else if (!strcmp(cat, "dailyPsalms")) {
      if (!daily_study.empty()) daily_study += "\n";
      daily_study += JSON_GET(obj, ["title"]);
    } else if (!strcmp(cat, "dailyRambam3")) {
      if (!daily_study.empty()) daily_study += "\n";
      // Only show first chapter (truncate at comma)
      daily_study += truncate_at_comma(JSON_GET(obj, ["title"]));
    }
  }

  // ---- Zmanim — from API, fallback to empty ----
  // The zmanim API provides accurate minchaGedola, plagHaMincha, and sunset.
  // We compute Maariv as sunset + 50 min. If the API data is missing,
  // zmanim stays empty and the display shows nothing for that section.
  zmanim = build_zmanim_string(zmanim_doc);

  // ---- Today notes (Omer, Selichot season) ----
  if (is_elul) {
    if (!today_notes.empty()) today_notes += "\n";
    today_notes += "Selichot season";
  }
  if (is_tishrei) {
    if (!today_notes.empty()) today_notes += "\n";
    today_notes += "Selichot / Yomim Noraim";
  }

  // ---- Shofar status ----
  if (blow_shofar)
    shofar_status = str_sprintf("Shofar today! \u05EA\u05E7\u05D9\u05E2\u05D4 \u05E9\u05D1\u05E8\u05D9\u05DD \u05EA\u05E8\u05D5\u05E2\u05D4");
  else if (is_elul)
    shofar_status = "Shabbat \u2014 no shofar";
  else if (is_tishrei)
    shofar_status = "Tishrei \u2014 Selichot / Yomim Noraim";
  else
    shofar_status = "";

  // ---- Sanitize single-line fields (NOT multi-line: zmanim, upcoming, daily_study) ----
  strip_newlines(candles_time);
  strip_newlines(havdalah_time);
  strip_newlines(hebrew_date);
  strip_newlines(english_date);
  strip_newlines(parasha_hebrew);
  strip_newlines(parasha_torah);
  strip_newlines(parasha_haftarah);
  strip_newlines(parasha_english);
  strip_newlines(today_notes);
  strip_newlines(shofar_status);

  // ---- Footer ----
  footer = str_sprintf("%s | %s", heb_mon, GEONAME_CITIES[geoname_idx]);
}

// ============================================================================
// Rendering: Main Shabbos clock screen
// ============================================================================

// Renders the main Shabbos clock screen from pre-computed display strings.
//
// IMPORTANT: This function does NOT parse any JSON. All data extraction
// happens in extract_display_data() which is called once after each data
// fetch (every 2 hours). This function just draws from the pre-computed
// strings, keeping render time under ~3 seconds instead of adding the
// overhead of JSON parsing on every frame.
//
// Layout: 4-quadrant grid on a 480x320 display
//   Top-Left:  Times (candle, havdalah, shofar, zmanim)
//   Top-Right: Parsha (English name, Hebrew name, Torah, Haftarah)
//   Bot-Left:  Today (Hebrew date, English date, notes, daily study)
//   Bot-Right: Upcoming events
// Plus a 28px header bar and 32px footer bar.
template<typename TDisplay, typename TFont, typename TIconFont, typename TImage>
static void render_shabbos(
    TDisplay &it,
    TFont *font_small,
    TFont *font_mid,
    TIconFont *font_icons,
    TImage *bg_img,
    const std::string &hdr,
    const std::string &candles_time, const std::string &candles_icon,
    const std::string &havdalah_time, const std::string &havdalah_icon,
    const std::string &hebrew_date,
    const std::string &english_date,
    const std::string &parasha_hebrew,
    const std::string &parasha_torah,
    const std::string &parasha_haftarah,
    const std::string &daily_study,
    const std::string &parasha_english,
    const std::string &zmanim,
    const std::string &upcoming,
    const std::string &today_notes,
    const std::string &shofar_status) {

  auto c_bg    = Color(0, 0, 0);
  auto c_gold  = Color(255, 200, 100);
  auto c_wht   = Color(255, 255, 255);
  auto c_dim   = Color(160, 160, 160);
  auto c_div   = Color(50, 50, 40);

  // Draw background image first, then overlay boxes on top
  if (bg_img) it.image(0, 0, bg_img);

  // ---- Header bar (full width, 28px) ----
  it.filled_rectangle(0, 0, 480, 28, c_bg);
  it.print(240, 6, font_small, c_wht, TextAlign::CENTER, hdr.c_str());

  // ---- Dividers between the 4 quadrants ----
  // Top boxes are asymmetric: left=190px (times), right=290px (parsha text)
  // Bottom boxes are equal width (240px each)
  it.filled_rectangle(0, 145, 480, 1, c_div);             // horizontal
  it.filled_rectangle(190, 32, 1, 113, c_div);             // top vertical (at x=190)
  it.filled_rectangle(240, 145, 1, 142, c_div);            // bottom vertical (at x=240)

  int mx = 8, my = 34;               // content margin from box edges
  int col_w_l = 186, col_w_r = 290;  // top left/right box widths (edge to edge)
  int col_w_b = 240;                 // bottom box width (edge to divider to edge)
  int box_h_t = 111, box_h_b = 138;  // top/bottom box heights

  // ---- Top-Left: Times (candle, havdalah, shofar, mincha/plag/maariv) ----
  int x = mx, y = my;
  it.filled_rectangle(0, y - 4, col_w_l, box_h_t, c_bg);
  // Candle lighting (shown only when valid for today)
  if (candles_time != "--:--") {
    it.print(x, y, font_icons, c_gold, candles_icon.c_str());
    it.print(x + 24, y, font_mid, c_gold, candles_time.c_str()); y += 22;
  }
  // Havdalah (shown only when valid for today)
  if (havdalah_time != "--:--") {
    it.print(x, y, font_icons, c_gold, havdalah_icon.c_str());
    it.print(x + 24, y, font_mid, c_gold, havdalah_time.c_str()); y += 22;
  }
  // Shofar status (Elul/Tishrei only)
  if (!shofar_status.empty()) {
    it.print(x, y, font_small, c_gold, shofar_status.c_str()); y += 16;
  }
  // Zmanim (Mincha, Plag, Maariv — each on its own line)
  if (!zmanim.empty()) {
    std::string::size_type pos = 0;
    while (true) {
      auto nl = zmanim.find('\n', pos);
      if (nl == zmanim.npos) {
        it.print(x, y, font_small, c_dim, zmanim.substr(pos).c_str());
        break;
      }
      it.print(x, y, font_small, c_dim, zmanim.substr(pos, nl - pos).c_str());
      y += 16;
      pos = nl + 1;
    }
  }

  // ---- Top-Right: Parsha (English name, Hebrew name, Torah, Haftarah) ----
  x = 194; y = my;
  it.filled_rectangle(x - 4, y - 4, col_w_r, box_h_t, c_bg);
  it.print(x, y, font_icons, c_dim, "\U000F1CCC");  // book icon
  // English parsha name (gold, next to book icon)
  if (!parasha_english.empty()) { it.print(x + 22, y, font_small, c_gold, parasha_english.c_str()); y += 14; }
  // Hebrew parsha name (white, right-aligned — Hebrew reads R-to-L)
  if (!parasha_hebrew.empty()) { it.print(x + col_w_r - 4, y, font_small, c_wht, TextAlign::RIGHT, parasha_hebrew.c_str()); y += 14; }
  // Torah reading
  if (!parasha_torah.empty()) { it.print(x, y, font_small, c_wht, parasha_torah.c_str()); y += 14; }
  // Haftarah reading
  if (!parasha_haftarah.empty()) { it.print(x, y, font_small, c_wht, parasha_haftarah.c_str()); y += 14; }

  // ---- Bottom-Left: Today (Hebrew date, English date, notes, daily study) ----
  x = mx; y = 149;
  it.filled_rectangle(0, y - 4, col_w_b, box_h_b, c_bg);
  // Hebrew date (right-aligned — Hebrew reads R-to-L)
  it.print(x + col_w_b - 4, y, font_mid, c_wht, TextAlign::RIGHT, hebrew_date.c_str()); y += 20;
  // English date
  it.print(x, y, font_small, c_wht, english_date.c_str()); y += 16;
  // Today notes (Omer, Selichot — gold)
  if (!today_notes.empty()) {
    std::string::size_type pos = 0;
    while (true) {
      auto nl = today_notes.find('\n', pos);
      if (nl == today_notes.npos) {
        it.print(x, y, font_small, c_gold, today_notes.substr(pos).c_str());
        break;
      }
      it.print(x, y, font_small, c_gold, today_notes.substr(pos, nl - pos).c_str());
      y += 16;
      pos = nl + 1;
    }
  }
  // Daily study (Daf Yomi, Psalms, Rambam — dim)
  if (!daily_study.empty()) {
    std::string::size_type pos = 0;
    while (true) {
      auto nl = daily_study.find('\n', pos);
      if (nl == daily_study.npos) {
        it.print(x, y, font_small, c_dim, daily_study.substr(pos).c_str());
        break;
      }
      it.print(x, y, font_small, c_dim, daily_study.substr(pos, nl - pos).c_str());
      y += 16;
      pos = nl + 1;
    }
  }

  // ---- Bottom-Right: Upcoming events ----
  // Each event is stored as: title|hd_short|hebrew|greg_date
  // Rendered as: line 1 = dates, line 2 = English name + Hebrew name (RTL)
  x = 244; y = 149;
  it.filled_rectangle(x - 4, y - 4, col_w_b, box_h_b, c_bg);
  if (!upcoming.empty()) {
    std::string::size_type pos = 0;
    while (true) {
      auto nl = upcoming.find('\n', pos);
      std::string evt = (nl == upcoming.npos) ? upcoming.substr(pos) : upcoming.substr(pos, nl - pos);
      // Split on '|' to get the 4 fields
      size_t p1 = evt.find('|');
      size_t p2 = p1 != std::string::npos ? evt.find('|', p1 + 1) : std::string::npos;
      size_t p3 = p2 != std::string::npos ? evt.find('|', p2 + 1) : std::string::npos;
      std::string title   = (p1 != std::string::npos) ? evt.substr(0, p1) : evt;
      std::string hd_s    = (p2 != std::string::npos) ? evt.substr(p1 + 1, p2 - p1 - 1) : "";
      std::string heb     = (p3 != std::string::npos) ? evt.substr(p2 + 1, p3 - p2 - 1) : "";
      std::string greg_s  = (p3 != std::string::npos) ? evt.substr(p3 + 1) : "";
      // Line 1: Hebrew date (English) + Gregorian date
      std::string date_line = hd_s;
      if (!greg_s.empty()) date_line += "  " + greg_s;
      if (!date_line.empty()) { it.print(x, y, font_small, c_dim, date_line.c_str()); y += 14; }
      // Line 2: English name + Hebrew name (reversed for RTL display)
      std::string name_line = title;
      if (!heb.empty()) name_line += "  " + rtl(heb);
      if (!name_line.empty()) { it.print(x, y, font_small, c_wht, name_line.c_str()); y += 14; }
      // 6px gap before next event
      y += 6;
      if (nl == upcoming.npos) break;
      pos = nl + 1;
    }
  }

  // ---- Footer bar (full width, 32px) ----
  it.filled_rectangle(0, 288, 480, 32, c_bg);
  it.print(240, 295, font_small, c_dim, TextAlign::CENTER, "Swipe Left to set location, Swipe Right to setup Wifi");
}

// ============================================================================
// Rendering: Location settings screen (swipe left from main screen)
// ============================================================================

template<typename TDisplay, typename TFont, typename TIconFont, typename TImage>
static void render_location_screen(
    TDisplay &it,
    TFont *font_small,
    TFont *font_mid,
    TIconFont *font_icons,
    TImage *bg_img,
    int geoname_id,
    const std::string &input) {

  auto c_bg    = Color(0, 0, 0);
  auto c_gold  = Color(255, 200, 100);
  auto c_wht   = Color(255, 255, 255);
  auto c_dim   = Color(160, 160, 160);
  auto c_btn   = Color(40, 40, 80);

  if (bg_img) it.image(0, 0, bg_img);

  // Header
  it.filled_rectangle(0, 0, 480, 28, c_bg);
  it.print(4, 6, font_small, c_wht, "Set Location");

  // Current location info
  it.print(10, 36, font_mid, c_gold, "Current GeoName ID:");
  char id_buf[32];
  snprintf(id_buf, sizeof(id_buf), "%d", geoname_id);
  it.print(10, 60, font_mid, c_wht, id_buf);

  // Input display
  it.filled_rectangle(10, 90, 460, 36, c_btn);
  std::string display_str = input.empty() ? "Enter ID..." : input;
  it.print(16, 96, font_mid, c_wht, display_str.c_str());

  it.print(10, 136, font_small, c_dim, "Lookup your city's GeoName ID at:");
  it.print(10, 152, font_small, c_gold, "hebcal.com/geo");

  // Numeric keypad - 3x4 grid
  // Keys: 1-9, backspace, 0, submit
  // Position: x from 20 to 460, y from 180 to 280
  // Each key: 100x30, with 10px gaps
  int kx = 20, ky = 180, kw = 100, kh = 30, gap = 10;
  const char *keys[] = {
    "1", "2", "3",
    "4", "5", "6",
    "7", "8", "9",
    "\U000F049A", "0", "\u2714"
  };
  for (int i = 0; i < 12; i++) {
    int col = i % 3;
    int row = i / 3;
    int x = kx + col * (kw + gap);
    int y = ky + row * (kh + gap);
    // Draw key background
    it.filled_rectangle(x, y, kw, kh, c_btn);
    // Draw key border
    it.rectangle(x, y, kw, kh, c_dim);
    // Draw key label
    int tx = x + kw / 2;
    int ty = y + 4;
    if (i == 9) {
      // Backspace icon
      it.print(tx - 16, ty + 2, font_mid, c_gold, "\U000F049A");
    } else if (i == 11) {
      // Submit checkmark
      it.print(tx - 8, ty + 2, font_mid, c_gold, "\u2714");
    } else {
      it.print(tx - 6, ty + 2, font_mid, c_wht, keys[i]);
    }
  }

  // Footer
  it.filled_rectangle(0, 287, 480, 1, c_dim);
  it.filled_rectangle(0, 288, 480, 32, c_bg);
  it.print(240, 295, font_small, c_dim, TextAlign::CENTER, "Swipe Right to return");
}

// ============================================================================
// Rendering: WiFi settings screen (swipe right from main screen)
// ============================================================================

template<typename TDisplay, typename TFont, typename TIconFont, typename TImage, typename TQr>
static void render_wifi_screen(
    TDisplay &it,
    TFont *font_small,
    TFont *font_mid,
    TIconFont *font_icons,
    TImage *bg_img,
    TQr *ap_qr,
    bool wifi_connected,
    const std::string &wifi_ssid,
    float signal_dbm) {

  auto c_bg    = Color(0, 0, 0);
  auto c_gold  = Color(255, 200, 100);
  auto c_wht   = Color(255, 255, 255);
  auto c_dim   = Color(160, 160, 160);
  auto c_btn   = Color(40, 40, 80);
  auto c_red   = Color(200, 50, 50);

  if (bg_img) it.image(0, 0, bg_img);

  // Header
  it.filled_rectangle(0, 0, 480, 28, c_bg);
  it.print(4, 6, font_small, c_wht, "WiFi Settings");

  // Connection status
  it.print(10, 36, font_mid, wifi_connected ? c_gold : c_red,
    wifi_connected ? "Connected" : "Not Connected");

  if (wifi_connected) {
    it.print(10, 60, font_small, c_wht, "SSID:");
    if (!wifi_ssid.empty()) {
      it.print(60, 60, font_small, c_wht, wifi_ssid.c_str());
    }
    char db_buf[32];
    if (isnan(signal_dbm)) {
      snprintf(db_buf, sizeof(db_buf), "Signal: -- dBm");
    } else {
      snprintf(db_buf, sizeof(db_buf), "Signal: %.0f dBm", signal_dbm);
    }
    it.print(10, 80, font_small, c_wht, db_buf);
  } else {
    it.print(10, 60, font_small, c_dim, "Connect via AP at 192.168.4.1");
  }

  // QR Code - connect to captive portal
  it.print(240, 44, font_small, c_dim, TextAlign::CENTER, "Scan to connect");
  it.qr_code(160, 60, ap_qr, c_wht, 3);

  // Reset WiFi button
  it.filled_rectangle(40, 190, 400, 40, c_red);
  it.print(240, 200, font_mid, c_wht, TextAlign::CENTER, "RESET WiFi & Reconfigure");

  // Footer
  it.filled_rectangle(0, 287, 480, 1, c_dim);
  it.filled_rectangle(0, 288, 480, 32, c_bg);
  it.print(240, 295, font_small, c_dim, TextAlign::CENTER, "Swipe Left to return");
}
