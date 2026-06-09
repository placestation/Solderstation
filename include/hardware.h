#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <Wire.h>
#include "TCA9554.h"
#include "config.h"

// --- Hardware Objects ---
extern TCA9554 expander;

// --- Hardware Functions ---
void initI2C();
void initExpander();
void initBacklight();
void customLcdReset();

#endif // HARDWARE_H