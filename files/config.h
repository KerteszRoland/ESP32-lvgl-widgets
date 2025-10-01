#pragma once

// check (required) parameters passed from the ini
// create your own private_config.ini with the data. See private_config.example.ini
#ifndef CLOCKIFY_API_KEY
#error "Clockify api key missing"
#endif
#ifndef WIFI_SSID
#error "Wifi ssid missing"
#endif
#ifndef WIFI_PASSWORD
#error "Wifi password missing"
#endif
#ifndef STOCK_API_KEY
#error "Stock api key missing"
#endif