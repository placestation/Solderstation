#ifndef REFLOW_LOGIC_H
#define REFLOW_LOGIC_H

#include <Arduino.h>
#include <max6675.h>
#include <PID_v1.h>
#undef LIBRARY_VERSION
#include <PID_AutoTune_v0.h>
#include <Preferences.h>
#include "config.h"

#define OVERHEAT_TEMPERATURE        255
#define OVERHEAT_SAMPLES_THRESHOLD  10
#define TC_FAULT_SAMPLES_THRESHOLD  5    // consecutive bad TC reads (~2.5s) -> safe abort
#define PREPARE_TIME_MS             45000
#define PREPARE_TEMPERATURE_CUTOFF  45
#define TEMP_DELTA_PERIOD_S         3

// --- PID Defaults (used if nothing saved in NVS) ---
#define PID_KP        18.1215
#define PID_KI        0.8171
#define PID_KD        140.7766
#define PID_WINDOW_MS 1500

// --- Autotune settings ---
#define AUTOTUNE_TARGET_TEMP  150.0
#define AUTOTUNE_STEP         100
#define AUTOTUNE_NOISE        1.0
#define AUTOTUNE_LOOKBACK_S   20

enum ReflowState { IDLE, PREPARE, PREHEAT, SOAK, REFLOW, COOLING, AUTOTUNE };

struct ReflowProfile {
    String name;
    float soakTemp;
    int soakTime;
    float reflowTemp;
    int reflowTime;
};

struct ProfileTimings {
    unsigned long preheat;
    unsigned long soakEnd;
    unsigned long reflowPeak;
    unsigned long reflowEnd;
    unsigned long processEnd;
};

// --- Core API ---
void reflow_init();
void reflow_loop();
void reflow_start_process();
void reflow_stop_process();
void reflow_toggle_manual_heat(int heater);
void reflow_set_profile_index(int index);
void reflow_update_custom_profile(float soakT, int soakS, float reflowT, int reflowS);
String reflow_get_ideal_profile_points();

// --- Autotune API ---
void   reflow_start_autotune();
void   reflow_cancel_autotune();
bool   reflow_is_autotuning();
String reflow_get_autotune_status();
double reflow_get_autotune_kp();
double reflow_get_autotune_ki();
double reflow_get_autotune_kd();

// --- NVS PID save/load ---
void reflow_save_pid_to_nvs(double kp, double ki, double kd);
void reflow_reset_pid_to_defaults();
bool reflow_pid_is_from_nvs();  // true if current values were loaded from flash

// --- Event callbacks ---
void onOverheating(void (*fn_cb)(void));
void onUpdateCustomProfile(void (*fn_cb)(void));

// --- Getters ---
float          getTargetTemperature(unsigned long atTimeMs);
ReflowState    reflow_get_state();
float          reflow_get_current_temp();
float          reflow_get_target_temp();
unsigned long  reflow_get_elapsed_time();
int            reflow_get_current_profile_index();
ReflowProfile* reflow_get_profile(int index);
ProfileTimings reflow_get_profile_timings();
int            reflow_get_profile_count();
const char*    reflow_get_state_string();   // const literal — no per-call heap alloc
bool           reflow_is_manual_heater_on(int heater_num);
bool           isOverheat();
bool           reflow_is_tc_fault();        // thermocouple disconnected / faulted
String         reflow_get_fault_string();   // human-readable abort reason, "" if none
double         reflow_get_pid_output();
double         reflow_get_pid_kp();
double         reflow_get_pid_ki();
double         reflow_get_pid_kd();

#endif