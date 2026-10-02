#pragma once
#include <Arduino.h>

// NimBLE peripheral used only for first-time WiFi provisioning:
//   InfoRead    (read)         {"fingerprint","name","fw"}       — identify before pairing
//   CredsWrite  (write)        {"ssid","pass","name"}            — hands off to wifi_manager
//   StatusNotify(read+notify)  {"status","fingerprint","ip","msg"}
//
// Advertises only while unprovisioned / after a boot-hold reset; main.cpp
// stops advertising once WiFi connects and restarts it if WiFi later fails.
void bleInit();            // call once from setup(), after fingerprint is available
void bleStartAdvertising();
void bleStopAdvertising();

// Polls wifi_manager's state and (re)publishes StatusNotify on change.
// Cheap no-op most ticks; call every loop().
void bleService(uint32_t now);
