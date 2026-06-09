#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "config.h"

// --- Display Objects --
extern Arduino_ESP32SPI bus;
extern Arduino_ST7796 tft;

// --- LVGL Buffers ---
extern lv_disp_draw_buf_t draw_buf;
extern lv_color_t lvgl_buf1[];
extern lv_color_t lvgl_buf2[];
extern uint32_t lvgl_last_tick;

// --- Display Functions ---
void initDisplay();
void initLVGL();
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);

#endif // DISPLAY_H