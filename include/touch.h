#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include <FT6336U.h>
#include <lvgl.h>
#include "config.h"

// --- Touch Objects ---
extern FT6336U ts;

// --- Touch Functions ---
void initTouch();
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
void registerTouchInput();

#endif // TOUCH_H