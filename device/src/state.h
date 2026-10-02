#pragma once
#include <Arduino.h>
#include "effects.h"

// Single source of truth for "what the LED is doing right now". Both the
// physical button (short/long press) and the REST API mutate this; reads
// happen once per render frame. Guarded by a spinlock because NimBLE runs
// its callbacks on its own FreeRTOS task, not the Arduino loop task.
struct DeviceState {
  Mode         mode = Mode::CandleColor;
  EffectParams params;
  uint8_t      brightness = MAX_BRIGHT;
};

void stateInit();
DeviceState stateGet();

// Partial setters — each touches only what it's given; callers read-modify-
// write via stateGet()/stateSet() for multi-field patches (see web_api.cpp).
void stateSet(const DeviceState& s);

// Button-driven helpers; return the new value for logging.
Mode    stateNextMode();
uint8_t stateNextBrightness();

// Brief "locate me" flash, overrides the active mode until it expires.
void identifyTrigger(uint32_t now);
bool identifyActive(uint32_t now);
