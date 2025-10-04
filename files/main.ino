#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include "stock_widget.h"
#include "clockify_widget.h"
#include "linkedin_widget.h"
#include <WiFi.h>
#include "time.h"
#include "config.h"

LilyGo_Class amoled;

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

lv_obj_t *tileview = nullptr;
lv_obj_t *tile_stock = nullptr;
lv_obj_t *tile_clockify = nullptr;
lv_obj_t *tile_linkedin = nullptr;

void setup()
{
    Serial.begin(115200);

    delay(1000); // Give serial monitor time to connect
    Serial.println("Starting setup...");

    bool rslt = false;

    // Begin LilyGo  1.91 Inch AMOLED board class
    rslt = amoled.beginAMOLED_191();

    if (!rslt)
    {
        while (1)
        {
            Serial.println("The board model cannot be detected, please raise the Core Debug Level to an error");
            delay(1000);
        }
    }

    WiFi.begin(ssid, password);
    Serial.println("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo, 5000))
    {
        Serial.println("Failed to obtain time");
    }
    Serial.println(&timeinfo, "Current UTC time: %A, %B %d %Y %H:%M:%S");
    beginLvglHelper(amoled);

    tileview = lv_tileview_create(lv_screen_active());
    lv_obj_set_style_bg_color(tileview, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

    tile_linkedin = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    lv_obj_set_style_pad_all(tile_linkedin, 10, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(tile_linkedin, LV_SCROLLBAR_MODE_OFF);
    render_linkedin_widget(tile_linkedin);

    tile_stock = lv_tileview_add_tile(tileview, 1, 0, (lv_dir_t)(LV_DIR_RIGHT | LV_DIR_LEFT));
    lv_obj_set_style_pad_all(tile_stock, 10, LV_PART_MAIN);
    render_stock_widget(tile_stock);

    tile_clockify = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT);
    lv_obj_set_style_pad_all(tile_clockify, 10, LV_PART_MAIN);
}

void loop()
{
    if (tileview != nullptr && lv_tileview_get_tile_active(tileview) == tile_clockify)
    {
        render_clockify_widget(tile_clockify);
        start_clockify_widget_tasks();
    }
    else
    {
        stop_clockify_widget_tasks();
    }

    lv_task_handler();
    delay(5);
}
