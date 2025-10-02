#include <lvgl.h>
#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "stock_widget.h"
#include "config.h"
#include "utils.h"

struct stock_month_chart_data_item
{
  std::string date;
  float open_price;
  float close_price;
};

struct widget_data
{
  std::string stock_ticker;
  std::string company_name;
  float price;
  float percent_change;
  float dollar_change;
  std::vector<stock_month_chart_data_item> month_chart_data;
  std::vector<int32_t> display_chart_data;
  bool price_positive;
  bool is_loading = false;
};

widget_data stock_widget_data;

lv_obj_t *stock_widget_box;
lv_obj_t *latest_price_label;
lv_obj_t *percent_change_label;
lv_obj_t *dollar_change_label;
lv_obj_t *chart;
lv_obj_t *stock_ticker_arrow_label;
lv_obj_t *stock_ticker_label;
lv_obj_t *company_name_label;

const std::string STOCK_TICKER = "TSLA";
const bool DEBUG_API_REQUESTS = true;

std::string get_month_ago_utc_time_str(void)
{
  time_t timeinfo;
  time(&timeinfo);

  time_t current_time = timeinfo;
  current_time -= 30 * 24 * 60 * 60;
  char time_str[30];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d", gmtime(&current_time));
  return std::string(time_str);
}

std::pair<bool, JsonDocument> send_http_request_stock(const std::string serverEndpoint, const std::string http_method, const std::string payload="")
{
  return send_http_request(serverEndpoint, http_method, payload,{}, DEBUG_API_REQUESTS);
}

std::pair<bool, std::vector<stock_month_chart_data_item>> request_stock_info(void)
{
  std::string time_str = get_month_ago_utc_time_str();
  if (DEBUG_API_REQUESTS)
  {
    Serial.printf("Time: %s\n", time_str.c_str());
  }

  std::string serverEndpoint = (String("https://financialmodelingprep.com/stable/historical-price-eod/full?") +
                                "symbol=" + String(STOCK_TICKER.c_str()) + "&apikey=" + String(STOCK_API_KEY) + "&from=" + String(time_str.c_str()))
                                   .c_str();

  auto [doc_valid, doc] = send_http_request_stock(serverEndpoint, "GET");

  if (!doc_valid)
  {
    return {false, {}};
  }

  std::vector<stock_month_chart_data_item> items;
  for (JsonObject entry : doc.as<JsonArray>())
  {
    items.push_back({
        .date = entry["date"] ? (std::string)entry["date"].as<std::string>() : "",
        .open_price = entry["open"] ? entry["open"].as<float>() : 0.0f,
        .close_price = entry["close"] ? entry["close"].as<float>() : 0.0f,
    });
  }
  return {true, items};
}

