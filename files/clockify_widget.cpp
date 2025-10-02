#include <lvgl.h>
#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "clockify_widget.h"
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "utils.h"
#include "fonts.h"

struct time_interval
{
    std::string start;
    std::string end;
    std::string duration;
};

struct time_entry
{
    std::string id;
    std::string description;
    std::string projectId;
    time_interval interval;
    bool has_time_interval = false;
};

struct user_data
{
    std::string user_id;
    std::string workspace_id;
    std::string time_zone;
};

struct widget_data
{
    user_data user = {};
    bool has_user_data = false;
    time_entry in_progress_entry = {};
    bool has_in_progress_entry = false;
    std::vector<time_entry> time_entries;
    bool in_progress_entry_is_loading = false;
    bool entries_list_is_loading = false;
};

lv_obj_t *clockify_widget_box;
lv_obj_t *in_progress_timer_box;
lv_obj_t *timer_list_box;
lv_obj_t *in_progress_name;
lv_obj_t *in_progress_btn_label;
lv_obj_t *in_progress_btn;
lv_obj_t *in_progress_timer;
lv_obj_t *no_timer_label;
lv_obj_t *no_timer_list_label;
lv_obj_t *in_progress_entry_spinner;
lv_obj_t *entries_list_spinner;
widget_data clockify_widget_data = {};
widget_data clockify_widget_data_prev = {};
bool init_render_clockify = true;
static TaskHandle_t create_entry_from_another_task = NULL;
static bool create_entry_from_another_in_progress = false;
static TaskHandle_t stop_in_progress_entry_task = NULL;
static bool stop_in_progress_entry_in_progress = false;
static TaskHandle_t refresh_time_entries_task = NULL;
static bool refresh_time_entries_in_progress = false;
static TaskHandle_t clockify_widget_polling_task = NULL;
static bool clockify_widget_polling_in_progress = false;
static TaskHandle_t clockify_widget_timer_task = NULL;
static bool clockify_widget_timer_in_progress = false;

const int REFRESH_CLOCKIFY_WIDGET_TIMER_FREQ_MS = 500;   // Refresh frequency in seconds
const int REFRESH_CLOCKIFY_WIDGET_POLLING_FREQ_MS = 5000; // Refresh frequency in seconds

const bool DEBUG_API_REQUESTS = true;

std::string start_time_str_to_timer(std::string *start)
{
    time_t end = get_current_utc_time();

    struct tm start_timeinfo = {0};
    if (strptime(start->c_str(), "%Y-%m-%dT%H:%M:%SZ", &start_timeinfo) == nullptr)
    {
        Serial.println("Failed to parse start time");
        return "00:00:00";
    }
    // Convert end time to string format
    struct tm *end_timeinfo = gmtime(&end);
    char end_time_buffer[32];
    strftime(end_time_buffer, sizeof(end_time_buffer), "%Y-%m-%dT%H:%M:%SZ", end_timeinfo);
    std::string current_time_str = std::string(end_time_buffer);
    return time_span_from_str(start, &current_time_str);
}

std::pair<bool, JsonDocument> send_http_request_clockify(const std::string serverEndpoint, const std::string http_method, const std::string payload="", const std::vector<std::pair<std::string, std::string>> headers={})
{
    return send_http_request(serverEndpoint, http_method, payload, {{"x-api-key", CLOCKIFY_API_KEY}}, DEBUG_API_REQUESTS);
}

std::pair<bool, user_data> request_clockify_user_info(void)
{
    const std::string serverEndpoint = "https://api.clockify.me/api/v1/user";
    auto [doc_valid, doc] = send_http_request_clockify(serverEndpoint, "GET");

    if (!doc_valid)
    {
        return {false, {}};
    }

    user_data user = {
        .user_id = (std::string)doc["id"].as<std::string>(),
        .workspace_id = (std::string)doc["activeWorkspace"].as<std::string>(),
        .time_zone = (std::string)doc["settings"]["timeZone"].as<std::string>(),
    };
    return {true, user};
}

