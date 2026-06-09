#include <Arduino.h>
#include "esp_system.h"
#include "config.h"
#include "hardware.h"
#include "display.h"
#include "touch.h"
#include "reflow_logic.h"
#include "styles.h"
#include "ui.h"
#include "web_server.h"

uint32_t lvgl_last_tick = 0;
uint32_t lvgl_last_tick2 = 0;
uint32_t now;
uint32_t lvgl_task_handler_cb_period = 0;

void setup() {
    // Drive SSR pins LOW immediately on boot — before anything else
    // Prevents optocoupler leakage current during boot/init sequence
    pinMode(SSR1_PIN, OUTPUT);
    pinMode(SSR2_PIN, OUTPUT);
    digitalWrite(SSR1_PIN, LOW);
    digitalWrite(SSR2_PIN, LOW);

    // On cold power-on the USB peripheral can be left in an indeterminate
    // state. A software restart fixes it; subsequent boots come up as
    // ESP_RST_SW / ESP_RST_EXT so this only fires once per power cycle.
    if (esp_reset_reason() == ESP_RST_POWERON) {
        delay(200);
        esp_restart();
    }

    Serial.begin(115200);
    while (!Serial && millis() < 2000) {}
    Serial.println("=== Reflow Oven Controller Starting ===");

    initI2C();
    initTouch();
    // initExpander();
    initBacklight();
    customLcdReset();
    reflow_init();
    initDisplay();
    initLVGL();
    apply_app_styles(); // Must be after initLVGL()
    registerTouchInput();
    show_boot_screen(); // splash screen while rest of system inits
    create_ui();
    web_server_init();

    Serial.println("=== Setup Complete. System Ready. ===");
}

void loop() {    
    now = millis();

    if (now - lvgl_last_tick >= LVGL_TICK_PERIOD_MS) {
        lv_tick_inc(now - lvgl_last_tick);
        lvgl_last_tick = now;
    }

    if (now - lvgl_last_tick2 > lvgl_task_handler_cb_period) {
        lvgl_task_handler_cb_period = lv_task_handler();
        lvgl_last_tick2 = now;
    }

    reflow_loop();
    web_server_loop();

    delay(1); // yield to RTOS; lower than before to keep up with the faster refresh/touch cadence
}