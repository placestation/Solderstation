#include "styles.h"

// --- Style Objects (Definitions) ---
lv_style_t style_screen;
lv_style_t style_card;
lv_style_t style_btn_primary;
lv_style_t style_btn_destructive;
lv_style_t style_title;
lv_style_t style_text_label;
lv_style_t style_text_value_large;

void apply_app_styles() {
    // Screen
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, COLOR_BACKGROUND);

    // Card — soft, subtle shadow like the web cards (0 2px 8px rgba(0,0,0,.05))
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_CARD);
    lv_style_set_radius(&style_card, 16);
    lv_style_set_pad_all(&style_card, 16);
    lv_style_set_shadow_width(&style_card, 16);
    lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
    lv_style_set_shadow_opa(&style_card, LV_OPA_10);
    lv_style_set_shadow_ofs_y(&style_card, 4);

    // Buttons — blue fill, 16px
    lv_style_init(&style_btn_primary);
    lv_style_set_radius(&style_btn_primary, 12);
    lv_style_set_bg_color(&style_btn_primary, COLOR_PRIMARY);
    lv_style_set_text_color(&style_btn_primary, lv_color_white());
    lv_style_set_text_font(&style_btn_primary, &lv_font_montserrat_16);

    lv_style_init(&style_btn_destructive);
    lv_style_set_radius(&style_btn_destructive, 12);
    lv_style_set_bg_color(&style_btn_destructive, COLOR_DESTRUCTIVE);
    lv_style_set_text_color(&style_btn_destructive, lv_color_white());
    lv_style_set_text_font(&style_btn_destructive, &lv_font_montserrat_14);

    // Text — hero temperature reading
    lv_style_init(&style_text_value_large);
    lv_style_set_text_font(&style_text_value_large, &lv_font_montserrat_28);
    lv_style_set_text_color(&style_text_value_large, COLOR_TEXT_PRI);
    
    lv_style_init(&style_text_label);
    lv_style_set_text_font(&style_text_label, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_text_label, COLOR_TEXT_SEC);

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_title, COLOR_TEXT_PRI);
}