std::pair<bool, std::vector<time_entry>> request_clockify_time_entries()
{
    if (clockify_widget_data.has_user_data == false)
    {
        Serial.println("No user data, cannot request time entries");
        return {false, {}};
    }

    user_data *user = &clockify_widget_data.user;
    const std::string serverEndpoint = (String("https://api.clockify.me/api/v1/workspaces/") + user->workspace_id.c_str() + String("/user/") + user->user_id.c_str() + String("/time-entries?in-progress=false&page-size=5")).c_str();
    auto [doc_valid, doc] = send_http_request_clockify(serverEndpoint, "GET");
    if (!doc_valid)
    {
        return {false, {}};
    }

    std::vector<time_entry> entries;
    for (JsonObject entry : doc.as<JsonArray>())
    {
        time_entry te = {
            .id = (std::string)entry["id"].as<std::string>(),
            .description = (std::string)entry["description"].as<std::string>(),
            .projectId = entry["projectId"] ? (std::string)entry["projectId"].as<std::string>() : "",
            .interval = {
                .start = entry["timeInterval"]["start"] ? (std::string)entry["timeInterval"]["start"].as<std::string>() : "",
                .end = entry["timeInterval"]["end"] ? (std::string)entry["timeInterval"]["end"].as<std::string>() : "",
                .duration = entry["timeInterval"]["duration"] ? (std::string)entry["timeInterval"]["duration"].as<std::string>() : "",
            },
        };
        entries.push_back(te);
    }
    return {true, entries};
}

std::pair<bool, time_entry> request_clockify_in_progress_entry()
{
    if (clockify_widget_data.has_user_data == false)
    {
        Serial.println("No user data, cannot request in progress entry");
        return {false, {}};
    }

    user_data *user = &clockify_widget_data.user;
    const std::string serverEndpoint = (String("https://api.clockify.me/api/v1/workspaces/") + user->workspace_id.c_str() + String("/user/") + user->user_id.c_str() + String("/time-entries?in-progress=true&page-size=1")).c_str();
    auto [doc_valid, doc] = send_http_request_clockify(serverEndpoint, "GET");
    if (!doc_valid)
    {
        return {false, {}};
    }

    std::vector<time_entry> entries;
    for (JsonObject entry : doc.as<JsonArray>())
    {
        time_entry te = {
            .id = (std::string)entry["id"].as<std::string>(),
            .description = (std::string)entry["description"].as<std::string>(),
            .projectId = entry["projectId"] ? (std::string)entry["projectId"].as<std::string>() : "",
            .interval = {
                .start = entry["timeInterval"]["start"] ? (std::string)entry["timeInterval"]["start"].as<std::string>() : "",
                .end = entry["timeInterval"]["end"] ? (std::string)entry["timeInterval"]["end"].as<std::string>() : "",
                .duration = entry["timeInterval"]["duration"] ? (std::string)entry["timeInterval"]["duration"].as<std::string>() : "",
            },
        };
        entries.push_back(te);
    }

    if (entries.size() > 0)
    {
        return {true, entries[0]};
    }
    else
    {
        return {false, {}};
    }
}

bool request_clockify_stop_in_progress_entry()
{
    if (clockify_widget_data.has_user_data == false)
    {
        Serial.println("No user data, cannot request stop in progress entry");
        return false;
    }

    user_data *user = &clockify_widget_data.user;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5000))
    {
        Serial.println("Failed to obtain time");
        throw std::invalid_argument("Failed to obtain time");
    }

    char end_time_str[30];
    strftime(end_time_str, sizeof(end_time_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    String patch_payload = "{\"end\": \"" + String(end_time_str) + "\"}";

    const std::string serverEndpoint = (String("https://api.clockify.me/api/v1/workspaces/") + user->workspace_id.c_str() + String("/user/") + user->user_id.c_str() + String("/time-entries")).c_str();
    auto [doc_valid, doc] = send_http_request_clockify(serverEndpoint, "PATCH", patch_payload.c_str());
    if (!doc_valid)
    {
        return false;
    }
    return true;
}

bool request_clockify_create_entry_from_another(time_entry *from_entry)
{
    if (clockify_widget_data.has_user_data == false)
    {
        Serial.println("No user data, cannot request create entry from another");
        return false;
    }

    user_data *user = &clockify_widget_data.user;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5000))
    {
        Serial.println("Failed to obtain time");
        throw std::invalid_argument("Failed to obtain time");
    }

    char start_time_str[30];
    strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    String patch_payload = "{";
    patch_payload += "\"description\": \"" + String(from_entry->description.c_str()) + "\",";
    patch_payload += "\"start\": \"" + String(start_time_str) + "\",";
    if (from_entry->projectId.c_str() != "")
    {
        patch_payload += "\"projectId\": \"" + String(from_entry->projectId.c_str()) + "\",";
    }
    patch_payload += "\"type\": \"REGULAR\"";
    patch_payload += "}";

    const std::string serverEndpoint = (String("https://api.clockify.me/api/v1/workspaces/") + user->workspace_id.c_str() + String("/user/") + user->user_id.c_str() + String("/time-entries")).c_str();
    auto [doc_valid, doc] = send_http_request_clockify(serverEndpoint, "POST", patch_payload.c_str());
    if (!doc_valid)
    {
        return false;
    }
    return true;
}

