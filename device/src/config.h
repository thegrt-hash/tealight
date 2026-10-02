#pragma once
#include <Arduino.h>

// ---- Firmware ----
constexpr const char* FW_VERSION = "1.1.0";

// ---- Hardware pins (XIAO ESP32-C3 GPIO numbers) ----
// XIAO ESP32-C3 silk: D0=GPIO2, D1=GPIO3, D2=GPIO4, D3=GPIO5, D4=GPIO6, ...
constexpr uint8_t LED_PIN    = 2;   // D0  -> WS2812 DIN
constexpr uint8_t BUTTON_PIN = 3;   // D1  -> push button to GND (INPUT_PULLUP)

// ---- LED strip ----
constexpr uint16_t NUM_LEDS   = 1;   // single NeoPixel; bump for a ring/strip
constexpr uint8_t  MAX_BRIGHT = 160; // 0-255 cap (eyes + battery friendly)

// ---- Animation timing ----
constexpr uint16_t FRAME_MS = 16;   // ~60 fps render cadence

// ---- Button ----
constexpr uint16_t DEBOUNCE_MS      = 30;
constexpr uint16_t LONGPRESS_MS     = 600;   // held = brightness step
constexpr uint16_t BOOT_RESET_HOLD_MS = 3000; // held at power-on = wipe WiFi creds, re-enter BLE pairing

// ---- WiFi ----
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000; // per-attempt budget
constexpr uint8_t  WIFI_MAX_BOOT_RETRIES   = 5;     // boot-time attempts before falling back to BLE

// ---- Identify (locate-me flash) ----
constexpr uint32_t IDENTIFY_FLASH_MS = 2000;

// ---- mDNS ----
constexpr const char* MDNS_SERVICE = "tealight";
constexpr const char* MDNS_PROTO   = "tcp";
constexpr uint16_t    API_PORT     = 80;

// ---- BLE provisioning ----
// Freshly generated, project-specific UUIDs (not reused tutorial UUIDs).
constexpr const char* BLE_SVC_UUID         = "e2ebef84-3a8f-4682-89dd-5d57827d5eaa";
constexpr const char* BLE_CHR_CREDS_UUID   = "53c2f022-9349-49ef-8b56-903bfa3637b6"; // write: {ssid,pass,name}
constexpr const char* BLE_CHR_STATUS_UUID  = "0886dd9c-de2e-4b35-8f33-01446fb6fe39"; // read+notify: {status,fingerprint,ip,msg}
constexpr const char* BLE_CHR_INFO_UUID    = "caa25f32-b6f0-46cc-8cf6-691efcb3994e"; // read: {fingerprint,name,fw}
