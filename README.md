# ESP32 LVGL Widgets

A feature-rich ESP32 project for the LilyGo T-Display AMOLED board, featuring interactive widgets for stock tracking and [Clockify](https://clockify.me) time management using [LVGL](https://github.com/lvgl/lvgl).

## Features

### 📈 Stock Widget

- Stock price tracking using Financial Modeling Prep API
- 30-day price history chart with visual trends
- Percentage and dollar change indicators
- Color-coded price movements (green for gains, red for losses)
- Currently configured for Tesla (TSLA) stock

### ⏱️ Clockify Widget

- Integration with Clockify time tracking API
- Start/stop timers directly from the device
- View your time entry history
- Live timer display for in-progress tasks
- Quick-start timers from previous entries
- Automatic data synchronization

## Hardware Requirements

- **Board**: [LilyGo T-Display S3 AMOLED](https://lilygo.cc/products/t-display-s3-amoled?variant=43532279939253) (1.91 inch, with touch)

## Prerequisites

- [PlatformIO](https://platformio.org/) installed
- Clockify API Free key (get from [Clockify](https://clockify.me/))
- Financial Modeling Prep API Free key (get from [FMP](https://financialmodelingprep.com/))
- WiFi credentials

## Setup

### 1. Clone the Repository

```bash
git clone https://github.com/KerteszRoland/ESP32-lvgl-widgets
cd ESP32-lvgl-widgets
```

### 2. Configure API Keys

Create a `private_config.ini` file in the root directory based on the example:

```bash
cp "private_config example.ini" private_config.ini
```

Edit `private_config.ini` and add your credentials:

### 3. Install Dependencies

PlatformIO will automatically install the required dependencies listed in `platformio.ini`:

- LVGL v9.3
- TFT_eSPI
- ArduinoJson
- XPowersLib
- And more...

### 4. Build and Upload

```bash
# Build the project
pio run

# Upload to the board
pio run --target upload

# Monitor serial output
pio device monitor
```

Or use PlatformIO IDE's built-in buttons.

## Usage

### Navigation

The interface uses a tileview with swipe navigation:

- **Home Screen (Tile 0)**: Stock widget - swipe left to access Clockify
- **Clockify Screen (Tile 1)**: Time tracking widget - swipe right to return

### Stock Widget

The stock widget automatically:

- Fetches the latest price data on startup
- Displays current price with trend indicators
- Shows 30-day price history chart
- Show price movements (percentage and dollar change)

To track a different stock, modify the `STOCK_TICKER` constant in `files/stock_widget.cpp`:

```cpp
const std::string STOCK_TICKER = "TSLA"; // Change to your preferred ticker
```

### Clockify Widget

- **View Active Timer**: If a timer is running, it displays at the top with a stop button
- **Timer History**: Scroll through your recent time entries
- **Start Timer**: Tap the play button on any previous entry to start a new timer based on that entry
- **Stop Timer**: Tap the stop button to end the current timer

## Project Structure

```
ESP32-lvgl-widgets/
├── boards/
│   └── T-Display-AMOLED.json      # Board definition
├── files/                          # Main source directory
│   ├── main.ino                    # Main application entry point
│   ├── config.h                    # Configuration validation
│   ├── stock_widget.cpp/.h         # Stock widget implementation
│   ├── clockify_widget.cpp/.h      # Clockify widget implementation
│   └── src/
│       ├── arial_20.c              # Custom font
│       └── arial_26.c              # Custom font
├── src/                            # Library source files
│   ├── LilyGo_AMOLED.cpp/.h        # Board-specific driver
│   ├── LV_Helper.cpp/.h            # LVGL helper functions
│   ├── lv_conf.h                   # LVGL configuration
│   └── ...
├── platformio.ini                  # PlatformIO configuration
├── private_config.ini              # Your private credentials (gitignored)
└── private_config example.ini      # Template for credentials
```

## Customization

### Adding New Widgets

1. Create widget header and implementation files in `files/`
2. Include the widget in `main.ino`
3. Add a new tile to the tileview:

```cpp
lv_obj_t * tile_new = lv_tileview_add_tile(tileview, x, y, direction);
render_your_widget(tile_new);
```

### Changing Stock Ticker

Edit `STOCK_TICKER` in `files/stock_widget.cpp`:

```cpp
const std::string STOCK_TICKER = "AAPL"; // Apple
// or
const std::string STOCK_TICKER = "GOOGL"; // Google
```

### Adjusting Polling Intervals

In `files/clockify_widget.cpp`:

```cpp
const int REFRESH_CLOCKIFY_WIDGET_TIMER_FREQ_MS = 500;   // Timer update frequency
const int REFRESH_CLOCKIFY_WIDGET_POLLING_FREQ_MS = 5000; // API polling frequency
```

## Development

### Debug Mode

Enable debug output by setting in `files/stock_widget.cpp` and `files/clockify_widget.cpp`:

```cpp
const bool DEBUG_API_REQUESTS = true;
```

Monitor serial output at 115200 baud.

## Dependencies

| Library                                                       | Version | Purpose            |
| ------------------------------------------------------------- | ------- | ------------------ |
| mikalhart/TinyGPSPlus                                         | 1.0.3   | GPS data parsing   |
| adafruit/Adafruit NeoPixel                                    | 1.11.0  | LED control        |
| bxparks/AceButton                                             | 1.10.1  | Button handling    |
| lvgl/lvgl                                                     | 9.3     | Graphics library   |
| lewisxhe/XPowersLib                                           | 0.2.7   | Power management   |
| lewisxhe/SensorLib                                            | 0.2.4   | Sensor interfacing |
| bodmer/TFT_eSPI                                               | 2.5.31  | Display driver     |
| FS                                                            | -       | File system        |
| SPIFFS                                                        | -       | Flash file system  |
| SD                                                            | -       | SD card support    |
| sparkfun/SparkFun MAX3010x Pulse and Proximity Sensor Library | ^1.1.2  | Heart rate sensor  |
| paulstoffregen/OneWire                                        | ^2.3.8  | OneWire protocol   |
| bblanchon/ArduinoJson                                         | ^7.4.2  | JSON parsing       |

## License

[MIT License](https://github.com/KerteszRoland/ESP32-lvgl-widgets/blob/master/LICENSE)

## Credits

- LilyGo for the T-Display AMOLED hardware and [git repo](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series/tree/master)
- LVGL team for the graphics library
- Financial Modeling Prep for stock data API
- Clockify for time tracking API