bool set_clockify_widget_data_user_data()
{
    auto [user_info_flag, user] = request_clockify_user_info();
    if (user_info_flag)
    {
        clockify_widget_data.user.time_zone = user.time_zone;
        clockify_widget_data.user.workspace_id = user.workspace_id;
        clockify_widget_data.user.user_id = user.user_id;
        clockify_widget_data.has_user_data = true;
        return true;
    }
    else
    {
        clockify_widget_data.has_user_data = false;
        return false;
    }
}

bool set_clockify_widget_data_time_entries()
{
    if (clockify_widget_data.has_user_data == false)
    {
        set_clockify_widget_data_user_data();
    }

    auto [time_entries_flag, time_entries] = request_clockify_time_entries();
    if (time_entries_flag)
    {
        clockify_widget_data.time_entries = time_entries;
        return true;
    }
    else
    {
        clockify_widget_data.time_entries = {};
        return false;
    }
}

bool set_clockify_widget_data_in_progress_entry()
{
    if (clockify_widget_data.has_user_data == false)
    {
        set_clockify_widget_data_user_data();
    }

    auto [in_progress_entry_flag, in_progress_entry] = request_clockify_in_progress_entry();
    if (in_progress_entry_flag)
    {
        clockify_widget_data.in_progress_entry = in_progress_entry;
        clockify_widget_data.has_in_progress_entry = true;
        return true;
    }
    else
    {
        clockify_widget_data.has_in_progress_entry = false;
        clockify_widget_data.in_progress_entry = {};
        return false;
    }
}

static void on_stop_timer_btn_click(lv_event_t *e); // Predeclaration
static void on_play_timer_btn_click(lv_event_t *e); // Predeclaration
bool is_clockify_widget_data_changed_time_entries(); // Predeclaration

