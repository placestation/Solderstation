#include "reflow_logic.h"

// #define SIMULATION_MODE

// --- NVS ---
static Preferences prefs;
static bool        pid_from_nvs = false;

// --- Private Variables ---
MAX6675 thermocouple(MAX6675_CLK, MAX6675_CS, MAX6675_DO);
float ambient     = 25.0;
float currentTemp = 25.0;
float targetTemp  = 25.0;
unsigned long prepareStartTime = 0;
unsigned long processStartTime = 0;
ReflowState currentState       = IDLE;
bool heater1_manual_on         = false;
bool heater2_manual_on         = false;
int  currentProfileIndex       = 0;
unsigned long lastTempRead     = 0;
unsigned long elapsedTime      = 0UL;

uint8_t numOfOverheatSamples = 0;
bool    flagIsOverheat       = false;

void (*callback_onOverheating)(void)         = nullptr;
void (*callback_onUpdateCustomProfile)(void) = nullptr;

// --- PID ---
double pid_input    = 0.0;
double pid_output   = 0.0;
double pid_setpoint = 0.0;
double pid_kp = PID_KP, pid_ki = PID_KI, pid_kd = PID_KD;
PID reflowPID(&pid_input, &pid_output, &pid_setpoint, pid_kp, pid_ki, pid_kd, DIRECT);
unsigned long pidWindowStart = 0;

// --- Autotune ---
PID_ATune pidAutoTune(&pid_input, &pid_output);
bool   atRunning         = false;
double at_kp             = 0.0;
double at_ki             = 0.0;
double at_kd             = 0.0;
String at_status         = "Idle";
static const double AT_OUTPUT_START = 255.0;
static bool at_preheating = false;  // true = heating up to setpoint, not yet oscillating

// --- Profiles ---
ReflowProfile profiles[] = {
    {"Lead-Free",  150.0, 90,  245.0, 30},
    {"Lead-Based", 140.0, 75,  200.0, 25},
    {"Custom",     150.0, 90,  240.0, 30}
};
constexpr int PROFILE_COUNT = sizeof(profiles) / sizeof(profiles[0]);

float preheatRate = 0.8, reflowRate = 0.8, coolingRate = -2.0;
ProfileTimings pTimes;


// ---------------------------------------------------------------------------
// NVS helpers
// ---------------------------------------------------------------------------

void loadPidFromNvs() {
    prefs.begin("pid", true); // read-only
    bool hasData = prefs.isKey("kp");
    if (hasData) {
        pid_kp = prefs.getDouble("kp", PID_KP);
        pid_ki = prefs.getDouble("ki", PID_KI);
        pid_kd = prefs.getDouble("kd", PID_KD);
        pid_from_nvs = true;
        Serial.printf("PID loaded from NVS: Kp=%.4f Ki=%.6f Kd=%.4f\n", pid_kp, pid_ki, pid_kd);
    } else {
        pid_kp = PID_KP;
        pid_ki = PID_KI;
        pid_kd = PID_KD;
        pid_from_nvs = false;
        Serial.println("PID using hardcoded defaults.");
    }
    prefs.end();
}

void reflow_save_pid_to_nvs(double kp, double ki, double kd) {
    prefs.begin("pid", false); // read-write
    prefs.putDouble("kp", kp);
    prefs.putDouble("ki", ki);
    prefs.putDouble("kd", kd);
    prefs.end();
    // Apply immediately
    pid_kp = kp; pid_ki = ki; pid_kd = kd;
    reflowPID.SetTunings(pid_kp, pid_ki, pid_kd);
    pid_from_nvs = true;
    Serial.printf("PID saved to NVS: Kp=%.4f Ki=%.6f Kd=%.4f\n", kp, ki, kd);
}

void reflow_reset_pid_to_defaults() {
    prefs.begin("pid", false);
    prefs.clear();
    prefs.end();
    pid_kp = PID_KP; pid_ki = PID_KI; pid_kd = PID_KD;
    reflowPID.SetTunings(pid_kp, pid_ki, pid_kd);
    pid_from_nvs = false;
    Serial.println("PID reset to hardcoded defaults.");
}

