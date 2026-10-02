#pragma once
#include <Arduino.h>

enum class WifiState : uint8_t { IDLE, CONNECTING, CONNECTED, FAILED };

// Loads any stored credentials and, if present, starts connecting. Call once
// from setup() after fingerprint/BLE/state init.
void wifiInit();

// Non-blocking state machine; call every loop() iteration.
void wifiService(uint32_t now);

bool      wifiHasCredentials();
WifiState wifiGetState();
String    wifiGetIp();   // "" unless CONNECTED
String    wifiGetName();  // friendly name (defaults to getDefaultName())

// Called from BLE provisioning on a CredsWrite. Persists creds and begins
// connecting immediately (resets the boot-retry budget).
void wifiSetCredentials(const String& ssid, const String& pass, const String& name);

// Wipes stored creds and disconnects; used by the boot-hold-button reset.
void wifiClearCredentials();
