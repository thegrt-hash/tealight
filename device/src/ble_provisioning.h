#pragma once
#include <Arduino.h>

// NimBLE peripheral used only for first-run naming (a device with no name is
// not yet "provisioned"). Once named it stops advertising and lives on ESP-NOW.
//   InfoRead    (read)         {"fingerprint","name","fw"}  — identify before pairing
//   NameWrite   (write)        {"name"}                     — adopts + persists the name
//   StatusNotify(read+notify)  {"status","fingerprint","name"}
//
// Advertises only while un-named / after a boot-hold reset.
void bleInit();            // call once from setup(), after settings are loaded
void bleStartAdvertising();
void bleStopAdvertising();
bool bleIsAdvertising();
void bleService(uint32_t now);

// Returns true exactly once after a successful NameWrite, so main.cpp can stop
// advertising, announce over ESP-NOW, and drop into the normal sleep regime.
bool bleConsumeNamedEvent();