bool reflow_pid_is_from_nvs() { return pid_from_nvs; }


// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void calculateProfileTimings() {
    ReflowProfile p = profiles[currentProfileIndex];
    pTimes.preheat    = (unsigned long)((p.soakTemp    - ambient) / preheatRate * 1000);
    pTimes.soakEnd    = pTimes.preheat + (unsigned long)(p.soakTime * 1000);
    pTimes.reflowPeak = pTimes.soakEnd + (unsigned long)((p.reflowTemp - p.soakTemp) / reflowRate * 1000);
    pTimes.reflowEnd  = pTimes.reflowPeak + (unsigned long)(p.reflowTime * 1000);
    pTimes.processEnd = pTimes.reflowEnd  + (unsigned long)((p.reflowTemp - ambient) / abs(coolingRate) * 1000);
}

void updateTemperature() {
#ifdef SIMULATION_MODE
    float noise = (rand() % 20 - 10) / 10.0;
    if (currentState == IDLE) {
        currentTemp = 25.0 + noise;
    } else {
        float diff = targetTemp - currentTemp;
        if (abs(diff) < 1.0) currentTemp = targetTemp + noise;
        else currentTemp += (diff / 8.0) + noise;
    }
#else
    float temp = thermocouple.readCelsius();
    if (!isnan(temp) && temp > 0) currentTemp = temp;
#endif
    lastTempRead = millis();

    if (currentTemp >= OVERHEAT_TEMPERATURE) {
        numOfOverheatSamples++;
        if (numOfOverheatSamples >= OVERHEAT_SAMPLES_THRESHOLD) {
            flagIsOverheat = true;
            reflow_stop_process();
            if (callback_onOverheating) callback_onOverheating();
        }
    } else {
        if (numOfOverheatSamples > 0) numOfOverheatSamples--;
    }
}

void updateTargetAndState() {
    if (currentState == IDLE || currentState == AUTOTUNE || processStartTime == 0) return;

    if ((millis() - prepareStartTime) > (unsigned)PREPARE_TIME_MS) {
        elapsedTime = (millis() - processStartTime) - (unsigned)PREPARE_TIME_MS;
        targetTemp  = getTargetTemperature(elapsedTime);

        if      (elapsedTime < pTimes.preheat)    currentState = PREHEAT;
        else if (elapsedTime < pTimes.soakEnd)    currentState = SOAK;
        else if (elapsedTime < pTimes.reflowEnd)  currentState = REFLOW;
        else if (elapsedTime < pTimes.processEnd) {
            if (currentState != COOLING) {
                pid_output = 0;
                Serial.println("Entering COOLING — PID output reset.");
            }
            currentState = COOLING;
        }
        else {
            reflow_stop_process();
        }
    } else {
        currentState = PREPARE;
        targetTemp   = PREPARE_TEMPERATURE_CUTOFF;
    }
}

void updateHeaters() {
    if (currentState == AUTOTUNE) {
        // Direct relay - no time-proportional window during autotune
        bool shouldHeat = (pid_output > 0);
        digitalWrite(SSR1_PIN, shouldHeat);
        digitalWrite(SSR2_PIN, shouldHeat);
        return;
    }

    if (currentState == PREHEAT || currentState == SOAK || currentState == REFLOW) {
        pid_input    = currentTemp;
        pid_setpoint = targetTemp;
        reflowPID.Compute();
        unsigned long now = millis();
        if (now - pidWindowStart >= PID_WINDOW_MS) pidWindowStart = now;
        bool shouldHeat = ((pid_output / 255.0) * PID_WINDOW_MS) > (now - pidWindowStart);
        digitalWrite(SSR1_PIN, shouldHeat);
        digitalWrite(SSR2_PIN, shouldHeat);
    }
    else if (currentState == PREPARE && currentTemp < PREPARE_TEMPERATURE_CUTOFF) {
        digitalWrite(SSR1_PIN, HIGH);
        digitalWrite(SSR2_PIN, HIGH);
    }
    else if (currentState == IDLE) {
        digitalWrite(SSR1_PIN, heater1_manual_on);
        digitalWrite(SSR2_PIN, heater2_manual_on);
    }
    else {
        digitalWrite(SSR1_PIN, LOW);
        digitalWrite(SSR2_PIN, LOW);
    }
}

