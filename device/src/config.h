#pragma once
#include <Arduino.h>

// ---- Firmware ----
constexpr const char* FW_VERSION = "2.0.0"; // 2.x = ESP-NOW control + deep-sleep

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
constexpr uint16_t BOOT_RESET_HOLD_MS = 3000; // held at power-on = wipe name, re-enter BLE naming

// ---- Identify (locate-me flash) ----
constexpr uint32_t IDENTIFY_FLASH_MS = 2000;

// ---- ESP-NOW fleet control ----
// The whole fleet shares ONE fixed 2.4 GHz channel. The controller is a WiFi
// STA on your home network, so its radio sits on the home AP's channel — set
// your router's 2.4 GHz radio to a FIXED channel and put that number here (and
// in controller/src/config.h). If the two disagree, devices never hear a word.
constexpr uint8_t FLEET_CHANNEL = 1;

// ---- Two-tier sleep ----
// Only Solid mode sleeps (the WS2812 latches its colour with no refresh).
// Animated modes must keep the MCU awake to push frames, so they never sleep.
//   - After any command/button ("live window") the device stays awake + RX so
//     repeated tweaks land instantly.
//   - When idle it deep-sleeps, waking every IDLE_WAKE_INTERVAL_S to open a
//     brief RX_LISTEN_MS window (and a button press always wakes immediately).
constexpr uint32_t LIVE_WINDOW_MS       = 5UL * 60UL * 1000UL; // awake+listening after a poke
constexpr uint32_t IDLE_WAKE_INTERVAL_S = 30;   // deep-sleep timer-wake cadence
constexpr uint32_t RX_LISTEN_MS         = 150;  // listen window opened on each timer wake

// ---- BLE naming (first-run only) ----
// Freshly generated, project-specific UUIDs (not reused tutorial UUIDs). A
// fresh/un-named unit advertises these so the controller can find and name it;
// once named it stops advertising and lives purely on ESP-NOW. The CREDS UUID
// is kept for wire-compat but now carries {"name": "..."} only — no WiFi creds.
constexpr const char* BLE_SVC_UUID         = "e2ebef84-3a8f-4682-89dd-5d57827d5eaa";
constexpr const char* BLE_CHR_CREDS_UUID   = "53c2f022-9349-49ef-8b56-903bfa3637b6"; // write: {name}
constexpr const char* BLE_CHR_STATUS_UUID  = "0886dd9c-de2e-4b35-8f33-01446fb6fe39"; // read+notify: {status,fingerprint,name}
constexpr const char* BLE_CHR_INFO_UUID    = "caa25f32-b6f0-46cc-8cf6-691efcb3994e"; // read: {fingerprint,name,fw}
