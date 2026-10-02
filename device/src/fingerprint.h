#pragma once
#include <Arduino.h>

// Stable device identity, independent of WiFi IP (and of WiFi MAC
// randomization) — derived from the chip's factory-burned efuse MAC. Used
// both over BLE (so a central can identify a device before it's provisioned)
// and in the mDNS TXT record (so the controller can re-associate a device
// after its IP changes).
String getFingerprint();  // 12 lowercase hex chars, e.g. "a1b2c3d4e5f6"
String getDefaultName();  // "Tealight-" + last 4 hex chars, e.g. "Tealight-E5F6"
String getMdnsHostname(); // "tealight-" + fingerprint — always a valid DNS label,
                          // unlike the free-form friendly `name` a user can set