void updateAutotune() {
    if (!atRunning) return;

    pid_input = currentTemp;

    // --- Phase 1: Preheat ---
    // Heat at full power until we are within 5°C of target.
    // Only THEN hand control to PID_ATune so it doesn't immediately give up.
    if (at_preheating) {
        float gap = AUTOTUNE_TARGET_TEMP - currentTemp;
        at_status = "Preheating... " + String(currentTemp, 1) + "°C → "
                  + String(AUTOTUNE_TARGET_TEMP, 0) + "°C";
        Serial.printf("[AT Preheat] %.1f°C / %.0f°C  gap=%.1f\n",
                      currentTemp, AUTOTUNE_TARGET_TEMP, gap);

        if (gap <= 5.0) {
            // Close enough — hand off to PID_ATune
            at_preheating = false;
            pid_output    = AT_OUTPUT_START;
            pid_setpoint  = AUTOTUNE_TARGET_TEMP;
            pidAutoTune.SetNoiseBand(AUTOTUNE_NOISE);
            pidAutoTune.SetOutputStep(255);
            pidAutoTune.SetLookbackSec(AUTOTUNE_LOOKBACK_S);
            pidAutoTune.SetControlType(1);
            Serial.println("[AT] Preheat done — handing off to PID_ATune oscillation.");
        } else {
            // Keep heaters full on
            pid_output = 255;
        }
        return;
    }

    // --- Phase 2: PID_ATune oscillation ---
    pid_setpoint = AUTOTUNE_TARGET_TEMP;
    int result   = pidAutoTune.Runtime();

    at_status = "Tuning... " + String(currentTemp, 1) + "°C";
    Serial.printf("[AT Tuning] %.1f°C  output=%.0f\n", currentTemp, pid_output);

    if (result != 0) {
        atRunning = false;
        if (result == 1) {
            at_kp = pidAutoTune.GetKp();
            at_ki = pidAutoTune.GetKi();
            at_kd = pidAutoTune.GetKd();
            pid_kp = at_kp; pid_ki = at_ki; pid_kd = at_kd;
            reflowPID.SetTunings(pid_kp, pid_ki, pid_kd);
            at_status = "Done! Kp=" + String(at_kp, 2)
                      + " Ki=" + String(at_ki, 4)
                      + " Kd=" + String(at_kd, 2);
            Serial.println("=== Autotune Complete ===");
            Serial.printf("  Kp=%.4f  Ki=%.6f  Kd=%.4f\n", at_kp, at_ki, at_kd);
        } else {
            at_status = "Failed — try again";
            Serial.println("[AT] Failed.");
        }
        reflowPID.SetMode(AUTOMATIC);
        pid_output   = 0;
        currentState = IDLE;
        digitalWrite(SSR1_PIN, LOW);
        digitalWrite(SSR2_PIN, LOW);
    }
}


// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void reflow_init() {
    pinMode(SSR1_PIN, OUTPUT);
    pinMode(SSR2_PIN, OUTPUT);
    digitalWrite(SSR1_PIN, LOW);
    digitalWrite(SSR2_PIN, LOW);

    // Explicitly configure MAX6675 pins before first use
    pinMode(MAX6675_CS,  OUTPUT); digitalWrite(MAX6675_CS,  HIGH);
    pinMode(MAX6675_CLK, OUTPUT); digitalWrite(MAX6675_CLK, HIGH);
    pinMode(MAX6675_DO,  INPUT);
    delay(500); // let GPIO settle after boot

    loadPidFromNvs();

    reflowPID.SetTunings(pid_kp, pid_ki, pid_kd);
    reflowPID.SetMode(AUTOMATIC);
    reflowPID.SetOutputLimits(0, 255);
    reflowPID.SetSampleTime(500);
    pidWindowStart = millis();

    calculateProfileTimings();

    // Dummy reads to flush bad boot state from MAX6675
    for (int i = 0; i < 3; i++) {
        thermocouple.readCelsius();
        delay(250);
    }

    float t = thermocouple.readCelsius();
    Serial.printf("[INIT] CLK=%d DO=%d CS=%d first read=%.2f\n",
        MAX6675_CLK, MAX6675_DO, MAX6675_CS, t);
    if (!isnan(t) && t > 0) currentTemp = t;
}