std::pair<bool, std::vector<stock_month_chart_data_item>> request_stock_info_dummy(void)
{
  if (DEBUG_API_REQUESTS)
  {
    Serial.println("request_stock_info_dummy");
  }
  std::string dummy_json_str = "[{\"symbol\":\"TSLA\",\"date\":\"2025-10-01\",\"open\":443.8,\"high\":462.29,\"low\":440.75,\"close\":459.46,\"volume\":97498810,\"change\":15.66,\"changePercent\":3.53,\"vwap\":451.575},{\"symbol\":\"TSLA\",\"date\":\"2025-09-30\",\"open\":441.52,\"high\":445,\"low\":433.12,\"close\":444.72,\"volume\":74358000,\"change\":3.2,\"changePercent\":0.72477,\"vwap\":441.09},{\"symbol\":\"TSLA\",\"date\":\"2025-09-29\",\"open\":444.35,\"high\":450.98,\"low\":439.5,\"close\":443.21,\"volume\":79491510,\"change\":-1.14,\"changePercent\":-0.25655,\"vwap\":444.51},{\"symbol\":\"TSLA\",\"date\":\"2025-09-26\",\"open\":428.3,\"high\":440.47,\"low\":421.02,\"close\":440.4,\"volume\":101628200,\"change\":12.1,\"changePercent\":2.83,\"vwap\":432.5475},{\"symbol\":\"TSLA\",\"date\":\"2025-09-25\",\"open\":435.24,\"high\":435.35,\"low\":419.08,\"close\":423.39,\"volume\":96746426,\"change\":-11.85,\"changePercent\":-2.72,\"vwap\":428.265},{\"symbol\":\"TSLA\",\"date\":\"2025-09-24\",\"open\":429.83,\"high\":444.21,\"low\":429.03,\"close\":442.79,\"volume\":93133600,\"change\":12.96,\"changePercent\":3.02,\"vwap\":436.465},{\"symbol\":\"TSLA\",\"date\":\"2025-09-23\",\"open\":439.88,\"high\":440.97,\"low\":423.72,\"close\":425.85,\"volume\":83422700,\"change\":-14.03,\"changePercent\":-3.19,\"vwap\":432.605},{\"symbol\":\"TSLA\",\"date\":\"2025-09-22\",\"open\":431.11,\"high\":444.98,\"low\":429.13,\"close\":434.21,\"volume\":97108800,\"change\":3.1,\"changePercent\":0.71907,\"vwap\":434.8575},{\"symbol\":\"TSLA\",\"date\":\"2025-09-19\",\"open\":421.82,\"high\":429.47,\"low\":421.72,\"close\":426.07,\"volume\":93131034,\"change\":4.25,\"changePercent\":1.01,\"vwap\":424.77},{\"symbol\":\"TSLA\",\"date\":\"2025-09-18\",\"open\":428.87,\"high\":432.22,\"low\":416.56,\"close\":416.85,\"volume\":90454509,\"change\":-12.01,\"changePercent\":-2.8,\"vwap\":423.625},{\"symbol\":\"TSLA\",\"date\":\"2025-09-17\",\"open\":415.75,\"high\":428.31,\"low\":409.67,\"close\":425.86,\"volume\":106133532,\"change\":10.11,\"changePercent\":2.43,\"vwap\":419.8975},{\"symbol\":\"TSLA\",\"date\":\"2025-09-16\",\"open\":414.5,\"high\":423.25,\"low\":411.43,\"close\":421.62,\"volume\":104285721,\"change\":7.13,\"changePercent\":1.72,\"vwap\":417.7},{\"symbol\":\"TSLA\",\"date\":\"2025-09-15\",\"open\":423.13,\"high\":425.7,\"low\":402.43,\"close\":410.04,\"volume\":163823700,\"change\":-13.09,\"changePercent\":-3.09,\"vwap\":415.325},{\"symbol\":\"TSLA\",\"date\":\"2025-09-12\",\"open\":370.94,\"high\":396.69,\"low\":370.24,\"close\":395.94,\"volume\":168156400,\"change\":25,\"changePercent\":6.74,\"vwap\":383.4525},{\"symbol\":\"TSLA\",\"date\":\"2025-09-11\",\"open\":350.17,\"high\":368.99,\"low\":347.6,\"close\":368.81,\"volume\":103756010,\"change\":18.64,\"changePercent\":5.32,\"vwap\":358.8925},{\"symbol\":\"TSLA\",\"date\":\"2025-09-10\",\"open\":350.55,\"high\":356.33,\"low\":346.07,\"close\":347.79,\"volume\":72121700,\"change\":-2.76,\"changePercent\":-0.78733,\"vwap\":350.185},{\"symbol\":\"TSLA\",\"date\":\"2025-09-09\",\"open\":348.44,\"high\":350.77,\"low\":343.82,\"close\":346.97,\"volume\":53816000,\"change\":-1.47,\"changePercent\":-0.42188,\"vwap\":347.5},{\"symbol\":\"TSLA\",\"date\":\"2025-09-08\",\"open\":354.64,\"high\":358.44,\"low\":344.84,\"close\":346.4,\"volume\":75208300,\"change\":-8.24,\"changePercent\":-2.32,\"vwap\":351.08},{\"symbol\":\"TSLA\",\"date\":\"2025-09-05\",\"open\":348,\"high\":355.87,\"low\":344.68,\"close\":350.84,\"volume\":108989800,\"change\":2.84,\"changePercent\":0.81609,\"vwap\":349.8475},{\"symbol\":\"TSLA\",\"date\":\"2025-09-04\",\"open\":336.15,\"high\":338.89,\"low\":331.48,\"close\":338.53,\"volume\":60711033,\"change\":2.38,\"changePercent\":0.70802,\"vwap\":336.2625},{\"symbol\":\"TSLA\",\"date\":\"2025-09-03\",\"open\":335.2,\"high\":343.33,\"low\":328.51,\"close\":334.09,\"volume\":88733300,\"change\":-1.11,\"changePercent\":-0.33115,\"vwap\":335.2825},{\"symbol\":\"TSLA\",\"date\":\"2025-09-02\",\"open\":328.23,\"high\":333.33,\"low\":325.6,\"close\":329.36,\"volume\":58392000,\"change\":1.13,\"changePercent\":0.34427,\"vwap\":329.13}]";

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, dummy_json_str);

  if (error)
  {
    Serial.printf("JSON parsing failed: %s\n", error.c_str());
    return {false, {}};
  }

  std::vector<stock_month_chart_data_item> items;
  for (JsonObject entry : doc.as<JsonArray>())
  {
    items.push_back({
        .date = entry["date"] ? (std::string)entry["date"].as<std::string>() : "",
        .open_price = entry["open"] ? entry["open"].as<float>() : 0.0f,
        .close_price = entry["close"] ? entry["close"].as<float>() : 0.0f,
    });
  }
  return {true, items};
}

