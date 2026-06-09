#include <Arduino.h>  // Add this at the top
#include "touch.h"

FT6336U ts(I2C_SDA_PIN, I2C_SCL_PIN, TP_RST, TP_INT);

void initTouch() {
    Serial.println("Initializing touchscreen...");
    ts.begin();
    delay(80);
    Serial.println("Touchscreen initialized");
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint8_t touches = ts.read_td_status();

    if (touches > 0) {
        uint16_t rawX = ts.read_touch1_x();
        uint16_t rawY = ts.read_touch1_y();

        rawX = constrain(rawX, 0, screenWidth  - 1);
        rawY = constrain(rawY, 0, screenHeight - 1);

        data->state    = LV_INDEV_STATE_PR;
        data->point.x  = rawX;
        data->point.y  = rawY;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    LV_UNUSED(indev_driver);
}

void registerTouchInput() {
    Serial.println("Registering touch input...");
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
    Serial.println("Touch input registered");
}