void reflow_loop() {
    // Temp read gated to 500ms (MAX6675 limit)
    if (millis() - lastTempRead > 500) {
        updateTemperature();
        if (currentState == AUTOTUNE) {
            updateAutotune(); // updates pid_output based on phase
        } else {
            updateTargetAndState();
            updateHeaters();
        }
    }

    // SSR drive runs every loop during autotune — must not be gated by 500ms
    if (currentState == AUTOTUNE) {
        updateHeaters();
    }
}

void reflow_start_process() {
    if (currentState == IDLE) {
        ambient = currentTemp;
        calculateProfileTimings();
        heater1_manual_on = false;
        heater2_manual_on = false;
        prepareStartTime  = millis();
        processStartTime  = prepareStartTime;
        pidWindowStart    = millis();
        pid_output        = 0;
        currentState      = PREPARE;
    }
}

void reflow_stop_process() {
    currentState      = IDLE;
    prepareStartTime  = 0;
    processStartTime  = 0;
    targetTemp        = 25.0;
    pid_output        = 0;
    heater1_manual_on = false;
    heater2_manual_on = false;
    if (atRunning) {
        pidAutoTune.Cancel();
        atRunning  = false;
        at_status  = "Cancelled";
        reflowPID.SetMode(AUTOMATIC);
    }
    digitalWrite(SSR1_PIN, LOW);
    digitalWrite(SSR2_PIN, LOW);
}

void reflow_start_autotune() {
    if (currentState != IDLE) return;
    Serial.println("=== Autotune Starting ===");
    Serial.printf("Current temp: %.1f°C  Target: %.0f°C\n", currentTemp, AUTOTUNE_TARGET_TEMP);

    at_status     = "Preheating...";
    atRunning     = true;
    at_preheating = true;   // Phase 1: heat up first, PID_ATune starts later
    pid_input     = currentTemp;
    pid_output    = 255;    // full power during preheat
    pid_setpoint  = AUTOTUNE_TARGET_TEMP;

    reflowPID.SetMode(MANUAL); // disable normal PID
    pidWindowStart = millis();
    currentState   = AUTOTUNE;
    targetTemp     = AUTOTUNE_TARGET_TEMP;
}

void reflow_cancel_autotune() {
    if (!atRunning) return;
    pidAutoTune.Cancel();
    atRunning     = false;
    at_preheating = false;
    at_status     = "Cancelled";
    reflowPID.SetMode(AUTOMATIC);
    pid_output   = 0;
    currentState = IDLE;
    digitalWrite(SSR1_PIN, LOW);
    digitalWrite(SSR2_PIN, LOW);
    Serial.println("Autotune cancelled.");
}

void reflow_set_profile_index(int index) {
    if (index >= 0 && index < PROFILE_COUNT) {
        currentProfileIndex = index;
        calculateProfileTimings();
    }
}

void reflow_update_custom_profile(float soakT, int soakS, float reflowT, int reflowS) {
    profiles[2].soakTemp   = soakT;
    profiles[2].soakTime   = soakS;
    profiles[2].reflowTemp = reflowT;
    profiles[2].reflowTime = reflowS;
    if (currentProfileIndex == 2) {
        calculateProfileTimings();
        if (callback_onUpdateCustomProfile) callback_onUpdateCustomProfile();
    }
}

void reflow_toggle_manual_heat(int heater_num) {
    if (currentState != IDLE) return;
    if (heater_num == 0) {
        bool s = !(heater1_manual_on && heater2_manual_on);
        heater1_manual_on = heater2_manual_on = s;
    } else if (heater_num == 1) heater1_manual_on = !heater1_manual_on;
    else if  (heater_num == 2) heater2_manual_on = !heater2_manual_on;
}

