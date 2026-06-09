# Waveshare35TDemo
Demo of the Waveshare 3.5" touch screen device. This demo allow you to 
access all the LVGL demos and includes a flag to change the device orientation to landscape.

You can buy it with my referral link: [Waveshare 3.5" Touch Screen](https://www.waveshare.com/esp32-s3-touch-lcd-3.5b.htm?&aff_id=117711)

<img src="https://www.waveshare.com/media/catalog/product/cache/1/image/560x560/9df78eab33525d08d6e5fb8d27136e95/e/s/esp32-s3-touch-lcd-3.5b-1.jpg" width="400" style="margin:-50px;">

## LVGL Setup Instructions

To use the built-in examples and demos of LVGL;
1) Expand folder `.pio/libdeps/Waveshare-S3-35/lvgl`
2) Move the `lvgl/demos` folder to `lvgl/src/demos`
3) Open file `lvgl/src/demos/widgets/lv_demo_widgets.h`
4) Replace the line:
   ```cpp     
   #include "../../src/draw/lv_draw.h"   
   ```
   with:
   ```cpp
   #include "../../../src/draw/lv_draw.h"
   ```
5) Replace the line:
   ```cpp
   #include "../../src/draw/lv_draw_triangle.h"
   ```
   with:
   ```cpp
   #include "../../../src/draw/lv_draw_triangle.h"
   ```

## Configuration Options

Uncomment `LANDSCAPE_MODE` to enable landscape mode (default is portrait mode)

## Pin outs

<img src="https://www.waveshare.com/img/devkit/ESP32-S3-Touch-LCD-3.5B/ESP32-S3-Touch-LCD-3.5B-details-intro.jpg" width="400">





