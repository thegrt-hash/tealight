#pragma once
#include <Arduino.h>
#include "state.h"

// Two-tier power manager. See config.h for the policy knobs.
//
// Deep sleep on the ESP32-C3 is a full cold boot (no RAM retention), so the
// active state is stashed in RTC-backed memory across the nap and restored in
// setup(). Only Solid mode ever sleeps — animated modes render continuously.

void powerInit();               // read the wake cause; call first thing in setup()
bool powerResumedFromSleep();   // true if this boot came out of deep sleep
bool powerWokeByButton();       // true if the wake cause was the button GPIO

// RTC-retained state across a deep-sleep nap.
bool        powerRestoreState(DeviceState& out); // false if nothing valid stashed
void        powerSaveState(const DeviceState& s);

// Call on any command or button press to (re)open the full live window.
void powerNoteActivity();
// Call after a timer wake to open only a short RX_LISTEN_MS window.
void powerNoteShortWindow();

// Decide whether to deep-sleep now; if eligible this does NOT return (the chip
// sleeps and later cold-boots). Safe to call every loop iteration.
void powerMaybeSleep(uint32_t now, const DeviceState& s, bool identifyActive, bool bleAdvertising);
