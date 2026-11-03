#pragma once

#include <cstdio>
#include <cstring>
#include <cmath>
#include <cctype>
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
  char buf[32];
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

// Compute ESPTime day-of-week (1=Sun..7=Sat) from a Gregorian date.
static int esp_dow_from_date(int y, int m, int d) {
  int a = (14 - m) / 12;
  int yy = y + 4800 - a;
  int mm = m + 12 * a - 3;
  int jdn = d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
  return (jdn % 7 + 1) % 7 + 1;
}

// Normalize an event title to its base (strip trailing year and roman numerals)
// so that "Rosh Hashana 5787" and "Rosh Hashana II" both become "Rosh Hashana".
static std::string event_base_title(const std::string &title) {
  std::string s = title;
  // strip trailing 4-digit year
  if (s.size() >= 5 && s[s.size()-5] == ' ' &&
      isdigit((unsigned char)s[s.size()-1]) && isdigit((unsigned char)s[s.size()-2]) &&
      isdigit((unsigned char)s[s.size()-3]) && isdigit((unsigned char)s[s.size()-4])) {
    s.resize(s.size() - 5);
  }
  // strip trailing roman numeral tokens (II, III, IV, V)
  while (true) {
    size_t sp = s.rfind(' ');
    if (sp == std::string::npos) break;
    std::string last = s.substr(sp + 1);
    if (last == "II" || last == "III" || last == "IV" || last == "V" || last == "2" || last == "3") {
      s.resize(sp);
    } else break;
  }
  // trim trailing spaces
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

// Compute the next calendar day as ISO "YYYY-MM-DD".
static std::string next_day_iso(const std::string &iso) {
  int y = 0, m = 0, d = 0;
  if (sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) < 3) return "";
  static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int lim = dim[m - 1];
  if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) lim = 29;
  if (++d > lim) { d = 1; if (++m > 12) { m = 1; y++; } }
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
  return std::string(buf);
}

// ============================================================================
// Predefined locations
// ============================================================================

