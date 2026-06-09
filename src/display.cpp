#include <Arduino.h>  // Add this at the top
#include "display.h"

// --- Display Objects ---
Arduino_ESP32SPI bus(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, -1);
Arduino_ST7796 tft(&bus, -1, 0, true, 320, 480, 0, 0, 0, false);

// --- LVGL Buffers ---
lv_disp_draw_buf_t draw_buf;
lv_color_t lvgl_buf1[screenWidth * LVGL_BUFFER_LINES];
lv_color_t lvgl_buf2[screenWidth * LVGL_BUFFER_LINES];

void initDisplay() {
    Serial.println("Initializing display...");
    // Run the SPI bus at 80 MHz (Arduino_GFX default is 40 MHz). This roughly
    // halves the time to push a frame to the ST7796 — the single biggest win
    // for scroll smoothness and chart repaint. If the panel shows artifacts
    // (snow, torn pixels) on your wiring, step down to 60000000 or 40000000.
    tft.begin(80000000);
    tft.invertDisplay(false);
    tft.fillScreen(0x0000);
    delay(120);
    Serial.println("Display initialized");
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.writeAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((uint16_t *)color_p, w * h);
    tft.endWrite();

    lv_disp_flush_ready(disp);
    LV_UNUSED(disp);
}

void initLVGL() {
    Serial.println("Initializing LVGL...");
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, lvgl_buf1, lvgl_buf2,
                          screenWidth * LVGL_BUFFER_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = screenWidth;
    disp_drv.ver_res  = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    Serial.println("LVGL display driver registered");
}