bool load_stock_widget_data(void)
{
  auto [is_items_valid, items] = request_stock_info();
  // auto [is_items_valid, items] = request_stock_info_dummy(); // dummy data
  if (!is_items_valid)
  {
    Serial.println("load_stock_widget_data failed");
    return false;
  }

  std::vector<int32_t> display_chart_data = {};
  for (int i = items.size() - 1; i >= 0; i--)
  {
    display_chart_data.push_back((int32_t)items[i].open_price);
    display_chart_data.push_back((int32_t)items[i].close_price);
  }
  float latest_price = items[0].close_price;
  float previous_close_price = items[items.size() - 1].close_price;

  stock_widget_data.stock_ticker = STOCK_TICKER;
  stock_widget_data.company_name = "Tesla, Inc.";
  stock_widget_data.price = latest_price;
  stock_widget_data.dollar_change = latest_price - previous_close_price;
  stock_widget_data.percent_change = ((previous_close_price != 0.0f) ? (stock_widget_data.dollar_change / previous_close_price) * 100.0f : 0.0f);
  stock_widget_data.month_chart_data = items;
  stock_widget_data.display_chart_data = display_chart_data;
  stock_widget_data.price_positive = (latest_price >= previous_close_price);
  stock_widget_data.is_loading = false;
  return true;
}

