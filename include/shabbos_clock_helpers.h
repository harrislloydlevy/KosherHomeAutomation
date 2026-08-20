#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
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

// Reverse a string for RTL display (ESPHome prints LTR)
static std::string rtl(const std::string &s) {
  return std::string(s.rbegin(), s.rend());
}

// URL builder — 90-day window with leyning
static std::string build_hebcal_url(int gid, int y, int m, int d) {
  int ey = y, em = m, ed = d;
  static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  for (int i = 0; i < 90; i++) {
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

// Predefined geonames
static const int GEONAME_PRESETS[] = {2147714, 5128581, 281184, 2643743, 293397};
static const char *GEONAME_CITIES[] = {"Sydney","New York","Jerusalem","London","Tel Aviv"};
static const int GEONAME_COUNT = 5;

// Extract all display data from the JsonDocument
static void extract_display_data(
    JsonDocument &doc,
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
    std::string &upcoming,
    std::string &melachot_status,
    std::string &shofar_status,
    std::string &footer) {

  const char *city      = JSON_GET(doc, ["location"]["city"]);
  const char *first_hd  = JSON_GET(doc, ["items"][0]["hdate"]);

  std::string candles_dt, candles_memo, havdalah_dt;
  std::string p_name, p_hebrew, p_torah, p_haftarah;
  std::string heb_today_hebrew;
  bool found_today = false;

  bool is_yomtov = false, is_shabbat = false;
  const char *yt_name = "";

  JsonArray items = doc["items"];
  for (JsonObject obj : items) {
    const char *cat = JSON_GET(obj, ["category"]);

    if (!strcmp(cat, "candles")) {
      candles_dt = JSON_GET(obj, ["date"]);
      candles_memo = JSON_GET(obj, ["memo"]);
    } else if (!strcmp(cat, "havdalah")) {
      havdalah_dt = JSON_GET(obj, ["date"]);
    } else if (!strcmp(cat, "parashat")) {
      p_name    = JSON_GET(obj, ["title"]);
      p_hebrew  = JSON_GET(obj, ["hebrew"]);
      p_torah   = str_sprintf("Torah: %s", JSON_GET(obj, ["leyning"]["torah"]));
      p_haftarah = str_sprintf("Haftarah: %s", JSON_GET(obj, ["leyning"]["haftarah"]));
    } else if (!strcmp(cat, "holiday")) {
      if (JSON_GET(obj, ["yomtov"]) && *JSON_GET(obj, ["yomtov"])) {
        is_yomtov = true;
        yt_name = JSON_GET(obj, ["title"]);
      }
      if (!upcoming.empty()) upcoming += "\n";
      upcoming += str_sprintf("%s  %s",
        JSON_GET(obj, ["title"]), JSON_GET(obj, ["hdate"]));

    } else if (!strcmp(cat, "roshchodesh")) {
      if (!upcoming.empty()) upcoming += "\n";
      upcoming += str_sprintf("%s  %s",
        JSON_GET(obj, ["title"]), JSON_GET(obj, ["hdate"]));

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
  if (!found_today) heb_today_hebrew = JSON_GET(doc, ["items"][0]["hebrew"]);

  int wd = (dow + 5) % 7;  // 0=Mon ... 5=Sat, 6=Sun
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
  candles_icon = "\U000F17D3";  // candelabra
  havdalah_time = havdalah_dt.empty() ? "--:--" : fmt_time_12h(havdalah_dt.c_str());
  havdalah_icon = "\U000F059B";  // sunset

  std::string heb_trimmed = heb_today_hebrew;
  // Remove "of " prefix if present (e.g. "7th of Elul" → "7th Elul")
  size_t ofpos = heb_trimmed.find(" of ");
  if (ofpos != std::string::npos) heb_trimmed.replace(ofpos, 4, " ");
  hebrew_date = rtl(heb_today_hebrew);
  english_date = str_sprintf("%s, %d %s %d", dow_full(dow), dy, mon_full(mo), yr);

  // Parasha: strip "Parashat " prefix from Hebrew name, then RTL
  std::string p_heb_display = p_hebrew;
  if (p_heb_display.find("\u05E4\u05E8\u05E9\u05EA") == 0)
    p_heb_display = p_heb_display.substr(8);  // "פרשת " (5+1 space)
  parasha_hebrew = rtl(p_heb_display);
  parasha_torah  = p_torah;
  parasha_haftarah = p_haftarah;

  // Daily study — look for dafyomi, tehillim, etc.
  daily_study.clear();
  for (JsonObject obj : items) {
    const char *cat = JSON_GET(obj, ["category"]);
    if (!strcmp(cat, "dafyomi")) {
      daily_study = str_sprintf("Daf Yomi: %s", JSON_GET(obj, ["title"]));
      break;
    }
  }
  if (daily_study.empty()) {
    for (JsonObject obj : items) {
      const char *cat = JSON_GET(obj, ["category"]);
      if (!strcmp(cat, "tehillim")) {
        daily_study = str_sprintf("Tehillim: %s", JSON_GET(obj, ["title"]));
        break;
      }
    }
  }

  // Melachot
  std::string ct_display = candles_time;
  std::string ht_display = havdalah_time;
  if (is_yomtov)
    melachot_status = str_sprintf("Yom Tov: %s", yt_name);
  else if (is_shabbat)
    melachot_status = str_sprintf("Melachot prohibited from %s until %s", ct_display.c_str(), ht_display.c_str());
  else
    melachot_status = "Weekday \u2014 Melachot permitted";

  // Shofar
  if (blow_shofar)
    shofar_status = str_sprintf("Shofar today! \u05EA\u05E7\u05D9\u05E2\u05D4 \u05E9\u05D1\u05E8\u05D9\u05DD \u05EA\u05E8\u05D5\u05E2\u05D4");
  else if (is_elul)
    shofar_status = "Shabbat \u2014 no shofar";
  else if (is_tishrei)
    shofar_status = "Tishrei \u2014 Selichot / Yomim Noraim";
  else
    shofar_status = "";

  // Footer
  footer = str_sprintf("%s | %s", heb_mon, GEONAME_CITIES[geoname_idx]);
}

// Template rendering function — accepts any display/font/image types
template<typename TDisplay, typename TFont, typename TIconFont, typename TImage>
static void render_shabbos(
    TDisplay &it,
    JsonDocument &doc,
    TFont *font_small,
    TFont *font_mid,
    TIconFont *font_icons,
    TImage *bg_img,
    int yr, int mo, int dy, int hr, int mi, int dow,
    int geoname_idx) {

  std::string hdr, ct, ci, ht, hi, hd, ed, ph, pt, paf, ds, up, ms, ss, ft;
  extract_display_data(doc, yr, mo, dy, hr, mi, dow, geoname_idx,
      hdr, ct, ci, ht, hi, hd, ed, ph, pt, paf, ds, up, ms, ss, ft);

  auto c_bg    = Color(5, 5, 20);
  auto c_panel = Color(10, 10, 30);
  auto c_gold  = Color(255, 200, 100);
  auto c_wht   = Color(255, 255, 255);
  auto c_dim   = Color(160, 160, 160);
  auto c_div   = Color(50, 50, 40);

  if (bg_img) it.image(0, 0, bg_img);

  it.filled_rectangle(0, 0, 480, 28, c_bg);
  it.print(4, 6, font_small, c_wht, hdr.c_str());

  it.filled_rectangle(4, 32, 234, 252, c_panel);
  int ly = 36;
  it.print(10, ly, font_icons, c_gold, ci.c_str());
  it.print(32, ly, font_mid, c_gold, ct.c_str()); ly += 22;
  it.print(10, ly, font_icons, c_gold, hi.c_str());
  it.print(32, ly, font_mid, c_gold, ht.c_str()); ly += 24;
  it.filled_rectangle(8, ly, 226, 1, c_div); ly += 6;
  it.print(10, ly, font_small, c_dim, "Today"); ly += 16;
  it.print(10, ly, font_mid, c_wht, hd.c_str()); ly += 20;
  it.print(10, ly, font_small, c_wht, ed.c_str()); ly += 20;
  it.print(10, ly, font_small, c_gold, ms.c_str()); ly += 16;
  if (!ss.empty()) { it.print(10, ly, font_small, c_gold, ss.c_str()); ly += 16; }
  if (!ds.empty()) { it.print(10, ly, font_small, c_dim, ds.c_str()); }

  it.filled_rectangle(242, 32, 234, 252, c_panel);
  int ry = 36;
  if (!ph.empty()) { it.print(248, ry, font_mid, c_wht, ph.c_str()); ry += 22; }
  it.print(248, ry, font_small, c_wht, pt.c_str()); ry += 14;
  it.print(248, ry, font_small, c_wht, paf.c_str()); ry += 18;
  it.filled_rectangle(246, ry, 226, 1, c_div); ry += 6;
  if (!up.empty()) {
    it.print(248, ry, font_small, c_dim, "Upcoming"); ry += 14;
    it.print(248, ry, font_small, c_wht, up.c_str()); ry += 14;
  }

  it.filled_rectangle(0, 287, 480, 1, c_div);
  it.filled_rectangle(240, 32, 1, 252, c_div);
  it.filled_rectangle(0, 288, 480, 32, c_bg);
  it.print(6, 296, font_small, c_dim, ft.c_str());
  it.print(240, 296, font_small, c_dim, "Swipe L: location  R: WiFi");
}
