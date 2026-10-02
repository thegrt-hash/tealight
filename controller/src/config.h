#pragma once
#include <Arduino.h>

// ---- BLE provisioning UUIDs ----
// Must exactly match tealight/device/src/config.h — these two firmwares
// only ever agree on a device's identity/state by matching UUIDs, since
// they're separate PlatformIO projects with no shared header.
constexpr const char* BLE_SVC_UUID        = "e2ebef84-3a8f-4682-89dd-5d57827d5eaa";
constexpr const char* BLE_CHR_CREDS_UUID  = "53c2f022-9349-49ef-8b56-903bfa3637b6";
constexpr const char* BLE_CHR_STATUS_UUID = "0886dd9c-de2e-4b35-8f33-01446fb6fe39";
constexpr const char* BLE_CHR_INFO_UUID   = "caa25f32-b6f0-46cc-8cf6-691efcb3994e";

// ---- mDNS ----
constexpr const char* MDNS_SERVICE = "tealight";
constexpr const char* MDNS_PROTO   = "tcp";
constexpr uint16_t    DEVICE_PORT  = 80; // tealight's own REST API port

// ---- Controller's own web server ----
constexpr uint16_t CONTROLLER_PORT = 80;

// ---- Timing ----
constexpr uint32_t BLE_SCAN_MS             = 5000;  // discovery scan window
constexpr uint32_t BLE_PROVISION_TIMEOUT_MS = 20000; // wait for StatusNotify "connected"/"failed"
constexpr uint32_t MDNS_BROWSE_INTERVAL_MS  = 15000; // periodic registry reconciliation
constexpr uint32_t DEVICE_POLL_INTERVAL_MS  = 3000;  // background /api/state polling per device
constexpr uint32_t PROXY_TIMEOUT_MS         = 800;   // per-request timeout when proxying to a device

// ---- Storage ----
constexpr const char* REGISTRY_PATH = "/registry.json";