void init_render_stock_widget(lv_obj_t *parent)
{
  std::string ticker = stock_widget_data.stock_ticker;
  std::string company_name = stock_widget_data.company_name;
  float price = stock_widget_data.price;
  float percent_change = stock_widget_data.percent_change;
  float dollar_change = stock_widget_data.dollar_change;
  std::vector<int32_t> *chart_data = &stock_widget_data.display_chart_data;
  bool price_positive = stock_widget_data.price_positive;

  auto primary_color = (price_positive) ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED);

  // Create a container stock_widget_box
  stock_widget_box = lv_obj_create(parent);
  lv_obj_set_size(stock_widget_box, 220, 220);
  lv_obj_set_align(stock_widget_box, LV_ALIGN_CENTER);
  lv_obj_set_style_radius(stock_widget_box, 16, LV_PART_MAIN);
  lv_obj_set_style_bg_color(stock_widget_box, lv_palette_darken(LV_PALETTE_GREY, 4), LV_PART_MAIN);
  lv_obj_set_style_border_width(stock_widget_box, 0, LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(stock_widget_box, LV_SCROLLBAR_MODE_OFF); // No scrollbars
  lv_obj_clear_flag(stock_widget_box, LV_OBJ_FLAG_SCROLLABLE);        // Disable scrolling

  // Chart setup
  chart = lv_chart_create(stock_widget_box);
  lv_obj_add_flag(chart, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_size(chart, 200, 90);
  lv_obj_center(chart);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_div_line_count(chart, 0, 0);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR); // No point circles
  lv_obj_set_style_width(chart, 5, LV_PART_ITEMS);       // Line width
  lv_obj_set_style_bg_color(chart, lv_palette_darken(LV_PALETTE_GREY, 4), LV_PART_MAIN);
  lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
  lv_chart_set_point_count(chart, chart_data->size());

  lv_chart_series_t *ser1 = lv_chart_add_series(chart, primary_color, LV_CHART_AXIS_PRIMARY_Y);

  if (!chart_data->empty())
  {
    auto minmax = std::minmax_element(chart_data->begin(), chart_data->end());
    lv_coord_t min = *minmax.first;
    lv_coord_t max = *minmax.second;
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
    for (size_t i = 0; i < chart_data->size(); ++i)
    {
      lv_chart_set_next_value(chart, ser1, (*chart_data)[i]);
    }
  }

  // Display stock ticker trend arrow
  stock_ticker_arrow_label = lv_label_create(stock_widget_box);
  lv_obj_add_flag(stock_ticker_arrow_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_label_set_text(stock_ticker_arrow_label, price_positive ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
  lv_obj_set_style_text_color(stock_ticker_arrow_label, primary_color, 0);
  lv_obj_set_style_text_font(stock_ticker_arrow_label, &lv_font_montserrat_14, 0);
  lv_obj_align(stock_ticker_arrow_label, LV_ALIGN_TOP_LEFT, 0, 5);

  // Display stock ticker
  stock_ticker_label = lv_label_create(stock_widget_box);
  lv_obj_add_flag(stock_ticker_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_label_set_text(stock_ticker_label, ticker.c_str());
  lv_obj_set_style_text_font(stock_ticker_label, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(stock_ticker_label, lv_color_white(), 0);
  lv_obj_align(stock_ticker_label, LV_ALIGN_TOP_LEFT, 20, 0);

  // Display company name
  company_name_label = lv_label_create(stock_widget_box);
  lv_obj_add_flag(company_name_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_label_set_text(company_name_label, company_name.c_str());
  lv_obj_set_style_text_font(company_name_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(company_name_label, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_obj_align(company_name_label, LV_ALIGN_TOP_LEFT, 0, 30);

  // Display latest price
  latest_price_label = lv_label_create(stock_widget_box);
  lv_obj_add_flag(latest_price_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_label_set_text(latest_price_label, round_float_to_string(price, 2).c_str());
  lv_obj_set_style_text_font(latest_price_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(latest_price_label, lv_color_white(), 0);
  lv_obj_align(latest_price_label, LV_ALIGN_BOTTOM_RIGHT, 0, 10);

  // Display percent change
  percent_change_label = lv_label_create(stock_widget_box);
  lv_obj_add_flag(percent_change_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_label_set_text_fmt(percent_change_label, percent_change >= 0 ? "+%s%%" : "%s%%", round_float_to_string(percent_change, 2).c_str());
  lv_obj_set_style_text_color(percent_change_label, primary_color, 0);
  lv_obj_set_style_text_font(percent_change_label, &lv_font_montserrat_20, 0);
  lv_obj_align(percent_change_label, LV_ALIGN_TOP_RIGHT, 0, 0);

  // Display dollar change
  dollar_change_label = lv_label_create(stock_widget_box);
  lv_obj_add_flag(dollar_change_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_label_set_text_fmt(dollar_change_label, dollar_change >= 0 ? "+%s" : "%s", round_float_to_string(dollar_change, 2).c_str());
  lv_obj_set_style_text_color(dollar_change_label, primary_color, 0);
  lv_obj_set_style_text_font(dollar_change_label, &lv_font_montserrat_20, 0);
  lv_obj_align(dollar_change_label, LV_ALIGN_TOP_RIGHT, 0, 30);
}

extern "C" void render_stock_widget(lv_obj_t *parent)
{
  load_stock_widget_data();
  init_render_stock_widget(parent);
}