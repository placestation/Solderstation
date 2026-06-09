#ifndef UI_H
#define UI_H

#include <lvgl.h>

// --- UI Objects ---
extern lv_obj_t* label_current_temp_val;
extern lv_obj_t* label_target_temp_val;
extern lv_obj_t* label_time_val;
extern lv_obj_t* label_status_val;
extern lv_obj_t* btn_start_stop;
extern lv_obj_t* label_start_stop;
extern lv_obj_t* chart;
extern lv_chart_series_t* series_actual;
extern lv_chart_series_t* series_ideal;
extern lv_obj_t* manual_control_card;
extern lv_obj_t* btn_heat_all;
extern lv_obj_t* btn_heat_top;
extern lv_obj_t* btn_heat_bottom;
extern lv_obj_t* profile_selector_card;

// static lv_obj_t* spinbox_soak_temp;
// static lv_obj_t* spinbox_soak_time;
// static lv_obj_t* spinbox_reflow_temp;
// static lv_obj_t* spinbox_reflow_time;

void show_boot_screen();
void create_ui();
void ui_update_values();

#endif // UI_H