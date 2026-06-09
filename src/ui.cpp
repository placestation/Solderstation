#include "ui.h"
#include "styles.h"
#include "reflow_logic.h"
#include "web_server.h"
#include "config.h"
#include <Arduino.h>

#define CHART_MAX_Y_VALUE 260 // celcius
#define CHART_MAX_X_VALUE 540 // 6mins, point sampled per second

// --- UI Objects ---
lv_timer_t* timerUiRefresh;
lv_obj_t* wifi_status; 
lv_obj_t* label_current_temp_val;
lv_obj_t* label_target_temp_val;
lv_obj_t* label_time_val;
lv_obj_t* label_power_val;
lv_obj_t* label_status_val;
lv_obj_t* btn_start_stop;
lv_obj_t* label_start_stop;
lv_obj_t* chart;
lv_chart_series_t* series_actual;
lv_chart_series_t* series_ideal;
lv_obj_t* manual_control_card;
lv_obj_t* btn_heat_all;
lv_obj_t* btn_heat_top;
lv_obj_t* btn_heat_bottom;
lv_obj_t* profile_selector_card;
lv_obj_t *profile_selector;

static int lastChartSec = -1;   // last profile-second index plotted on the actual curve

static lv_obj_t* status_badge;
static uint8_t prevProfileIndex = 255; // default invalid value

static const char *profile_map[] = {"Lead-Free", "Lead-Based", "Custom", ""};

static lv_coord_t customProfileInput_colDsc[] = {80, 40, 80, 40, LV_GRID_TEMPLATE_LAST};
static lv_coord_t customProfileInput_rowDsc[] = {50, 50, 50, 50, LV_GRID_TEMPLATE_LAST};

char temp_str[10];

// --- Forward Declarations ---
static void ui_draw_ideal_profile_chart();
static void show_too_hot_message(int currentT, int coolBelow);

// --- Event Handlers ---
static void start_stop_event_handler(lv_event_t* e) {
    if (reflow_get_state() == IDLE) {
        // Refuse to start if the oven is still above the profile's soak temp:
        // the profile timing assumes it starts below that, and physically you
        // can't "ramp up" to a temperature you're already past.
        ReflowProfile* p = reflow_get_profile(reflow_get_current_profile_index());
        int curT = (int)reflow_get_current_temp();
        if (p && curT >= (int)p->soakTemp) {
            show_too_hot_message(curT, (int)p->soakTemp);
            // Un-press the START toggle so the UI reflects that we didn't start.
            lv_obj_clear_state(btn_start_stop, LV_STATE_CHECKED);
            lv_label_set_text(label_start_stop, "START");
            return;
        }
        reflow_start_process();
        ui_draw_ideal_profile_chart();
    } else {
        reflow_stop_process();
    }
}

static void manual_heat_event_handler(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    if (btn == btn_heat_top) reflow_toggle_manual_heat(1);
    if (btn == btn_heat_bottom) reflow_toggle_manual_heat(2);
    if (btn == btn_heat_all) {
        bool top_on = reflow_is_manual_heater_on(1);
        bool bottom_on = reflow_is_manual_heater_on(2);
        if (top_on && bottom_on) {
            reflow_toggle_manual_heat(1);
            reflow_toggle_manual_heat(2);
        } else {
            if (!top_on) reflow_toggle_manual_heat(1);
            if (!bottom_on) reflow_toggle_manual_heat(2);
        }
    }
    ui_update_values();
}

static void event_profileChange(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(btn);
    reflow_set_profile_index((int16_t)id);
    ui_draw_ideal_profile_chart();
}

static void ui_update_timer_cb(lv_timer_t* timer) { ui_update_values(); }

// --- UI Builder Functions ---
static lv_obj_t* create_card(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return card;
}

static lv_obj_t* wifi_reset_mbox = nullptr;

static void wifi_reset_confirm_handler(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x8e8e93), 0);
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (lbl) lv_label_set_text(lbl, "Resetting...");
    lv_refr_now(NULL);
    web_server_reset_wifi();
}

static void wifi_reset_cancel_handler(lv_event_t* e) {
    if (wifi_reset_mbox) {
        lv_obj_del(wifi_reset_mbox);
        wifi_reset_mbox = nullptr;
        lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    }
}

