#ifndef STYLES_H
#define STYLES_H

#include <lvgl.h>

// --- Web UI Color Palette ---
#define COLOR_BACKGROUND  lv_color_hex(0xf7f7f7)
#define COLOR_CARD        lv_color_hex(0xffffff)
#define COLOR_BORDER      lv_color_hex(0xe5e5ea)
#define COLOR_TEXT_PRI    lv_color_hex(0x1c1c1e)
#define COLOR_TEXT_SEC    lv_color_hex(0x8e8e93)
#define COLOR_PRIMARY     lv_color_hex(0x007aff)
#define COLOR_DESTRUCTIVE lv_color_hex(0xff3b30)
#define COLOR_SUCCESS     lv_color_hex(0x34c759)
#define COLOR_HIGHLIGHT   lv_color_hex(0xf2f2f7)

// Style objects are declared here, defined in the .cpp file
extern lv_style_t style_screen;
extern lv_style_t style_card;
extern lv_style_t style_btn_primary;
extern lv_style_t style_btn_destructive;
extern lv_style_t style_title;
extern lv_style_t style_text_label;
extern lv_style_t style_text_value_large;

void apply_app_styles();

#endif // STYLES_H