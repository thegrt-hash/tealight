#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "effects.h"
#include "state.h"
#include "fingerprint.h"
#include "wifi_manager.h"
#include "ble_provisioning.h"
#include "web_api.h"
#include "mdns_service.h"

static CRGB leds[NUM_LEDS];

// ---- Non-blocking button (active-low, INPUT_PULLUP) ----
static bool     btnStable    = HIGH;  // debounced level
static bool     btnLastRead  = HIGH;
static uint32_t btnEdgeMs    = 0;     // last raw change
static uint32_t btnDownMs    = 0;     // when press began
static bool     longHandled  = false;

// Short press = next mode; long press (held) = next brightness step. Works
// identically whether or not a controller is present — it just mutates the
// same shared DeviceState the REST API reads/writes.
static void serviceButton(uint32_t now) {
  bool raw = digitalRead(BUTTON_PIN);

  if (raw != btnLastRead) {        // raw edge: start debounce window
    btnLastRead = raw;
    btnEdgeMs = now;
  }

  if ((now - btnEdgeMs) >= DEBOUNCE_MS && raw != btnStable) {
    btnStable = raw;               // accept debounced level
    if (btnStable == LOW) {        // pressed down
      btnDownMs = now;
      longHandled = false;
    } else {                       // released
      if (!longHandled) {
        Mode m = stateNextMode();
        Serial.printf("[tealight] mode -> %u (%s)\n", static_cast<unsigned>(m), modeName(m));
      }
    }
  }

  // Fire long-press once while still held.
  if (btnStable == LOW && !longHandled && (now - btnDownMs) >= LONGPRESS_MS) {
    longHandled = true;
    uint8_t b = stateNextBrightness();
    Serial.printf("[tealight] brightness -> %u\n", b);
  }
}

// Hold the button through power-on for BOOT_RESET_HOLD_MS to wipe stored
// WiFi credentials and force BLE re-pairing (e.g. moved to a new network, or
// recovering a device that can't reach its old AP). Blocking is fine here —
// it only runs once, before setup() does anything else that matters.
static bool checkBootResetHold() {
  if (digitalRead(BUTTON_PIN) != LOW) return false;
  uint32_t start = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - start >= BOOT_RESET_HOLD_MS) return true;
    delay(10);
  }
  return false;
}

void setup() {
  Serial.begin(MONITOR_SPEED);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  bool resetRequested = checkBootResetHold();

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  stateInit();
  FastLED.setBrightness(stateGet().brightness);
  FastLED.clear(true);

  bleInit();
  wifiInit();

  if (resetRequested) {
    Serial.println("[tealight] boot-hold reset: wiping WiFi credentials");
    wifiClearCredentials();
  }
  if (!wifiHasCredentials()) {
    bleStartAdvertising();
  }

  webApiInit();

  Serial.printf("[tealight] up — fingerprint %s\n", getFingerprint().c_str());
}

void loop() {
  uint32_t now = millis();

  serviceButton(now);
  wifiService(now);
  bleService(now);
  webApiService();

  // React to WiFi state transitions: stop BLE once we're reachable over
  // WiFi (and (re)announce mDNS so a stale IP never lingers), restart BLE
  // advertising if boot-time connection attempts are exhausted.
  static WifiState lastWifiState = WifiState::IDLE;
  WifiState ws = wifiGetState();
  if (ws != lastWifiState) {
    if (ws == WifiState::CONNECTED) {
      bleStopAdvertising();
      mdnsStart(getMdnsHostname(), getFingerprint(), wifiGetName());
      Serial.printf("[tealight] WiFi connected, ip=%s\n", wifiGetIp().c_str());
    } else if (ws == WifiState::FAILED) {
      bleStartAdvertising();
      Serial.println("[tealight] WiFi failed, advertising BLE for provisioning");
    }
    lastWifiState = ws;
  }

  static uint32_t lastFrame = 0;
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    DeviceState s = stateGet();
    if (identifyActive(now)) {
      renderIdentify(leds, now);
    } else {
      renderMode(s.mode, leds, now, s.params);
    }
    FastLED.setBrightness(s.brightness);
    FastLED.show();
  }
}