static void wifi_reset_btn_handler(lv_event_t* e) {
    if (wifi_reset_mbox) return;

    wifi_reset_mbox = lv_obj_create(lv_layer_top());
    lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(wifi_reset_mbox, 260, 160);
    lv_obj_center(wifi_reset_mbox);
    lv_obj_set_style_radius(wifi_reset_mbox, 12, 0);
    lv_obj_set_style_pad_all(wifi_reset_mbox, 16, 0);
    lv_obj_set_flex_flow(wifi_reset_mbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wifi_reset_mbox, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(wifi_reset_mbox, 0, 0);
    lv_obj_set_style_shadow_width(wifi_reset_mbox, 20, 0);
    lv_obj_clear_flag(wifi_reset_mbox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(wifi_reset_mbox);
    lv_label_set_text(title, "Reset WiFi?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_pad_bottom(title, 8, 0);

    lv_obj_t* msg = lv_label_create(wifi_reset_mbox);
    lv_label_set_text(msg, "Erase credentials\nand restart?");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(msg, lv_color_hex(0x8e8e93), 0);
    lv_obj_set_style_pad_bottom(msg, 16, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 228);

    lv_obj_t* btn_row = lv_obj_create(wifi_reset_mbox);
    lv_obj_remove_style_all(btn_row);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 10, 0);

    lv_obj_t* btn_no = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(btn_no, 1);
    lv_obj_set_height(btn_no, 40);
    lv_obj_set_style_bg_color(btn_no, lv_color_hex(0xe5e5ea), 0);
    lv_obj_set_style_radius(btn_no, 8, 0);
    lv_obj_set_style_shadow_width(btn_no, 0, 0);
    lv_obj_add_flag(btn_no, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn_no, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl_no = lv_label_create(btn_no);
    lv_label_set_text(lbl_no, "No");
    lv_obj_set_style_text_color(lbl_no, lv_color_hex(0x1c1c1e), 0);
    lv_obj_center(lbl_no);
    lv_obj_add_event_cb(btn_no, wifi_reset_cancel_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t* btn_yes = lv_btn_create(btn_row);
    lv_obj_set_flex_grow(btn_yes, 1);
    lv_obj_set_height(btn_yes, 40);
    lv_obj_set_style_bg_color(btn_yes, lv_color_hex(0xff3b30), 0);
    lv_obj_set_style_radius(btn_yes, 8, 0);
    lv_obj_set_style_shadow_width(btn_yes, 0, 0);
    lv_obj_add_flag(btn_yes, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn_yes, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl_yes = lv_label_create(btn_yes);
    lv_label_set_text(lbl_yes, "Yes, Reset");
    lv_obj_set_style_text_color(lbl_yes, lv_color_white(), 0);
    lv_obj_center(lbl_yes);
    lv_obj_add_event_cb(btn_yes, wifi_reset_confirm_handler, LV_EVENT_CLICKED, NULL);
}

static lv_obj_t* too_hot_mbox = nullptr;

static void too_hot_dismiss_handler(lv_event_t* e) {
    if (too_hot_mbox) {
        lv_obj_del(too_hot_mbox);
        too_hot_mbox = nullptr;
        lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    }
}

// Cheeky "let it cool" popup shown when Start is pressed with a too-hot oven.
static void show_too_hot_message(int currentT, int coolBelow) {
    if (too_hot_mbox) return;

    too_hot_mbox = lv_obj_create(lv_layer_top());
    lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(too_hot_mbox, 280, 200);
    lv_obj_center(too_hot_mbox);
    lv_obj_set_style_radius(too_hot_mbox, 12, 0);
    lv_obj_set_style_pad_all(too_hot_mbox, 16, 0);
    lv_obj_set_flex_flow(too_hot_mbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(too_hot_mbox, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(too_hot_mbox, 0, 0);
    lv_obj_set_style_shadow_width(too_hot_mbox, 20, 0);
    lv_obj_clear_flag(too_hot_mbox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(too_hot_mbox);
    lv_label_set_text(title, LV_SYMBOL_WARNING "  Whoa, hotshot!");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_DESTRUCTIVE, 0);
    lv_obj_set_style_pad_bottom(title, 8, 0);

    lv_obj_t* msg = lv_label_create(too_hot_mbox);
    lv_label_set_text_fmt(msg,
        "Oven's still %d°C - way too toasty\nto start over. Go grab a coffee\nand let it cool below %d°C first.",
        currentT, coolBelow);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(msg, lv_color_hex(0x8e8e93), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_bottom(msg, 16, 0);

    lv_obj_t* btn_ok = lv_btn_create(too_hot_mbox);
    lv_obj_set_size(btn_ok, 140, 40);
    lv_obj_set_style_radius(btn_ok, 8, 0);
    lv_obj_set_style_shadow_width(btn_ok, 0, 0);
    lv_obj_add_event_cb(btn_ok, too_hot_dismiss_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "Fine, I'll wait");
    lv_obj_set_style_text_color(lbl_ok, lv_color_white(), 0);
    lv_obj_center(lbl_ok);
}

void create_status_card(lv_obj_t* parent) {
    lv_obj_t* card = create_card(parent);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 4, 0);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(card, 10, 0);

    // WiFi status — full width row at top
    wifi_status = lv_label_create(card);
    lv_obj_add_style(wifi_status, &style_text_label, LV_PART_MAIN);
    lv_obj_set_width(wifi_status, LV_PCT(100));
    lv_label_set_long_mode(wifi_status, LV_LABEL_LONG_WRAP);

    // Middle row
    lv_obj_t* mid_row = lv_obj_create(card);
    lv_obj_remove_style_all(mid_row);
    lv_obj_set_size(mid_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(mid_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mid_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(mid_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(mid_row, LV_OBJ_FLAG_CLICKABLE);

    // Info column
    lv_obj_t* info_col = lv_obj_create(mid_row);
    lv_obj_remove_style_all(info_col);
    lv_obj_set_flex_flow(info_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(info_col, 1);
    lv_obj_clear_flag(info_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(info_col, LV_OBJ_FLAG_CLICKABLE);

    label_current_temp_val = lv_label_create(info_col);
    lv_obj_add_style(label_current_temp_val, &style_text_value_large, LV_PART_MAIN);

    label_target_temp_val = lv_label_create(info_col);
    lv_obj_add_style(label_target_temp_val, &style_text_label, LV_PART_MAIN);

    label_time_val = lv_label_create(info_col);
    lv_obj_add_style(label_time_val, &style_text_label, LV_PART_MAIN);

    label_power_val = lv_label_create(info_col);
    lv_obj_add_style(label_power_val, &style_text_label, LV_PART_MAIN);

    status_badge = lv_obj_create(info_col);
    lv_obj_set_size(status_badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(status_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_badge, lv_color_hex(0xf2f2f7), 0);
    lv_obj_set_style_pad_hor(status_badge, 10, 0);
    lv_obj_set_style_pad_ver(status_badge, 4, 0);
    lv_obj_set_style_pad_top(status_badge, 4, 0);

    label_status_val = lv_label_create(status_badge);
    lv_obj_add_style(label_status_val, &style_text_label, 0);
    lv_obj_center(label_status_val);

    // Right column
    lv_obj_t* right_col = lv_obj_create(mid_row);
    lv_obj_remove_style_all(right_col);
    lv_obj_set_size(right_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(right_col, 8, 0);
    lv_obj_clear_flag(right_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(right_col, LV_OBJ_FLAG_CLICKABLE);

    btn_start_stop = lv_btn_create(right_col);
    lv_obj_set_size(btn_start_stop, 100, 60);
    lv_obj_add_event_cb(btn_start_stop, start_stop_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(btn_start_stop, LV_OBJ_FLAG_CHECKABLE);
    label_start_stop = lv_label_create(btn_start_stop);
    lv_label_set_text(label_start_stop, "START");
    lv_obj_set_style_text_font(label_start_stop, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(label_start_stop);

    // WiFi reset button
    lv_obj_t* btn_wifi_reset = lv_btn_create(right_col);
    lv_obj_add_style(btn_wifi_reset, &style_btn_destructive, LV_PART_MAIN);
    lv_obj_set_size(btn_wifi_reset, 100, 36);
    lv_obj_add_flag(btn_wifi_reset, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(btn_wifi_reset, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl_wifi_reset = lv_label_create(btn_wifi_reset);
    lv_obj_set_style_text_font(lbl_wifi_reset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_wifi_reset, lv_color_white(), 0);
    lv_label_set_text(lbl_wifi_reset, "WiFi Reset");
    lv_obj_center(lbl_wifi_reset);
    lv_obj_add_event_cb(btn_wifi_reset, wifi_reset_btn_handler, LV_EVENT_CLICKED, NULL);
    Serial.println("DEBUG: WiFi Reset button created and event registered");
}

void create_chart_card(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, 280);

    lv_obj_t* title = lv_label_create(card);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Temperature Profile");

    chart = lv_chart_create(card);
    lv_obj_set_size(chart, 220, 180);
    lv_obj_set_pos(chart, 40, 32);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);   // was SCATTER — data is written by index, LINE renders the same far cheaper
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(chart, CHART_MAX_X_VALUE);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, 7, 1, true, 30);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, CHART_MAX_X_VALUE);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 6, 2, true, 40);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, CHART_MAX_Y_VALUE);
    lv_obj_set_style_line_width(chart, 3, LV_PART_MAIN);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
    series_actual = lv_chart_add_series(chart, COLOR_DESTRUCTIVE, LV_CHART_AXIS_PRIMARY_Y);

    // Start empty (no flat zero-line on boot); samples are filled in live.
    lv_chart_set_all_value(chart, series_actual, LV_CHART_POINT_NONE);

    // Ideal profile is a SECOND series on the SAME chart. Previously it was a
    // separate chart object stacked on top, which forced BOTH charts to repaint
    // on every update/scroll. One chart, two series = half the draw work and a
    // single shared set of axes.
    series_ideal = lv_chart_add_series(chart, COLOR_PRIMARY, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, series_ideal, LV_CHART_POINT_NONE);
}

void create_manual_control_card(lv_obj_t* parent) {
    manual_control_card = create_card(parent);

    lv_obj_t* title = lv_label_create(manual_control_card);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Manual Heater Control");

    lv_obj_t* btn_cont = lv_obj_create(manual_control_card);
    lv_obj_remove_style_all(btn_cont);
    lv_obj_set_width(btn_cont, LV_PCT(100));
    lv_obj_set_height(btn_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(btn_cont, 8, 0);

    btn_heat_all = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_heat_all, LV_PCT(100));
    lv_obj_set_height(btn_heat_all, 50);
    lv_obj_add_event_cb(btn_heat_all, manual_heat_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* label_all = lv_label_create(btn_heat_all);
    lv_label_set_text(label_all, "Heat All");
    lv_obj_center(label_all);

    lv_obj_t* bottom_row = lv_obj_create(btn_cont);
    lv_obj_remove_style_all(bottom_row);
    lv_obj_set_width(bottom_row, LV_PCT(100));
    lv_obj_set_height(bottom_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(bottom_row, 10, 0);

    btn_heat_top = lv_btn_create(bottom_row);
    lv_obj_add_style(btn_heat_top, &style_btn_primary, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn_heat_top, 1);
    lv_obj_set_height(btn_heat_top, 50);
    lv_obj_add_event_cb(btn_heat_top, manual_heat_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* label_top = lv_label_create(btn_heat_top);
    lv_label_set_text(label_top, "Heat Top");
    lv_obj_center(label_top);

    btn_heat_bottom = lv_btn_create(bottom_row);
    lv_obj_add_style(btn_heat_bottom, &style_btn_primary, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn_heat_bottom, 1);
    lv_obj_set_height(btn_heat_bottom, 50);
    lv_obj_add_event_cb(btn_heat_bottom, manual_heat_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* label_bottom = lv_label_create(btn_heat_bottom);
    lv_label_set_text(label_bottom, "Heat Bottom");
    lv_obj_center(label_bottom);
}

void create_profile_selector_card(lv_obj_t* parent) {
    profile_selector_card = create_card(parent);

    lv_obj_t* title = lv_label_create(profile_selector_card);
    lv_obj_add_style(title, &style_title, 0);
    lv_label_set_text(title, "Profile Selection");

    profile_selector = lv_btnmatrix_create(profile_selector_card);
    lv_btnmatrix_set_map(profile_selector, profile_map);
    lv_btnmatrix_set_one_checked(profile_selector, true);

    lv_obj_add_style(profile_selector, &style_btn_primary, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(profile_selector, COLOR_SUCCESS, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_height(profile_selector, 80, 0);
    lv_obj_set_style_pad_gap(profile_selector, 2, 0);
    lv_obj_set_style_pad_all(profile_selector, 0, 0);
    lv_obj_set_style_border_width(profile_selector, 0, 0);

    lv_btnmatrix_set_btn_ctrl_all(profile_selector, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_selected_btn(profile_selector, reflow_get_current_profile_index());

    lv_obj_add_event_cb(profile_selector, event_profileChange, LV_EVENT_CLICKED, NULL);
}

void show_overheat_message() {
    lv_timer_del(timerUiRefresh);

    lv_obj_t* screen = lv_scr_act();
    lv_obj_clean(screen);

    lv_obj_t* msgCard = create_card(screen);
    lv_obj_t* title = lv_label_create(msgCard);
    lv_label_set_text(title, LV_SYMBOL_WARNING " STOPPED");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, COLOR_DESTRUCTIVE, LV_PART_MAIN);

    // Reason is set by the logic layer (overheat or thermocouple fault).
    String reason = reflow_get_fault_string();
    lv_obj_t* msg = lv_label_create(msgCard);
    lv_label_set_text(msg, reason.length() ? reason.c_str() : "Restart to clear the message.");
    lv_obj_add_style(msg, &style_text_label, LV_PART_MAIN);
}

void show_boot_screen() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    const lv_coord_t cx  = 160;
    const lv_coord_t cy  = 210;
    const lv_color_t org = lv_color_hex(0xFF5500);
    const lv_coord_t lw  = 7;
    const lv_coord_t bw  = 30;   // box width (near-square, matches logo)
    const lv_coord_t bh  = 33;   // box height
    const lv_coord_t bsp = 8;    // gap between the two boxes
    const lv_coord_t arm_h = 100;
    const lv_coord_t arm_v = 90;
    const lv_coord_t gap_v = 14; // gap between vertical arms and boxes
    const lv_coord_t gap_h = 13; // gap between horizontal arms and boxes

    const lv_coord_t lbox_x  = cx - bsp/2 - bw;
    const lv_coord_t lbox_rx = cx - bsp/2;
    const lv_coord_t rbox_x  = cx + bsp/2;
    const lv_coord_t rbox_rx = cx + bsp/2 + bw;
    const lv_coord_t box_ty  = cy - bh/2;
    const lv_coord_t box_by  = cy + bh/2;

    lv_obj_t* ln_top = lv_line_create(scr);
    static lv_point_t pts_top[2] = {{cx, box_ty - gap_v - arm_v}, {cx, box_ty - gap_v}};
    lv_line_set_points(ln_top, pts_top, 2);
    lv_obj_set_style_line_color(ln_top, org, 0);
    lv_obj_set_style_line_width(ln_top, lw, 0);
    lv_obj_set_style_line_rounded(ln_top, true, 0);

    lv_obj_t* ln_bot = lv_line_create(scr);
    static lv_point_t pts_bot[2] = {{cx, box_by + gap_v}, {cx, box_by + gap_v + arm_v}};
    lv_line_set_points(ln_bot, pts_bot, 2);
    lv_obj_set_style_line_color(ln_bot, org, 0);
    lv_obj_set_style_line_width(ln_bot, lw, 0);
    lv_obj_set_style_line_rounded(ln_bot, true, 0);

    lv_obj_t* ln_left = lv_line_create(scr);
    static lv_point_t pts_left[2] = {{lbox_x - gap_h - arm_h, cy}, {lbox_x - gap_h, cy}};
    lv_line_set_points(ln_left, pts_left, 2);
    lv_obj_set_style_line_color(ln_left, org, 0);
    lv_obj_set_style_line_width(ln_left, lw, 0);
    lv_obj_set_style_line_rounded(ln_left, true, 0);

    lv_obj_t* ln_right = lv_line_create(scr);
    static lv_point_t pts_right[2] = {{rbox_rx + gap_h, cy}, {rbox_rx + gap_h + arm_h, cy}};
    lv_line_set_points(ln_right, pts_right, 2);
    lv_obj_set_style_line_color(ln_right, org, 0);
    lv_obj_set_style_line_width(ln_right, lw, 0);
    lv_obj_set_style_line_rounded(ln_right, true, 0);

    lv_obj_t* box_l = lv_obj_create(scr);
    lv_obj_set_size(box_l, bw, bh);
    lv_obj_set_pos(box_l, lbox_x, box_ty);
    lv_obj_set_style_bg_color(box_l, lv_color_hex(0x1c1c1e), 0);
    lv_obj_set_style_border_color(box_l, lv_color_hex(0xc8c8c8), 0);
    lv_obj_set_style_border_width(box_l, 3, 0);
    lv_obj_set_style_radius(box_l, 5, 0);
    lv_obj_clear_flag(box_l, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* box_r = lv_obj_create(scr);
    lv_obj_set_size(box_r, bw, bh);
    lv_obj_set_pos(box_r, rbox_x, box_ty);
    lv_obj_set_style_bg_color(box_r, lv_color_hex(0x1c1c1e), 0);
    lv_obj_set_style_border_color(box_r, lv_color_hex(0xc8c8c8), 0);
    lv_obj_set_style_border_width(box_r, 3, 0);
    lv_obj_set_style_radius(box_r, 5, 0);
    lv_obj_clear_flag(box_r, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* name = lv_label_create(scr);
    lv_label_set_text(name, "Solderstation");
    lv_obj_set_style_text_font(name, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0x1c1c1e), 0);
    lv_obj_set_pos(name, 0, box_by + gap_v + arm_v + 30);
    lv_obj_set_width(name, 320);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);

    lv_refr_now(NULL);
    delay(2500);

    // Turn backlight off before clearing — eliminates flicker during UI build
    digitalWrite(TFT_BL, LOW);

    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xf7f7f7), 0);
}

void create_ui() {
    lv_obj_t* screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_style(screen, &style_screen, 0);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 8, 0);
    lv_obj_set_style_pad_row(screen, 6, 0);

    create_status_card(screen);
    create_chart_card(screen);
    create_manual_control_card(screen);
    create_profile_selector_card(screen);

    ui_draw_ideal_profile_chart();
    onUpdateCustomProfile(ui_draw_ideal_profile_chart);
    onOverheating(show_overheat_message);

    // Single refresh timer. (Previously two were created here — the first was
    // leaked and never deleted, so ui_update_values ran twice per period and
    // show_overheat_message's lv_timer_del only stopped one of them.)
    timerUiRefresh = lv_timer_create(ui_update_timer_cb, 500, NULL);
    lv_timer_ready(timerUiRefresh);

    ui_update_values();   // populate labels immediately

    // Force full render then turn backlight on — clean transition, no flicker
    lv_refr_now(NULL);
    digitalWrite(TFT_BL, HIGH);
}

void ui_update_values() {
    // --- WiFi status label ---
    switch (getWifiState()) {
        case WIFI_STATE_CONNECTED:
            lv_label_set_text_fmt(wifi_status, "WiFi - %s", getWifiIPAddress().toString().c_str());
            break;
        case WIFI_STATE_AP_MODE:
            lv_label_set_text_fmt(wifi_status, "Connect to '%s' to setup WiFi", getWifiAPName().c_str());
            break;
        case WIFI_STATE_DISCONNECTED:
            lv_label_set_text(wifi_status, "WiFi - reconnecting...");
            break;
        case WIFI_STATE_CONNECTING:
        default:
            lv_label_set_text(wifi_status, "WiFi - connecting...");
            break;
    }

    // --- Temperature & time labels ---
    dtostrf(reflow_get_current_temp(), 4, 1, temp_str);
    lv_label_set_text_fmt(label_current_temp_val, "%s°C", temp_str);

    dtostrf(reflow_get_target_temp(), 4, 1, temp_str);
    lv_label_set_text_fmt(label_target_temp_val, "Target: %s°C", temp_str);

    unsigned long time_s = reflow_get_elapsed_time();
    lv_label_set_text_fmt(label_time_val, "Time: %02lu:%02lu", time_s / 60, time_s % 60);

    lv_label_set_text(label_status_val, reflow_get_state_string());

    // --- State-dependent UI ---
    ReflowState state = reflow_get_state();

    // Live heater power (PID duty; full-on during the PREPARE warm-up)
    int power_pct = (state == PREPARE)
                    ? 100
                    : (int)((reflow_get_pid_output() / 255.0) * 100.0 + 0.5);
    lv_label_set_text_fmt(label_power_val, "Heater: %d%%", power_pct);
    if (state == IDLE) {
        lv_label_set_text(label_start_stop, "START");
        lv_obj_clear_state(btn_start_stop, LV_STATE_CHECKED);
        lv_obj_clear_state(manual_control_card, LV_STATE_DISABLED);
        lv_obj_clear_state(profile_selector,    LV_STATE_DISABLED);
        lv_obj_clear_state(btn_heat_top,        LV_STATE_DISABLED);
        lv_obj_clear_state(btn_heat_bottom,     LV_STATE_DISABLED);
        lv_obj_clear_state(btn_heat_all,        LV_STATE_DISABLED);
        lv_chart_set_all_value(chart, series_actual, LV_CHART_POINT_NONE);
        lastChartSec = -1;
    }
    else {
        lv_label_set_text(label_start_stop, "STOP");
        lv_obj_add_state(btn_start_stop,      LV_STATE_CHECKED);
        lv_obj_add_state(manual_control_card, LV_STATE_DISABLED);
        lv_obj_add_state(profile_selector,    LV_STATE_DISABLED);
        lv_obj_add_state(btn_heat_top,        LV_STATE_DISABLED);
        lv_obj_add_state(btn_heat_bottom,     LV_STATE_DISABLED);
        lv_obj_add_state(btn_heat_all,        LV_STATE_DISABLED);

        // Sample the actual-temperature curve indexed by PROFILE seconds (the
        // temperature-gated clock) so it stays aligned with the ideal curve.
        // While the profile clock is paused the x-index doesn't advance, so the
        // two curves never drift apart. (Completion is driven by the profile
        // clock in reflow logic, not by the chart anymore.)
        if (state != PREPARE) {
            int ps = (int)reflow_get_elapsed_time();   // profile seconds
            if (ps != lastChartSec && ps >= 0 && ps < CHART_MAX_X_VALUE) {
                series_actual->y_points[ps] =
                    (int16_t)min((int)reflow_get_current_temp(), CHART_MAX_Y_VALUE);
                lastChartSec = ps;
                lv_chart_refresh(chart);
            }
        }
    }

    // --- Manual heat button colours ---
    bool top_on    = reflow_is_manual_heater_on(1);
    bool bottom_on = reflow_is_manual_heater_on(2);
    lv_obj_set_style_bg_color(btn_heat_top,    top_on    ? COLOR_DESTRUCTIVE : COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_heat_bottom, bottom_on ? COLOR_DESTRUCTIVE : COLOR_PRIMARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_heat_all,    (top_on && bottom_on) ? COLOR_DESTRUCTIVE : COLOR_PRIMARY, LV_PART_MAIN);

    // --- Profile selector sync ---
    uint8_t activeProfileIndex = (uint8_t)reflow_get_current_profile_index();
    if (activeProfileIndex != prevProfileIndex) {
        lv_btnmatrix_set_btn_ctrl(profile_selector, activeProfileIndex, LV_BTNMATRIX_CTRL_CHECKED);
        ui_draw_ideal_profile_chart();
        prevProfileIndex = activeProfileIndex;
    }
}

void ui_draw_ideal_profile_chart() {
    ReflowProfile* p = reflow_get_profile(reflow_get_current_profile_index());
    if (!p) return;

    ProfileTimings pTimes = reflow_get_profile_timings();
    int16_t ambient = (int16_t)reflow_get_current_temp();

    struct Point { int16_t x; int16_t y; };
    Point waypoints[] = {
        { 0,                                    ambient                 },
        { (int16_t)(pTimes.preheat    / 1000),  (int16_t)p->soakTemp   },
        { (int16_t)(pTimes.soakEnd    / 1000),  (int16_t)p->soakTemp   },
        { (int16_t)(pTimes.reflowPeak / 1000),  (int16_t)p->reflowTemp },
        { (int16_t)(pTimes.reflowEnd  / 1000),  (int16_t)p->reflowTemp },
        { (int16_t)(pTimes.processEnd / 1000),  ambient                },
    };
    int numWaypoints = sizeof(waypoints) / sizeof(waypoints[0]);

    for (uint16_t i = 0; i < CHART_MAX_X_VALUE; i++)
        series_ideal->y_points[i] = LV_CHART_POINT_NONE;

    for (int seg = 0; seg < numWaypoints - 1; seg++) {
        int16_t x0 = waypoints[seg].x,   y0 = waypoints[seg].y;
        int16_t x1 = waypoints[seg+1].x, y1 = waypoints[seg+1].y;
        if (x1 <= x0) continue;
        for (int16_t x = x0; x <= x1 && x < CHART_MAX_X_VALUE; x++) {
            float t = (float)(x - x0) / (float)(x1 - x0);
            series_ideal->y_points[x] = (lv_coord_t)(y0 + t * (y1 - y0));
        }
    }

    lv_chart_refresh(chart);
}