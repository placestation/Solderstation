# Solderstation

An open-source reflow oven controller.

The firmware in this repository runs on an ESP32-S3 with a 3.5" touch display
(PlatformIO + Arduino framework + LVGL): PID temperature control, selectable
reflow profiles, a live temperature graph, manual heater control, and a WiFi
web interface.

## Buy one

You can purchase the reflow oven here:
**https://www.placestation.in/products/reflow-oven?variant=55802498679153**

## Build & flash

This is a [PlatformIO](https://platformio.org/) project.

```bash
pio run                 # build
pio run -t upload       # flash to the board
pio device monitor      # serial console (115200 baud)
```

Target environment: `esp32s3_devkit` (see `platformio.ini`).
