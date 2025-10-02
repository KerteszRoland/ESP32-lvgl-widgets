#pragma once

#include <lvgl.h>
#include <string>
#include <vector>
#include <ctime>
#include <ArduinoJson.h>

lv_obj_t* create_lv_div(lv_obj_t* parent);
time_t get_current_utc_time(void);
std::string time_span_from_str(std::string *start, std::string *end);
std::string round_float_to_string(float number, int digits);
std::pair<bool, JsonDocument> send_http_request(const std::string serverEndpoint, const std::string http_method, const std::string payload="", const std::vector<std::pair<std::string, std::string>> headers={}, bool debug_api_requests=false);