void onOverheating(void (*fn_cb)(void))         { callback_onOverheating = fn_cb; }
void onUpdateCustomProfile(void (*fn_cb)(void)) { callback_onUpdateCustomProfile = fn_cb; }

// --- Getters ---
ReflowState    reflow_get_state()                 { return currentState; }
float          reflow_get_current_temp()          { return currentTemp; }
float          reflow_get_target_temp()           { return targetTemp; }
double         reflow_get_pid_output()            { return pid_output; }
double         reflow_get_pid_kp()                { return pid_kp; }
double         reflow_get_pid_ki()                { return pid_ki; }
double         reflow_get_pid_kd()                { return pid_kd; }
int            reflow_get_current_profile_index() { return currentProfileIndex; }
int            reflow_get_profile_count()         { return PROFILE_COUNT; }
ProfileTimings reflow_get_profile_timings()       { return pTimes; }
bool           isOverheat()                       { return flagIsOverheat; }
bool           reflow_is_autotuning()             { return atRunning; }
String         reflow_get_autotune_status()       { return at_status; }
double         reflow_get_autotune_kp()           { return at_kp; }
double         reflow_get_autotune_ki()           { return at_ki; }
double         reflow_get_autotune_kd()           { return at_kd; }

unsigned long reflow_get_elapsed_time() {
    return (currentState != IDLE && currentState != PREPARE && processStartTime > 0)
           ? elapsedTime / 1000 : 0;
}

ReflowProfile* reflow_get_profile(int index) {
    if (index >= 0 && index < PROFILE_COUNT) return &profiles[index];
    return nullptr;
}

String reflow_get_state_string() {
    switch (currentState) {
        case IDLE:     return "Idle";
        case PREPARE:  return "Preparing";
        case PREHEAT:  return "Preheat";
        case SOAK:     return "Soak";
        case REFLOW:   return "Reflow";
        case COOLING:  return "Cooling";
        case AUTOTUNE: return "Autotuning";
        default:       return "Unknown";
    }
}

bool reflow_is_manual_heater_on(int heater_num) {
    if (heater_num == 1) return heater1_manual_on;
    if (heater_num == 2) return heater2_manual_on;
    return false;
}

float getTargetTemperature(unsigned long atTimeMs) {
    ReflowProfile p = profiles[currentProfileIndex];
    float t = 0.0;
    if      (atTimeMs < pTimes.preheat)    t = ambient + (preheatRate * (atTimeMs / 1000.0));
    else if (atTimeMs < pTimes.soakEnd)    t = p.soakTemp;
    else if (atTimeMs < pTimes.reflowPeak) t = p.soakTemp + (reflowRate * ((atTimeMs - pTimes.soakEnd) / 1000.0));
    else if (atTimeMs < pTimes.reflowEnd)  t = p.reflowTemp;
    else if (atTimeMs < pTimes.processEnd) {
        t = p.reflowTemp + (coolingRate * ((atTimeMs - pTimes.reflowEnd) / 1000.0));
        t = max(t, ambient);
    }
    return t;
}

String reflow_get_ideal_profile_points() {
    ReflowProfile p = profiles[currentProfileIndex];
    String json = "[";
    json += "{\"x\":0,\"y\":"                                              + String(ambient, 1)      + "},";
    json += "{\"x\":" + String(pTimes.preheat    / 1000) + ",\"y\":"      + String(p.soakTemp, 1)   + "},";
    json += "{\"x\":" + String(pTimes.soakEnd    / 1000) + ",\"y\":"      + String(p.soakTemp, 1)   + "},";
    json += "{\"x\":" + String(pTimes.reflowPeak / 1000) + ",\"y\":"      + String(p.reflowTemp, 1) + "},";
    json += "{\"x\":" + String(pTimes.reflowEnd  / 1000) + ",\"y\":"      + String(p.reflowTemp, 1) + "},";
    json += "{\"x\":" + String(pTimes.processEnd / 1000) + ",\"y\":"      + String(ambient, 1)      + "}";
    json += "]";
    return json;
}