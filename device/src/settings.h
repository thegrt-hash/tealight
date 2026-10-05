#pragma once
#include <Arduino.h>

// Tiny NVS-backed store for the device's one persistent setting: its friendly
// name. "Named" doubles as "provisioned" — a named unit stops BLE advertising
// and runs purely on ESP-NOW + deep sleep; an un-named one stays discoverable.
void   settingsInit();
bool   settingsHasName();           // true once a name has been explicitly set
String settingsName();              // stored name, or getDefaultName() if unset
void   settingsSetName(const String& name);
void   settingsClearName();         // boot-hold reset -> re-enter BLE naming