void render_clockify_widget_box(lv_obj_t *parent)
{
    if (clockify_widget_box == nullptr)
    {
        lv_lock();
        clockify_widget_box = create_lv_div(parent);
        lv_obj_set_size(clockify_widget_box, lv_pct(100), lv_pct(100));
        lv_obj_set_align(clockify_widget_box, LV_ALIGN_RIGHT_MID);
        lv_obj_set_style_radius(clockify_widget_box, 16, LV_PART_MAIN);
        lv_obj_set_style_bg_color(clockify_widget_box, lv_color_hex(0xe4eaee), LV_PART_MAIN);
        lv_obj_set_layout(clockify_widget_box, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(clockify_widget_box, LV_FLEX_FLOW_COLUMN);     // Display Flex column
        lv_obj_set_style_pad_all(clockify_widget_box, 5, LV_PART_MAIN);    // Padding
        lv_unlock();
    }
}

void render_in_progress_box_entry()
{
    lv_lock();
    if ((clockify_widget_data.has_in_progress_entry || clockify_widget_data.in_progress_entry_is_loading) && in_progress_timer_box == nullptr) // not rendered yet
    {
        in_progress_timer_box = create_lv_div(clockify_widget_box);
        lv_obj_move_to_index(in_progress_timer_box, 0);
        lv_obj_set_width(in_progress_timer_box, lv_pct(100));
        lv_obj_set_style_radius(in_progress_timer_box, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_color(in_progress_timer_box, lv_color_hex(0xffffff), LV_PART_MAIN);
    }

    if (!clockify_widget_data.has_in_progress_entry && clockify_widget_data.in_progress_entry_is_loading && in_progress_timer_box != nullptr)
    {
        Serial.println("Rendering in progress entry spinner");
        lv_obj_clean(in_progress_timer_box);
        in_progress_name = nullptr;
        in_progress_timer = nullptr;
        in_progress_btn = nullptr;
        in_progress_btn_label = nullptr;
    
        in_progress_entry_spinner = lv_spinner_create(in_progress_timer_box);
        lv_obj_set_size(in_progress_entry_spinner, 80, 80);
        lv_obj_center(in_progress_entry_spinner);
        lv_spinner_set_anim_params(in_progress_entry_spinner, 1500, 200);
    }

    if (clockify_widget_data.has_in_progress_entry && in_progress_name == nullptr && in_progress_timer_box != nullptr)
    {
        Serial.println("Rendering in progress box entry");
        lv_obj_clean(in_progress_timer_box); // Delete all children from the box
        in_progress_entry_spinner = nullptr;
        
        lv_obj_t *in_progress_entry_box = create_lv_div(in_progress_timer_box);
        lv_obj_set_width(in_progress_entry_box, lv_pct(100));
        lv_obj_set_flex_flow(in_progress_entry_box, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_flex_main_place(in_progress_entry_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_PART_MAIN);
        lv_obj_set_style_flex_cross_place(in_progress_entry_box, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_pad_left(in_progress_entry_box, 10, LV_PART_MAIN);


        lv_obj_t *in_progress_entry_box_left = create_lv_div(in_progress_entry_box);
        lv_obj_set_flex_grow(in_progress_entry_box_left, 1);
        lv_obj_set_flex_flow(in_progress_entry_box_left, LV_FLEX_FLOW_COLUMN);

        in_progress_name = lv_label_create(in_progress_entry_box_left);
        lv_label_set_text(in_progress_name, clockify_widget_data.in_progress_entry.description.c_str());
        lv_label_set_long_mode(in_progress_name, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_style_text_font(in_progress_name, &arial_20, 0);
        lv_obj_set_style_text_color(in_progress_name, lv_color_hex(0x0a0e10), LV_PART_MAIN);

        in_progress_timer = lv_label_create(in_progress_entry_box_left);
        lv_label_set_text(in_progress_timer, start_time_str_to_timer(&clockify_widget_data.in_progress_entry.interval.start).c_str());
        lv_obj_set_style_text_font(in_progress_timer, &lv_font_montserrat_26, 0);
        lv_obj_set_style_text_color(in_progress_timer, lv_color_hex(0x0a0e10), LV_PART_MAIN);

        in_progress_btn = lv_btn_create(in_progress_entry_box);
        lv_obj_add_event_cb(in_progress_btn, on_stop_timer_btn_click, LV_EVENT_PRESSED, &clockify_widget_data.in_progress_entry);
        lv_obj_set_size(in_progress_btn, 80, 80);
        lv_obj_set_style_bg_color(in_progress_btn, lv_color_hex(0xf44336), LV_PART_MAIN);

        in_progress_btn_label = lv_label_create(in_progress_btn);
        lv_obj_center(in_progress_btn_label);
        lv_label_set_text(in_progress_btn_label, LV_SYMBOL_STOP);
        lv_obj_set_style_text_font(in_progress_btn_label, &lv_font_montserrat_26, 0);
        lv_obj_add_flag(in_progress_btn_label, LV_OBJ_FLAG_EVENT_BUBBLE); // Bubble event
    }   

    // when in progress entry stopped, but still timer box
    if (!clockify_widget_data.has_in_progress_entry && clockify_widget_data.in_progress_entry_is_loading == false && in_progress_timer_box != nullptr)
    {
        lv_obj_del(in_progress_timer_box); // Delete the box
        in_progress_timer_box = nullptr;
        in_progress_entry_spinner = nullptr;
        in_progress_name = nullptr;
        in_progress_timer = nullptr;
        in_progress_btn = nullptr;
        in_progress_btn_label = nullptr;

        lv_obj_move_to_index(timer_list_box, 0);
        lv_obj_scroll_to_y(timer_list_box, 0, LV_ANIM_OFF);
    }

    lv_unlock();
}

void render_timer_entries_list_box()
{
    lv_lock();
    if (timer_list_box == nullptr) // not rendered yet
    {
        timer_list_box = create_lv_div(clockify_widget_box);
        lv_obj_set_width(timer_list_box, lv_pct(100));
        lv_obj_set_flex_grow(timer_list_box, 1);
        lv_obj_set_flex_flow(timer_list_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(timer_list_box, lv_color_hex(0xe4eaee), LV_PART_MAIN);
        lv_obj_set_style_pad_column(timer_list_box, 2, LV_PART_MAIN); // Flex gap
        lv_obj_set_flex_align(timer_list_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_flag(timer_list_box, LV_OBJ_FLAG_SCROLLABLE, true);
    }
 
    if (clockify_widget_data.entries_list_is_loading && timer_list_box != nullptr)
    {
        Serial.println("Rendering entries list spinner");
        lv_obj_clean(timer_list_box); // Delete all children from the box
        no_timer_list_label = nullptr;
        lv_obj_set_flex_align(timer_list_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        entries_list_spinner = lv_spinner_create(timer_list_box);
        lv_obj_set_size(entries_list_spinner, 80, 80);
        lv_spinner_set_anim_params(entries_list_spinner, 1500, 200);
    }

    if (clockify_widget_data.entries_list_is_loading == false && clockify_widget_data.time_entries.size() > 0 && timer_list_box != nullptr && is_clockify_widget_data_changed_time_entries())
    {
        Serial.println("Rendering timer entries list box");
        lv_obj_clean(timer_list_box); // Delete all children from the box
        lv_obj_set_flex_align(timer_list_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        no_timer_list_label = nullptr;
        entries_list_spinner = nullptr;

        uint32_t i;
        int size = clockify_widget_data.time_entries.size() > 10?10:clockify_widget_data.time_entries.size();
        for (i = 0; i < size; i++)
        {
            time_entry *entry = &clockify_widget_data.time_entries[i];

            lv_obj_t *timer_list_item_box = create_lv_div(timer_list_box);
            lv_obj_set_size(timer_list_item_box, lv_pct(100), 80);
            lv_obj_set_style_bg_color(timer_list_item_box, lv_color_hex(0xffffff), LV_PART_MAIN);
            lv_obj_set_flex_flow(timer_list_item_box, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_flex_main_place(timer_list_item_box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_PART_MAIN);
            lv_obj_set_style_flex_cross_place(timer_list_item_box, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_style_pad_left(timer_list_item_box, 10, LV_PART_MAIN);

            lv_obj_t *list_item_left_box = create_lv_div(timer_list_item_box);
            lv_obj_set_flex_grow(list_item_left_box, 1);
            lv_obj_set_flex_flow(list_item_left_box, LV_FLEX_FLOW_COLUMN);

            lv_obj_t *list_item_name_label = lv_label_create(list_item_left_box);
            lv_label_set_text(list_item_name_label, entry->description.c_str());
            lv_label_set_long_mode(list_item_name_label, LV_LABEL_LONG_MODE_DOTS); /*Break the long lines*/
            lv_obj_set_style_text_font(list_item_name_label, &arial_20, 0);
            lv_obj_set_style_text_color(list_item_name_label, lv_color_hex(0x0a0e10), LV_PART_MAIN);
  
            lv_obj_t *list_item_time_span_label = lv_label_create(list_item_left_box);
            lv_label_set_text(list_item_time_span_label, time_span_from_str(&entry->interval.start, &entry->interval.end).c_str());
            lv_obj_set_style_text_font(list_item_time_span_label, &arial_20, 0);
            lv_obj_set_style_text_color(list_item_time_span_label, lv_color_hex(0x0a0e10), LV_PART_MAIN);

            lv_obj_t *list_item_btn = lv_btn_create(timer_list_item_box);
            lv_obj_add_event_cb(list_item_btn, on_play_timer_btn_click, LV_EVENT_PRESSED, &clockify_widget_data.time_entries[i]);
            lv_obj_set_style_bg_color(list_item_btn, lv_color_hex(0x03a9f4), LV_PART_MAIN);
            lv_obj_set_size(list_item_btn, 80, 80);

            lv_obj_t *list_item_btn_label = lv_label_create(list_item_btn);
            lv_obj_add_flag(list_item_btn_label, LV_OBJ_FLAG_EVENT_BUBBLE); // Bubble event
            lv_obj_center(list_item_btn_label);
            lv_label_set_text(list_item_btn_label, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_font(list_item_btn_label, &lv_font_montserrat_26, 0);
        }
    }
    
    if (clockify_widget_data.entries_list_is_loading == false &&clockify_widget_data.time_entries.size() == 0 && timer_list_box != nullptr && no_timer_list_label == nullptr)
    {
        lv_obj_clean(timer_list_box); // Delete all children from the box
        entries_list_spinner = nullptr;
        lv_obj_set_flex_align(timer_list_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        no_timer_list_label = lv_label_create(timer_list_box);
        lv_label_set_text(no_timer_list_label, "No timer history");
        lv_obj_align(no_timer_list_label, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_text_font(no_timer_list_label, &lv_font_montserrat_26, 0);
    }
    lv_unlock();
}

bool clockify_widget_polling_update(void)
{
    return set_clockify_widget_data_in_progress_entry();
}

bool clockify_widget_timer_update(void)
{
    if (clockify_widget_data.has_in_progress_entry && in_progress_timer != nullptr)
    {
        lv_lock();
        lv_label_set_text(in_progress_timer, start_time_str_to_timer(&clockify_widget_data.in_progress_entry.interval.start).c_str());
        lv_unlock();
        return true;
    }
    return false;
}

void clockify_widget_timer_task_func(void *parameter)
{
    while (true)
    {
        clockify_widget_timer_update();
        vTaskDelay(pdMS_TO_TICKS(REFRESH_CLOCKIFY_WIDGET_TIMER_FREQ_MS));
    }
}

void clockify_widget_polling_task_func(void *parameter)
{
    while (true)
    {
        clockify_widget_polling_update();
        vTaskDelay(pdMS_TO_TICKS(REFRESH_CLOCKIFY_WIDGET_POLLING_FREQ_MS));
    }
}

static void refresh_time_entries_task_func(void *parameter)
{
    clockify_widget_data.entries_list_is_loading = true;
    set_clockify_widget_data_time_entries();
    clockify_widget_data.entries_list_is_loading = false;

    refresh_time_entries_in_progress = false;
    refresh_time_entries_task = NULL;
    vTaskDelete(NULL);
}

static void create_entry_from_another_task_func(void *parameter)
{
    time_entry *entry = (time_entry *)parameter;
    Serial.printf("Play entry id %s name %s\n", entry->id.c_str(), entry->description.c_str());
    request_clockify_create_entry_from_another(entry);
    // task will pick up the new in progress entry

    bool success = set_clockify_widget_data_in_progress_entry();
    clockify_widget_data.in_progress_entry_is_loading = false;

    create_entry_from_another_in_progress = false;
    create_entry_from_another_task = NULL;
    vTaskDelete(NULL);
}

static void stop_in_progress_entry_task_func(void *parameter)
{
    time_entry *entry = (time_entry *)parameter;
    Serial.printf("Stop entry %s\n", entry->id.c_str());
    
    bool success = request_clockify_stop_in_progress_entry();

    if (success)
    {
        set_clockify_widget_data_in_progress_entry();    
    }

    stop_in_progress_entry_in_progress = false;
    stop_in_progress_entry_task = NULL;
    vTaskDelete(NULL);
}

void start_clockify_widget_tasks(void)
{
    if (!clockify_widget_polling_in_progress) {
        clockify_widget_polling_in_progress = true;
        xTaskCreate(clockify_widget_polling_task_func, "ClockifyWidgetPolling", 8192, NULL, 1, &clockify_widget_polling_task);
    }
    if (!clockify_widget_timer_in_progress) {
        clockify_widget_timer_in_progress = true;
        xTaskCreate(clockify_widget_timer_task_func, "ClockifyWidgetTimer", 8192, NULL, 1, &clockify_widget_timer_task);
    }
}

void stop_clockify_widget_tasks(void)
{
    if (clockify_widget_polling_in_progress) {
        clockify_widget_polling_in_progress = false;
        vTaskDelete(clockify_widget_polling_task);
        clockify_widget_polling_task = NULL;
    }
    if (clockify_widget_timer_in_progress) {
        clockify_widget_timer_in_progress = false;
        vTaskDelete(clockify_widget_timer_task);
        clockify_widget_timer_task = NULL;
    }
}

static void on_stop_timer_btn_click(lv_event_t *e)
{
    time_entry *entry = (time_entry *)lv_event_get_user_data(e);
    if (!stop_in_progress_entry_in_progress) {
        stop_in_progress_entry_in_progress = true;
        xTaskCreate(stop_in_progress_entry_task_func, "StopInProgressEntry", 8192, entry, 1, &stop_in_progress_entry_task);
    }
}

static void on_play_timer_btn_click(lv_event_t *e)
{
    if (clockify_widget_data.has_in_progress_entry)
    {
        Serial.println("In progress entry, cannot create entry from another");
        return;
    }
    time_entry *entry = (time_entry *)lv_event_get_user_data(e);
    if (!create_entry_from_another_in_progress) {
        create_entry_from_another_in_progress = true;
        clockify_widget_data.in_progress_entry_is_loading = true;
        xTaskCreate(create_entry_from_another_task_func, "CreateEntryFromAnother", 8192, entry, 1, &create_entry_from_another_task);
    }
}

bool is_clockify_widget_data_changed_time_entries()
{
    auto *current = &clockify_widget_data;
    auto *prev = &clockify_widget_data_prev;

     // Compare time entries vector
     if (current->time_entries.size() != prev->time_entries.size())
     {
         return true;
     }
 
     for (size_t i = 0; i < current->time_entries.size(); i++)
     {
         const time_entry &current_entry = current->time_entries[i];
         const time_entry &prev_entry = prev->time_entries[i];
 
         if (current_entry.id != prev_entry.id ||
             current_entry.description != prev_entry.description ||
             current_entry.projectId != prev_entry.projectId ||
             current_entry.has_time_interval != prev_entry.has_time_interval)
         {
             return true;
         }
 
         if (current_entry.has_time_interval)
         {
             if (current_entry.interval.start != prev_entry.interval.start ||
                 current_entry.interval.end != prev_entry.interval.end ||
                 current_entry.interval.duration != prev_entry.interval.duration)
             {
                 return true;
             }
         }
     }
     return false;
}

bool is_clockify_widget_data_changed_in_progress_entry()
{
    auto *current = &clockify_widget_data;
    auto *prev = &clockify_widget_data_prev;

    if (current->has_in_progress_entry != prev->has_in_progress_entry)
    {
        return true;
    }

    if (current->has_in_progress_entry)
    {
        if (current->in_progress_entry.id != prev->in_progress_entry.id ||
            current->in_progress_entry.description != prev->in_progress_entry.description ||
            current->in_progress_entry.projectId != prev->in_progress_entry.projectId ||
            current->in_progress_entry.has_time_interval != prev->in_progress_entry.has_time_interval)
        {
            return true;
        }

        if (current->in_progress_entry.has_time_interval)
        {
            if (current->in_progress_entry.interval.start != prev->in_progress_entry.interval.start ||
                current->in_progress_entry.interval.end != prev->in_progress_entry.interval.end ||
                current->in_progress_entry.interval.duration != prev->in_progress_entry.interval.duration)
            {
                return true;
            }
        }
    }

    return false;
}

bool is_clockify_widget_data_changed()
{
    auto *current = &clockify_widget_data;
    auto *prev = &clockify_widget_data_prev;

    if (current->in_progress_entry_is_loading != prev->in_progress_entry_is_loading)
    {
        return true;
    }

    if (current->entries_list_is_loading != prev->entries_list_is_loading)
    {
        return true;
    }

    // Compare user data
    if (current->has_user_data != prev->has_user_data ||
        current->user.user_id != prev->user.user_id ||
        current->user.workspace_id != prev->user.workspace_id ||
        current->user.time_zone != prev->user.time_zone)
    {
        return true;
    }

    if (is_clockify_widget_data_changed_in_progress_entry())
    {
        return true;
    }
    
    if (is_clockify_widget_data_changed_time_entries())
    {
        return true;
    }

    return false;
}

extern "C" void render_clockify_widget(lv_obj_t *parent)
{
    if (init_render_clockify)
    {
        Serial.println("Initializing clockify widget");
        if (parent == nullptr)
        {
            parent = lv_screen_active();
        }
        render_clockify_widget_box(parent);

        if (!refresh_time_entries_in_progress) {
            refresh_time_entries_in_progress = true;
            xTaskCreate(refresh_time_entries_task_func, "RefreshTimeEntries", 8192, NULL, 1, &refresh_time_entries_task);
        }
       
        init_render_clockify = false;
    }

    if (is_clockify_widget_data_changed() == false)
    {
        return;
    }
    Serial.println("Clockify widget data changed");

    // If progress entry is removed -> refresh time entries list
    if(is_clockify_widget_data_changed_in_progress_entry() && clockify_widget_data.has_in_progress_entry == false)
    {
        if (!refresh_time_entries_in_progress) {
            refresh_time_entries_in_progress = true;
            xTaskCreate(refresh_time_entries_task_func, "RefreshTimeEntries", 8192, NULL, 1, &refresh_time_entries_task);
        }
    }

    render_in_progress_box_entry();
    render_timer_entries_list_box();

    clockify_widget_data_prev = clockify_widget_data;
}
