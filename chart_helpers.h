#pragma once

#include "esphome.h"
#include "lvgl.h"
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib> // for atof

static lv_obj_t *weight_chart = nullptr;
static lv_chart_series_t *weight_series = nullptr;

static void draw_weight_chart(lv_obj_t *parent) {
  if (weight_chart == nullptr) {
    weight_chart = lv_chart_create(parent);
    lv_obj_set_size(weight_chart, 275, 105);
    lv_obj_set_style_bg_color(weight_chart, lv_color_hex(0x111827), LV_PART_MAIN);
    lv_chart_set_type(weight_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(weight_chart, 20);
    lv_obj_set_style_line_color(weight_chart, lv_color_hex(0x4B5563), LV_PART_ITEMS);
    weight_series = lv_chart_add_series(weight_chart, lv_color_hex(0x10B981), LV_CHART_AXIS_PRIMARY_Y);
  }
}

static void update_weight_chart(const std::string &data_str) {
  if (weight_chart == nullptr) return;
  
  // Parse the comma-separated string into float values
  std::vector<float> values;
  std::stringstream ss(data_str);
  std::string item;
  
  while (std::getline(ss, item, ',')) {
    // Manual conversion without exceptions
    if (!item.empty()) {
      char *endptr;
      float val = std::atof(item.c_str());
      // Check if conversion was successful (basic check)
      if (endptr != item.c_str()) {
        values.push_back(val);
      }
    }
  }
  
  if (values.empty()) return;
  
  // Limit to the chart's point count
  int point_count = 20;
  int data_size = values.size();
  
  // Clear existing data
  for (int i = 0; i < point_count; i++) {
    lv_chart_set_next_value(weight_chart, weight_series, LV_CHART_POINT_NONE);
  }
  
  // Add new data points (show most recent values)
  int start_index = (data_size > point_count) ? (data_size - point_count) : 0;
  for (int i = start_index; i < data_size; i++) {
    lv_chart_set_next_value(weight_chart, weight_series, (lv_coord_t)values[i]);
  }
  
  lv_obj_invalidate(weight_chart);
}
