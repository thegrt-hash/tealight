#pragma once
#include <Arduino.h>

// ---- BLE naming UUIDs ----
// Must exactly match tealight/device/src/config.h — these two firmwares only
// agree on a device's identity by matching UUIDs, since they're separate
// PlatformIO projects. The CREDS UUID now carries {"name"} only (no WiFi creds).
constexpr const char* BLE_SVC_UUID        = "e2ebef84-3a8f-4682-89dd-5d57827d5eaa";
constexpr const char* BLE_CHR_CREDS_UUID  = "53c2f022-9349-49ef-8b56-903bfa3637b6";
constexpr const char* BLE_CHR_STATUS_UUID = "0886dd9c-de2e-4b35-8f33-01446fb6fe39";
constexpr const char* BLE_CHR_INFO_UUID   = "caa25f32-b6f0-46cc-8cf6-691efcb3994e";

// ---- ESP-NOW fleet control ----
// MUST equal device/src/config.h's FLEET_CHANNEL. The controller is a WiFi STA,
// so its radio sits on the home AP's channel — lock your router's 2.4 GHz radio
// to this channel, or devices (parked on FLEET_CHANNEL) never hear commands.
constexpr uint8_t FLEET_CHANNEL = 1;

// ---- Controller's own web server ----
constexpr uint16_t CONTROLLER_PORT = 80;

// ---- Timing ----
constexpr uint32_t BLE_SCAN_MS              = 5000;  // discovery scan window
constexpr uint32_t BLE_NAME_TIMEOUT_MS      = 15000; // wait for the device to accept its name
constexpr uint32_t DEVICE_QUERY_INTERVAL_MS = 5000;  // broadcast "report your state" cadence
constexpr uint32_t DEVICE_OFFLINE_MS        = 90000; // not heard in this long -> shown offline

// ---- Storage ----
constexpr const char* REGISTRY_PATH = "/registry.json";