static const int GEONAME_PRESETS[] = {2147714, 2158177, 5128581, 5368361, 281184, 2643743};
static const char *GEONAME_CITIES[] = {"Sydney","Melbourne","New York","Los Angeles","Jerusalem","London"};
static const int GEONAME_COUNT = 6;

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
  for (int i = 0; i < 24; i++) {
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
// Pulls as many halachic times as available and computes Maariv as
// sunset + 50 minutes. Returns empty string if core fields are missing.
static std::string build_zmanim_string(const JsonDocument &doc) {
  const char *mg   = JSON_GET(doc, ["times"]["minchaGedola"]);
  const char *plag = JSON_GET(doc, ["times"]["plagHaMincha"]);
  const char *sun  = JSON_GET(doc, ["times"]["sunset"]);
  if (!mg || !*mg || !plag || !*plag) return "";

  std::string out;
  auto add = [&](const char *label, const char *iso) {
    if (!iso || !*iso) return;
    std::string t = fmt_time_from_api(iso);
    if (t == "--:--") return;
    if (!out.empty()) out += "\n";
    out += std::string(label) + ": " + t;
  };

  add("Alos",     JSON_GET(doc, ["times"]["alotHaShachar"]));
  add("Sunrise",  JSON_GET(doc, ["times"]["sunrise"]));
  add("Shma",     JSON_GET(doc, ["times"]["sofZmanShma"]));
  add("Tefilla",  JSON_GET(doc, ["times"]["sofZmanTfilla"]));
  add("Chatzot",  JSON_GET(doc, ["times"]["chatzot"]));
  add("MinchaG",  mg);
  add("Plag",     plag);
  // Maariv = sunset + 50 min (standard ashkenazi practice)
  std::string maariv_s = "--:--";
  if (sun && *sun) {
    int h = 0, m = 0;
    if (sscanf(sun, "%*[^T]T%d:%d", &h, &m) >= 2) {
      m += 50;
      if (m >= 60) { h++; m -= 60; }
      maariv_s = format_12h(h, m);
    }
  }
  if (maariv_s != "--:--") {
    if (!out.empty()) out += "\n";
    out += "Maariv: " + maariv_s;
  }
  add("Sunset",   sun);
  add("Tzais",    JSON_GET(doc, ["times"]["tzeit85deg"]));
  add("Midnight", JSON_GET(doc, ["times"]["chatzotNight"]));

  return out;
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
    std::string &next_candles,
    std::string &next_havdalah,
    std::string &footer) {

  const char *city      = JSON_GET(doc, ["location"]["city"]);
  const char *first_hd  = JSON_GET(doc, ["items"][0]["hdate"]);

  std::string candles_dt, havdalah_dt;
  std::string next_candles_dt, next_havdalah_dt;
  std::string p_name, p_hebrew, p_torah, p_haftarah;
  std::string heb_today_hebrew;
  bool found_today = false;
  bool found_parsha = false;

  // Temporary collection of upcoming events (merged for 2-day hags later).
  struct UpcomingEvt { std::string title; std::string hd_short; std::string heb; std::string greg_s; std::string date_iso; };
  std::vector<UpcomingEvt> up_events;

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
      if (next_candles_dt.empty()) next_candles_dt = JSON_GET(obj, ["date"]);

    // Havdalah — only for today's date
    } else if (!strcmp(cat, "havdalah")) {
      if (matches_today(JSON_GET(obj, ["date"]), yr, mo, dy)) {
        havdalah_dt = JSON_GET(obj, ["date"]);
      }
      if (next_havdalah_dt.empty()) next_havdalah_dt = JSON_GET(obj, ["date"]);

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
        UpcomingEvt e;
        e.title = JSON_GET(obj, ["title"]);
        e.hd_short = hd_short;
        e.heb = hb;
        e.greg_s = gb;
        e.date_iso = gd ? std::string(gd) : std::string();
        if (e.date_iso.size() > 10) e.date_iso.resize(10);
        up_events.push_back(e);
      }

    // Rosh Chodesh — same format as holidays
    } else if (!strcmp(cat, "roshchodesh")) {
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
        UpcomingEvt e;
        e.title = JSON_GET(obj, ["title"]);
        e.hd_short = hd_short;
        e.heb = hb;
        e.greg_s = gb;
        e.date_iso = gd ? std::string(gd) : std::string();
        if (e.date_iso.size() > 10) e.date_iso.resize(10);
        up_events.push_back(e);
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

  // Merge consecutive 2-day holidays / Rosh Chodesh into single entries.
  std::vector<UpcomingEvt> merged;
  for (size_t i = 0; i < up_events.size(); ) {
    UpcomingEvt cur = up_events[i];
    if (i + 1 < up_events.size()) {
      const UpcomingEvt &nx = up_events[i + 1];
      if (event_base_title(nx.title) == event_base_title(cur.title) &&
          !cur.date_iso.empty() && nx.date_iso == next_day_iso(cur.date_iso)) {
        // Combine date ranges.
        // Hebrew date: "1 Tishrei" + "2 Tishrei" -> "1-2 Tishrei" if same month.
        {
          size_t sp_cur = cur.hd_short.find(' ');
          size_t sp_nx = nx.hd_short.find(' ');
          std::string mon_cur = (sp_cur == std::string::npos) ? "" : cur.hd_short.substr(sp_cur + 1);
          std::string mon_nx = (sp_nx == std::string::npos) ? "" : nx.hd_short.substr(sp_nx + 1);
          std::string day_cur = (sp_cur == std::string::npos) ? cur.hd_short : cur.hd_short.substr(0, sp_cur);
          std::string day_nx = (sp_nx == std::string::npos) ? nx.hd_short : nx.hd_short.substr(0, sp_nx);
          if (mon_cur == mon_nx && !mon_cur.empty()) {
            cur.hd_short = day_cur + "-" + day_nx + " " + mon_cur;
          } else {
            cur.hd_short = cur.hd_short + " - " + nx.hd_short;
          }
        }
        // Gregorian date range.
        {
          // greg_s like "12 Sep"
          size_t sp_cur = cur.greg_s.find(' ');
          size_t sp_nx = nx.greg_s.find(' ');
          std::string mon_cur = (sp_cur == std::string::npos) ? "" : cur.greg_s.substr(sp_cur + 1);
          std::string mon_nx = (sp_nx == std::string::npos) ? "" : nx.greg_s.substr(sp_nx + 1);
          std::string day_cur = (sp_cur == std::string::npos) ? cur.greg_s : cur.greg_s.substr(0, sp_cur);
          std::string day_nx = (sp_nx == std::string::npos) ? nx.greg_s : nx.greg_s.substr(0, sp_nx);
          if (mon_cur == mon_nx && !mon_cur.empty()) {
            cur.greg_s = day_cur + "-" + day_nx + " " + mon_cur;
          } else {
            cur.greg_s = cur.greg_s + " - " + nx.greg_s;
          }
        }
        // Keep title/heb from first day.
        i += 2;
        merged.push_back(cur);
        continue;
      }
    }
    merged.push_back(cur);
    i++;
  }

  // Deduplicate by base title (keeps first occurrence).
  std::vector<UpcomingEvt> deduped;
  std::vector<std::string> seen;
  for (const UpcomingEvt &e : merged) {
    std::string base = event_base_title(e.title);
    bool found = false;
    for (const std::string &s : seen) if (s == base) { found = true; break; }
    if (found) continue;
    seen.push_back(base);
    deduped.push_back(e);
  }
  merged = deduped;

  // Build upcoming string from merged events.
  for (const UpcomingEvt &e : merged) {
    if (!upcoming.empty()) upcoming += "\n";
    upcoming += str_sprintf("%s|%s|%s|%s",
      e.title.c_str(), e.hd_short.c_str(), e.heb.c_str(), e.greg_s.c_str());
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

  // Detect Hebrew month for seasonal notes (Selichot, shofar).
  // hdate format is "DAY MONTH YEAR" (e.g. "23 Cheshvan 5787"), so the
  // month is the middle word, not the last (which is the year).
  std::string heb_mon_str;
  {
    std::string hd(first_hd);
    size_t last_sp = hd.rfind(' ');
    size_t prev_sp = (last_sp == std::string::npos) ? std::string::npos : hd.rfind(' ', last_sp > 0 ? last_sp - 1 : 0);
    if (last_sp != std::string::npos) {
      if (prev_sp == std::string::npos)
        heb_mon_str = hd.substr(0, last_sp);
      else
        heb_mon_str = hd.substr(prev_sp + 1, last_sp - prev_sp - 1);
    }
  }
  const char *heb_mon = heb_mon_str.c_str();
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

  // ---- Next candle lighting (for next Shabbat/chag) ----
  next_candles = "--:--";
  if (!next_candles_dt.empty()) {
    int ny = 0, nm = 0, nd = 0, nh = 0, nmi = 0;
    if (sscanf(next_candles_dt.c_str(), "%d-%d-%dT%d:%d", &ny, &nm, &nd, &nh, &nmi) >= 5) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s %d %s  %s", dow_name(esp_dow_from_date(ny, nm, nd)),
        nd, mon_short(nm), fmt_time_12h(next_candles_dt.c_str()).c_str());
      next_candles = buf;
    } else {
      next_candles = fmt_time_12h(next_candles_dt.c_str());
    }
  }
  // ---- Next havdalah ----
  next_havdalah = "--:--";
  if (!next_havdalah_dt.empty()) {
    int ny = 0, nm = 0, nd = 0, nh = 0, nmi = 0;
    if (sscanf(next_havdalah_dt.c_str(), "%d-%d-%dT%d:%d", &ny, &nm, &nd, &nh, &nmi) >= 5) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s %d %s  %s", dow_name(esp_dow_from_date(ny, nm, nd)),
        nd, mon_short(nm), fmt_time_12h(next_havdalah_dt.c_str()).c_str());
      next_havdalah = buf;
    } else {
      next_havdalah = fmt_time_12h(next_havdalah_dt.c_str());
    }
  }

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
  if (p_heb_display.find("\u05E4\u05E8\u05E9\u05EA") == 0) {
    // "פרשת " = 4 Hebrew letters (8 UTF-8 bytes) + 1 space = 9 bytes
    if (p_heb_display.size() > 8 && p_heb_display[8] == ' ')
      p_heb_display = p_heb_display.substr(9);
    else
      p_heb_display = p_heb_display.substr(8);
  }
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
  if (blow_shofar) {
    std::string shofar_heb = rtl("\u05EA\u05E7\u05D9\u05E2\u05D4 \u05E9\u05D1\u05E8\u05D9\u05DD \u05EA\u05E8\u05D5\u05E2\u05D4");
    shofar_status = str_sprintf("Shofar today! %s", shofar_heb.c_str());
  } else if (is_elul)
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
  const char *city_name = (geoname_idx >= 0 && geoname_idx < GEONAME_COUNT) ? GEONAME_CITIES[geoname_idx] : "Custom";
  footer = str_sprintf("%s | %s", heb_mon, city_name);
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
template<typename TDisplay, typename TFont, typename TIconFont>
static void render_shabbos(
    TDisplay &it,
    TFont *font_small,
    TFont *font_mid,
    TFont *font_hdr,
    TIconFont *font_icons,
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
    const std::string &shofar_status,
    const std::string &next_candles,
    const std::string &next_havdalah,
    int cur_hour, int cur_minute) {

  auto c_wht   = Color(255, 255, 255);
  auto c_gold  = Color(255, 200, 100);
  auto c_dim   = Color(130, 130, 130);
  auto c_div   = Color(50, 50, 40);

  if (!font_small || !font_mid || !font_icons) return;
  // Background: solid black (image disabled)
  it.filled_rectangle(0, 0, 480, 320, Color(0, 0, 0));

  // ---- Header bar (full width, 28px) ----
  auto c_dark = Color(0, 0, 0);
  it.print(240, 8, font_hdr, c_dark, TextAlign::CENTER, hdr.c_str());
  // Current wall-clock time (top-right of header)
  std::string now_s = format_12h(cur_hour, cur_minute);
  it.print(472, 8, font_hdr, c_dark, TextAlign::RIGHT, now_s.c_str());

  // ---- Vertical divider between left/right columns ----
  it.filled_rectangle(190, 28, 1, 260, c_div);

  int mx = 8, my = 34;

  // ---- Left column: halachic dates for the current day (full height) ----
  int x = mx, y = my;
  // Hebrew date (large, right-aligned)
  it.print(x + 190 - mx - 4, y, font_mid, c_wht, TextAlign::RIGHT, hebrew_date.c_str()); y += 20;
  // Today notes (Omer, Selichot — gold)
  if (!today_notes.empty()) {
    std::string::size_type pos = 0;
    while (true) {
      auto nl = today_notes.find('\n', pos);
      if (nl == today_notes.npos) {
        it.print(x, y, font_small, c_wht, today_notes.substr(pos).c_str()); y += 13; break;
      }
      it.print(x, y, font_small, c_wht, today_notes.substr(pos, nl - pos).c_str()); y += 13;
      pos = nl + 1;
    }
    y += 2;
  }
  // Shofar status (Elul/Tishrei only)
  if (!shofar_status.empty()) {
    it.print(x, y, font_small, c_wht, shofar_status.c_str()); y += 14;
  }
  // Candle lighting
  if (candles_time != "--:--") {
    int ch = 0, cm = 0; char campm[4] = "";
    if (sscanf(candles_time.c_str(), "%d:%d %3s", &ch, &cm, campm) >= 2) {
      if (campm[0] == 'P' && ch != 12) ch += 12;
      if (campm[0] == 'A' && ch == 12) ch = 0;
    }
    bool passed = (ch * 60 + cm) <= (cur_hour * 60 + cur_minute) && candles_time != "--:--";
    auto col = passed ? c_dim : c_wht;
    it.print(x, y, font_icons, col, candles_icon.c_str());
    it.print(x + 24, y, font_mid, col, candles_time.c_str()); y += 18;
  }
  // Havdalah
  if (havdalah_time != "--:--") {
    int hh = 0, hm = 0; char hampm[4] = "";
    if (sscanf(havdalah_time.c_str(), "%d:%d %3s", &hh, &hm, hampm) >= 2) {
      if (hampm[0] == 'P' && hh != 12) hh += 12;
      if (hampm[0] == 'A' && hh == 12) hh = 0;
    }
    bool passed = (hh * 60 + hm) <= (cur_hour * 60 + cur_minute);
    auto col = passed ? c_dim : c_wht;
    it.print(x, y, font_icons, col, havdalah_icon.c_str());
    it.print(x + 24, y, font_mid, col, havdalah_time.c_str()); y += 18;
  }
  // Zmanim (Mincha, Plag, Maariv) — spread evenly over remaining space
  if (!zmanim.empty()) {
    // Count lines
    int zcount = 1;
    for (size_t i = 0; i < zmanim.size(); i++) if (zmanim[i] == '\n') zcount++;
    int avail = 284 - y;  // bottom margin 4px
    int zspacing = (zcount > 1) ? (avail / zcount) : 15;
    if (zspacing < 14) zspacing = 14;
    if (zspacing > 22) zspacing = 22;
    std::string::size_type pos = 0;
    while (true) {
      auto nl = zmanim.find('\n', pos);
      std::string line = (nl == zmanim.npos) ? zmanim.substr(pos) : zmanim.substr(pos, nl - pos);
      // Determine if this time has passed
      int zh = 0, zm = 0; char zampm[4] = "";
      const char *ls = line.c_str();
      const char *colon = strchr(ls, ':');
      if (colon) {
        if (sscanf(colon + 1, " %d:%d %3s", &zh, &zm, zampm) >= 2) {
          if (zampm[0] == 'P' && zh != 12) zh += 12;
          if (zampm[0] == 'A' && zh == 12) zh = 0;
        }
      }
      bool zpassed = (zh * 60 + zm) <= (cur_hour * 60 + cur_minute) && zh >= 0 && zampm[0] != '\0';
      auto zcol = zpassed ? c_dim : c_wht;
      it.print(x, y, font_small, zcol, line.c_str()); y += zspacing;
      if (nl == zmanim.npos) break;
      pos = nl + 1;
    }
  }

  // ---- Right column: parsha, daily learning, upcoming ----
  x = 200; y = my;
  int col_w_r = 276;
  // Next candle lighting / havdalah
  if (next_candles != "--:--") {
    it.print(x, y, font_icons, c_gold, "\U000F17D3");
    it.print(x + 24, y, font_small, c_wht, next_candles.c_str()); y += 16;
  }
  if (next_havdalah != "--:--") {
    it.print(x, y, font_icons, c_gold, "\U000F059B");
    it.print(x + 24, y, font_small, c_wht, next_havdalah.c_str()); y += 16;
  }
  if (next_candles != "--:--" || next_havdalah != "--:--") y += 4;
  // Book icon + English parsha name
  it.print(x, y, font_icons, c_wht, "\U000F1CCC");
  if (!parasha_english.empty()) { it.print(x + 22, y, font_small, c_wht, parasha_english.c_str()); y += 14; }
  else y += 14;
  // Hebrew parsha name (right-aligned)
  if (!parasha_hebrew.empty()) { it.print(x + col_w_r - 4, y, font_small, c_wht, TextAlign::RIGHT, parasha_hebrew.c_str()); y += 14; }
  // Torah reading
  if (!parasha_torah.empty()) { it.print(x, y, font_small, c_wht, parasha_torah.c_str()); y += 14; }
  // Haftarah reading
  if (!parasha_haftarah.empty()) { it.print(x, y, font_small, c_wht, parasha_haftarah.c_str()); y += 14; }
  y += 6;

  // Daily learning
  if (!daily_study.empty()) {
    std::string::size_type pos = 0;
    while (true) {
      auto nl = daily_study.find('\n', pos);
      if (nl == daily_study.npos) {
        it.print(x, y, font_small, c_wht, daily_study.substr(pos).c_str()); y += 14; break;
      }
      it.print(x, y, font_small, c_wht, daily_study.substr(pos, nl - pos).c_str()); y += 14;
      pos = nl + 1;
    }
    y += 6;
  }

  // Upcoming (next 3)
  if (!upcoming.empty()) {
    std::string::size_type pos = 0;
    int _evc = 0;
    while (_evc < 3) {
      if (pos >= upcoming.size()) break;
      auto nl = upcoming.find('\n', pos);
      std::string evt = (nl == upcoming.npos) ? upcoming.substr(pos) : upcoming.substr(pos, nl - pos);
      size_t p1 = evt.find('|');
      size_t p2 = p1 != std::string::npos ? evt.find('|', p1 + 1) : std::string::npos;
      size_t p3 = p2 != std::string::npos ? evt.find('|', p2 + 1) : std::string::npos;
      std::string title   = (p1 != std::string::npos) ? evt.substr(0, p1) : evt;
      std::string hd_s    = (p2 != std::string::npos) ? evt.substr(p1 + 1, p2 - p1 - 1) : "";
      std::string heb     = (p3 != std::string::npos) ? evt.substr(p2 + 1, p3 - p2 - 1) : "";
      std::string greg_s  = (p3 != std::string::npos) ? evt.substr(p3 + 1) : "";
      std::string date_line = hd_s;
      if (!greg_s.empty()) date_line += "  " + greg_s;
      if (!date_line.empty()) { it.print(x, y, font_small, c_wht, date_line.c_str()); y += 14; }
      std::string name_line = title;
      if (!heb.empty()) name_line += "  " + rtl(heb);
      if (!name_line.empty()) { it.print(x, y, font_small, c_wht, name_line.c_str()); y += 14; }
      y += 6;
      _evc++;
      if (nl == upcoming.npos) break;
      pos = nl + 1;
    }
  }

  // ---- Footer bar (full width, 32px) ----
  it.print(240, 295, font_hdr, c_dark, TextAlign::CENTER, "Tap left: Location, right: WiFi");
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
    const std::string &input,
    bool custom_mode) {

  auto c_bg    = Color(0, 0, 0);
  auto c_gold  = Color(255, 200, 100);
  auto c_wht   = Color(255, 255, 255);
  auto c_dim   = Color(160, 160, 160);
  auto c_btn   = Color(40, 40, 80);
  auto c_sel   = Color(80, 80, 40);

  (void)bg_img;
  it.filled_rectangle(0, 0, 480, 320, Color(0, 0, 0));

  // Header
  it.filled_rectangle(0, 0, 480, 28, c_bg);
  it.print(4, 6, font_small, c_wht, "Set Location");

  if (!custom_mode) {
    // Preset city grid: 2 columns x 3 rows
    it.print(10, 32, font_small, c_dim, "Tap a city:");
    int bw = 220, bh = 55, gap = 10;
    int sx = 10, sy = 50;
    for (int i = 0; i < GEONAME_COUNT; i++) {
      int col = i % 2;
      int row = i / 2;
      int x = sx + col * (bw + gap);
      int y = sy + row * (bh + gap);
      bool sel = (GEONAME_PRESETS[i] == geoname_id);
      it.filled_rectangle(x, y, bw, bh, sel ? c_sel : c_btn);
      it.rectangle(x, y, bw, bh, sel ? c_gold : c_dim);
      // City name centered
      int tx = x + bw / 2;
      int ty = y + bh / 2 - 8;
      it.print(tx, ty, font_mid, c_wht, TextAlign::CENTER, GEONAME_CITIES[i]);
    }
    // Custom ID button
    int cy = sy + 3 * (bh + gap) + 4;
    it.filled_rectangle(10, cy, 220, 36, c_btn);
    it.rectangle(10, cy, 220, 36, c_dim);
    it.print(120, cy + 8, font_small, c_wht, TextAlign::CENTER, "Custom ID...");
    // Back button
    it.filled_rectangle(250, cy, 220, 36, c_btn);
    it.rectangle(250, cy, 220, 36, c_dim);
    it.print(360, cy + 8, font_small, c_wht, TextAlign::CENTER, "Back");
  } else {
    // Custom ID entry with keypad
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "Current ID: %d", geoname_id);
    it.print(10, 36, font_mid, c_wht, id_buf);

    it.filled_rectangle(10, 90, 460, 36, c_btn);
    std::string display_str = input.empty() ? "Enter ID..." : input;
    it.print(16, 96, font_mid, c_wht, display_str.c_str());

    it.print(10, 136, font_small, c_dim, "Find your city's GeoName ID at hebcal.com/geo");

    int kx = 20, ky = 180, kw = 100, kh = 30, gap = 10;
    const char *keys[] = {
      "1", "2", "3",
      "4", "5", "6",
      "7", "8", "9",
      "\U000F049A", "0", "OK"
    };
    for (int i = 0; i < 12; i++) {
      int col = i % 3;
      int row = i / 3;
      int x = kx + col * (kw + gap);
      int y = ky + row * (kh + gap);
      it.filled_rectangle(x, y, kw, kh, c_btn);
      it.rectangle(x, y, kw, kh, c_dim);
      int tx = x + kw / 2;
      if (i == 9) {
        it.print(tx - 16, y + 4, font_icons, c_gold, "\U000F049A");
      } else if (i == 11) {
        it.print(tx - 8, y + 5, font_mid, c_gold, "OK");
      } else {
        it.print(tx - 6, y + 5, font_mid, c_wht, keys[i]);
      }
    }
  }

  // Footer
  it.filled_rectangle(0, 287, 480, 1, c_dim);
  if (custom_mode) {
    it.filled_rectangle(40, 288, 400, 32, c_btn);
    it.rectangle(40, 288, 400, 32, c_dim);
    it.print(240, 295, font_mid, c_wht, TextAlign::CENTER, "Save");
  } else {
    it.filled_rectangle(0, 288, 480, 32, c_bg);
    it.print(240, 295, font_small, c_dim, TextAlign::CENTER, "Tap a city or Back");
  }
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

  (void)bg_img;
  it.filled_rectangle(0, 0, 480, 320, Color(0, 0, 0));

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
