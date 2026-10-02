#pragma once
#include <Arduino.h>

// Starts advertising _tealight._tcp with fp/name/fw TXT records so the
// controller can find this device and re-resolve it after an IP change.
// Call once after WiFi connects.
void mdnsStart(const String& hostname, const String& fingerprint, const String& name);
void mdnsStop();
