#include <lvgl.h>
#include <time.h>
#include <string>
#include "utils.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

lv_obj_t* create_lv_div(lv_obj_t* parent)
{
    lv_obj_t* div = lv_obj_create(parent);
    lv_obj_set_style_pad_all(div, 0, LV_PART_MAIN);
    lv_obj_set_style_margin_all(div, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);
    lv_obj_set_style_min_height(div, 0, LV_PART_MAIN);
    lv_obj_set_style_min_width(div, 0, LV_PART_MAIN);
    lv_obj_set_height(div, LV_SIZE_CONTENT);
    lv_obj_set_scrollbar_mode(div, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
    return div;
}

time_t get_current_utc_time(void)
{
    time_t timeinfo;
    time(&timeinfo);
    return timeinfo;
}

std::string time_span_from_str(std::string *start, std::string *end)
{
    struct tm start_timeinfo = {0};
    if (strptime(start->c_str(), "%Y-%m-%dT%H:%M:%SZ", &start_timeinfo) == nullptr)
    {
        Serial.println("Failed to parse start time");
        return "00:00:00";
    }

    struct tm end_timeinfo = {0};
    if (strptime(end->c_str(), "%Y-%m-%dT%H:%M:%SZ", &end_timeinfo) == nullptr)
    {
        Serial.println("Failed to parse start time");
        return "00:00:00";
    }


    // Convert both times to time_t for proper calculation
    time_t start_time = mktime(&start_timeinfo);
    time_t end_time = mktime(&end_timeinfo);

    // Calculate difference in seconds
    double diff_seconds_total = difftime(end_time, start_time);

    if (diff_seconds_total < 0)
    {
        Serial.printf("End time: %s\n", ctime(&end_time));
        Serial.printf("Start time: %s\n", ctime(&start_time));
        Serial.printf("Diff seconds total: %f\n", diff_seconds_total);
        Serial.println("Negative time!");
        return "00:00:00"; // Handle negative time (shouldn't happen in normal use)
    }

    int total_seconds = (int)diff_seconds_total;
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    // Format with zero padding
    char timer_buffer[16];
    snprintf(timer_buffer, sizeof(timer_buffer), "%02d:%02d:%02d", hours, minutes, seconds);

    return std::string(timer_buffer);
}

std::string round_float_to_string(float number, int digits)
{
  // Round the number to the specified number of decimal places
  float multiplier = pow(10.0, digits);
  float rounded = round(number * multiplier) / multiplier;

  // Convert to string with proper precision
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.*f", digits, rounded);

  std::string number_str(buffer);
  return number_str;
}

std::pair<bool, JsonDocument> send_http_request(const std::string serverEndpoint, const std::string http_method, const std::string payload, const std::vector<std::pair<std::string, std::string>> headers, bool debug_api_requests)
{
    // Check WiFi connection status
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi not connected!");
        return {false, JsonDocument()};
    }

    HTTPClient http;
    http.begin(serverEndpoint.c_str());
    http.addHeader("Content-Type", "application/json");
    for (const auto &header : headers)
    {
        http.addHeader(header.first.c_str(), header.second.c_str());
    }

    int httpResponseCode = http.sendRequest(http_method.c_str(), payload.c_str());
    if (debug_api_requests)
    {
        Serial.printf("%s, %s\n", http_method.c_str(), serverEndpoint.c_str());
    }
    if (httpResponseCode > 0)
    {
        if (debug_api_requests)
        {
            Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        }
        String payload = http.getString();

        if (debug_api_requests)
        {
            //Serial.printf("Payload: %s\n", payload.c_str());
        }
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error)
        {
            Serial.printf("JSON parsing failed: %s\n", error.c_str());
            http.end();
            return {false, JsonDocument()};
        }

        if (doc.isNull())
        {
            Serial.println("doc is null!");
            http.end();
            return {false, JsonDocument()};
        }

        http.end();
        return {true, doc};
    }
    else
    {
        Serial.printf("HTTP Error code: %d\n", httpResponseCode);
        http.end();
        return {false, JsonDocument()};
    }
}