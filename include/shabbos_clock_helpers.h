#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include "esphome/core/helpers.h"
#include "esphome/components/json/json_util.h"

#define JSON_GET(obj, key) ((obj)key | "")

// Time helpers
static void parse_time_from_iso(const char *iso, int &hour, int &minute) {
  hour = minute = -1;
  if (!iso || !*iso) return;
  sscanf(iso, "%*[^T]T%d:%d", &hour, &minute);
}

static std::string fmt_time_12h(const char *iso) {
  if (!iso || !*iso) return "--:--";
  int h = 0, m = 0;
  if (sscanf(iso, "%*[^T]T%d:%d", &h, &m) < 2) return "--:--";
  const char *ampm = (h >= 12) ? "PM" : "AM";
  int h12 = h % 12;
  if (h12 == 0) h12 = 12;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d %s", h12, m, ampm);
  return std::string(buf);
}

static const char *dow_name(int d) {
  static const char *n[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
  return n[(d - 1 + 7) % 7];
}
static const char *dow_full(int d) {
  static const char *n[] = {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};
  return n[(d - 1 + 7) % 7];
}
static const char *mon_short(int m) {
  static const char *n[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  return (m >= 1 && m <= 12) ? n[m-1] : "?";
}
static const char *mon_full(int m) {
  static const char *n[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
  return (m >= 1 && m <= 12) ? n[m-1] : "?";
}

// URL builder
static std::string build_hebcal_url(int gid, int y, int m, int d) {
  int ey = y, em = m, ed = d;
  static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  for (int i = 0; i < 14; i++) {
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

static std::string build_geoname_url(int gid) {
  return str_sprintf(
    "http://www.hebcal.com/hebcal?v=1&cfg=json&geo=geoname&geonameid=%d", gid);
}

// Parse response
static bool parse_hebcal_response(JsonDocument &doc, const std::string &body) {
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    ESP_LOGE("hebcal", "JSON error: %s", err.c_str());
    return false;
  }
  ESP_LOGI("hebcal", "Parsed %d bytes OK", (int)body.length());
  return true;
}

// Predefined geonames for touch cycling
static const int GEONAME_PRESETS[] = {2147714, 5128581, 281184, 2643743, 293397};
static const char *GEONAME_CITIES[] = {"Sydney","New York","Jerusalem","London","Tel Aviv"};
static const int GEONAME_COUNT = 5;

// Extract data from JsonDocument into formatted strings
// Returns: header, candles_time, candles_label, havdalah_time, havdalah_label,
//          hebrew_date, english_date, parasha_name, parasha_torah, parasha_haftarah,
//          upcoming_title, upcoming_date, melachot_status, shofar_status, footer
// All output via reference params.
static void extract_display_data(
    JsonDocument &doc,
    int yr, int mo, int dy, int hr, int mi, int dow,
    int geoname_idx,
    // output strings:
    std::string &header,
    std::string &candles_time, std::string &candles_label,
    std::string &havdalah_time, std::string &havdalah_label,
    std::string &hebrew_date, std::string &english_date,
    std::string &parasha_name, std::string &parasha_torah, std::string &parasha_haftarah,
    std::string &upcoming_title, std::string &upcoming_date,
    std::string &melachot_status, std::string &shofar_status,
    std::string &footer) {

  const char *city = JSON_GET(doc, ["location"]["city"]);
  const char *first_hd = JSON_GET(doc, ["items"][0]["hdate"]);

  std::string candles_dt, candles_memo, havdalah_dt;
  std::string p_name, p_torah, p_haftarah;
  std::string up_title_, up_hdate_, up_gdate_;
  std::string heb_today_hebrew;
  bool found_today = false;
  bool is_yomtov = false, is_shabbat = false, is_fast = false;
  const char *yt_name = "";
  const char *fast_name = "";

  JsonArray items = doc["items"];
  for (JsonObject obj : items) {
    const char *cat = JSON_GET(obj, ["category"]);

    if (!strcmp(cat, "candles")) {
      candles_dt = JSON_GET(obj, ["date"]);
      candles_memo = JSON_GET(obj, ["memo"]);
    } else if (!strcmp(cat, "havdalah")) {
      havdalah_dt = JSON_GET(obj, ["date"]);
    } else if (!strcmp(cat, "parashat")) {
      p_name = JSON_GET(obj, ["title"]);
      p_torah = str_sprintf("Torah: %s", JSON_GET(obj, ["leyning"]["torah"]));
      p_haftarah = str_sprintf("Haftarah: %s", JSON_GET(obj, ["leyning"]["haftarah"]));
    } else if (!strcmp(cat, "holiday")) {
      if (JSON_GET(obj, ["yomtov"]) && *JSON_GET(obj, ["yomtov"])) {
        is_yomtov = true;
        yt_name = JSON_GET(obj, ["title"]);
      } else if (!strcmp(JSON_GET(obj, ["subcat"]), "fast")) {
        is_fast = true;
        fast_name = JSON_GET(obj, ["title"]);
      }
      if (up_title_.empty() && strcmp(JSON_GET(obj, ["subcat"]), "minor")) {
        up_title_ = JSON_GET(obj, ["title"]);
        up_hdate_ = JSON_GET(obj, ["hdate"]);
        up_gdate_ = JSON_GET(obj, ["date"]);
      }
    } else if (!strcmp(cat, "hebdate")) {
      if (!found_today) {
        int y, m, d;
        if (sscanf(JSON_GET(obj, ["date"]), "%d-%d-%d", &y, &m, &d) == 3) {
          if (y == yr && m == mo && d == dy) {
            heb_today_hebrew = JSON_GET(obj, ["hebrew"]);
            found_today = true;
          }
        }
      }
    }
  }
  if (!found_today) heb_today_hebrew = JSON_GET(doc, ["items"][0]["hebrew"]);

  int wd = (dow - 1 + 7) % 7;
  int c_h = -1, c_m = -1, h_h = -1, h_m = -1;
  parse_time_from_iso(candles_dt.c_str(), c_h, c_m);
  parse_time_from_iso(havdalah_dt.c_str(), h_h, h_m);

  if (wd == 4 && c_h >= 0 && hr * 60 + mi >= c_h * 60 + c_m) is_shabbat = true;
  else if (wd == 5 && h_h >= 0 && hr * 60 + mi < h_h * 60 + h_m) is_shabbat = true;

  const char *heb_mon_ptr = strrchr(first_hd, ' ');
  const char *heb_mon = heb_mon_ptr ? heb_mon_ptr + 1 : "";
  bool is_elul = strstr(heb_mon, "Elul") || strstr(heb_mon, "\u05D0\u05DC\u05D5\u05DC");
  bool blow_shofar = is_elul && wd != 4 && wd != 5;
  bool is_tishrei = strstr(heb_mon, "Tishrei") || strstr(heb_mon, "\u05EA\u05E9\u05E8\u05D9");

  // Format outputs
  header = str_sprintf("%s | %s %d %s %d | %s",
    first_hd, dow_name(dow), dy, mon_short(mo), yr, city);

  candles_time = candles_dt.empty() ? "--:--" : fmt_time_12h(candles_dt.c_str());
  candles_label = candles_memo.empty() ? "Candle Lighting"
    : str_sprintf("Candles  %s", candles_memo.c_str());
  havdalah_time = havdalah_dt.empty() ? "--:--" : fmt_time_12h(havdalah_dt.c_str());
  havdalah_label = "Havdalah";

  hebrew_date = heb_today_hebrew + "  " + (first_hd + 3);
  english_date = str_sprintf("%s, %d %s %d", dow_full(dow), dy, mon_full(mo), yr);

  parasha_name = p_name.empty() ? "(no parasha)" : p_name;
  parasha_torah = p_torah;
  parasha_haftarah = p_haftarah;

  if (up_title_.empty()) {
    upcoming_title = "No upcoming holidays";
    upcoming_date = "in the next 2 weeks";
  } else {
    upcoming_title = up_title_;
    upcoming_date = str_sprintf("%s  %s", up_hdate_.c_str(), up_gdate_.c_str());
  }

  if (is_yomtov)
    melachot_status = str_sprintf("Yom Tov: %s", yt_name);
  else if (is_shabbat)
    melachot_status = "Shabbat \u2014 Melachot prohibited";
  else if (is_fast)
    melachot_status = str_sprintf("Fast day: %s", fast_name);
  else
    melachot_status = "Weekday \u2014 Melachot permitted";

  if (blow_shofar)
    shofar_status = "Shofar today! \u05EA\u05E7\u05D9\u05E2\u05D4 \u05E9\u05D1\u05E8\u05D9\u05DD \u05EA\u05E8\u05D5\u05E2\u05D4";
  else if (is_elul)
    shofar_status = "Shabbat \u2014 no shofar";
  else if (is_tishrei)
    shofar_status = "Tishrei \u2014 Selichot / Yomim Noraim";
  else
    shofar_status = "";

  footer = str_sprintf("%s  |  %s  |  Tap G to cycle", heb_mon, GEONAME_CITIES[geoname_idx]);
}
