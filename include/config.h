#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- WiFi Configuration ---
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

// --- I2C pins for Waveshare display with WMCU ESP32-S3 devkit ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 7

//#define IO_EXPANDER_ADDR 0x20
#define LCD_RST 4
//#define EXPANDER_PIN_LCD_RST 10

// --- Waveshare ESP32 with 3.5" integrated touch display pins ---
// #define TFT_MOSI 1
// #define TFT_SCLK 5
// #define TFT_DC   3
// #define TFT_CS   12
// #define TFT_BL   6

// --- WMCU ESP32-S3 to display pins ---
#define TFT_MOSI 1
#define TFT_SCLK 5
#define TFT_DC   3
#define TFT_CS   39
#define TFT_BL   6

#define TP_RST 11
#define TP_INT 10

#define MAX6675_CLK 38
#define MAX6675_DO  21// 48 in personal
#define MAX6675_CS  16

#define SSR1_PIN 17
#define SSR2_PIN 18

// --- Screen & LVGL Settings ---
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 480;
#define LVGL_BUFFER_LINES 60   // larger draw buffer → fewer SPI flush round-trips (was 40)
#define LVGL_TICK_PERIOD_MS 5

#endif