// Tools -> PSRAM -> Enabled
// Tools -> Partition Scheme -> Huge App

// PCM5102MK
// BCK -> 27
// DATA -> 25
// LRCK -> 26
// GND -> GND
// VCC -> 5V

#ifndef BOARD_HAS_PSRAM
#error "This program requires PSRAM enabled"
#endif

#include <Arduino.h>
#include <WiFi.h>
#include "Audio.h"
#include "gt911.h"
#include "displays_config.h"

// =====================
// Touch
// =====================
static esp_lcd_touch_handle_t tp_handle = NULL;
#define MAX_TOUCH_POINTS 5
static uint16_t touch_x[MAX_TOUCH_POINTS];
static uint16_t touch_y[MAX_TOUCH_POINTS];
static uint16_t touch_strength[MAX_TOUCH_POINTS];
static uint8_t touch_cnt = 0;
static bool touch_pressed = false;

// =====================
// I2S Pins
// =====================
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC  26

// =====================
// WiFi creds
// =====================
String ssid = "SSID";
String password = "PASSWORD";

// =====================
// Audio
// =====================
Audio audio;
String currentStation = "RADIO 538";
String currentInfo = "Starting...";
const int maxVolume = 5; // 5 for now 21 = max

// Station list
struct Station {
  const char* name;
  const char* url;
};
Station stations[] = {
  {"RADIO 538", "https://playerservices.streamtheworld.com/api/livestream-redirect/RADIO538AAC.aac"},
  {"SLAM", "https://playerservices.streamtheworld.com/api/livestream-redirect/SLAM_AAC.aac"}
};
const int numStations = sizeof(stations)/sizeof(stations[0]);

// =====================
// Display setup
// =====================
Arduino_ESP32DSIPanel *dsipanel = new Arduino_ESP32DSIPanel(
  display_cfg.hsync_pulse_width,
  display_cfg.hsync_back_porch,
  display_cfg.hsync_front_porch,
  display_cfg.vsync_pulse_width,
  display_cfg.vsync_back_porch,
  display_cfg.vsync_front_porch,
  display_cfg.prefer_speed,
  display_cfg.lane_bit_rate);

Arduino_DSI_Display *gfx = new Arduino_DSI_Display(
  display_cfg.width,
  display_cfg.height,
  dsipanel,
  0,
  true,
  -1,
  display_cfg.init_cmds,
  display_cfg.init_cmds_size);

// =====================
// Draw GUI
// =====================
void drawUI() {
  gfx->fillScreen(RGB565_BLACK);

  int cx = 400, cy = 400; // center of 800x800 round display

  // Header
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(cx-120, 40);
  gfx->println("ESP32 Radio");

  // Current Station
  gfx->setTextColor(RGB565_CYAN);
  gfx->setTextSize(3);
  gfx->setCursor(cx-80, cy-20);
  gfx->println(currentStation);

  // Now Playing
  gfx->setTextColor(RGB565_YELLOW);
  gfx->setTextSize(2);
  gfx->setCursor(cx-140, cy+40);
  gfx->println(currentInfo);

  // Circular buttons
  int r = 200; // radius from center
  for (int i = 0; i < numStations; i++) {
    float angle = i * 3.1415 * 2 / numStations - 3.1415/2; // spread around top
    int bx = cx + r * cos(angle);
    int by = cy + r * sin(angle);

    // Button circle
    gfx->fillCircle(bx, by, 60, RGB565_BLUE);
    gfx->drawCircle(bx, by, 60, RGB565_WHITE);

    // Button text
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(bx-30, by-10);
    gfx->println(stations[i].name);
  }
}

// =====================
// Audio callback
// =====================
void my_audio_info(Audio::msg_t m) {
  Serial.printf("%s: %s\n", m.s, m.msg);
  currentInfo = String(m.msg);
  drawUI();
}

// =====================
// Touch handling
// =====================
void handleTouch() {
  if (!tp_handle) return;

  esp_lcd_touch_read_data(tp_handle);
  touch_pressed = esp_lcd_touch_get_coordinates(
      tp_handle, touch_x, touch_y, touch_strength, &touch_cnt, MAX_TOUCH_POINTS);

  if (touch_pressed && touch_cnt > 0) {
    int tx = touch_x[0];
    int ty = touch_y[0];

    // Check which station button was touched
    int cx = 400, cy = 400;
    int r = 200;
    for (int i = 0; i < numStations; i++) {
      float angle = i * 3.1415 * 2 / numStations - 3.1415/2;
      int bx = cx + r * cos(angle);
      int by = cy + r * sin(angle);

      int dx = tx - bx;
      int dy = ty - by;
      int dist = sqrt(dx*dx + dy*dy);
      if (dist < 60) {
        // Switch station
        currentStation = stations[i].name;
        audio.connecttohost(stations[i].url);
        audio.setVolume(maxVolume);
        drawUI();
        break;
      }
    }
  }
}

// =====================
// Setup
// =====================
void setup() {
  Serial.begin(115200);

  // Init Display
  DEV_I2C_Port port = DEV_I2C_Init();
  display_init(port);
  set_display_backlight(port, 255);
  delay(1000);

  // Touch
  tp_handle = touch_gt911_init(port);

  // Gfx
  if (!gfx->begin()) Serial.println("Display init failed!");
  gfx->fillScreen(RGB565_BLACK);

  gfx->setCursor(50,20);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(2);
  gfx->println("Connecting WiFi...");

  // WiFi
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) delay(500);

  // Audio
  Audio::audio_info_callback = my_audio_info;
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(maxVolume);

  audio.connecttohost(stations[0].url); // Start with RADIO 538

  drawUI();
}

// =====================
// Loop
// =====================
void loop() {
  audio.loop();
  handleTouch();
  vTaskDelay(1);
}


