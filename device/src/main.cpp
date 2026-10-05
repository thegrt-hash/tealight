#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "effects.h"
#include "state.h"
#include "fingerprint.h"
#include "settings.h"
#include "ble_provisioning.h"
#include "espnow_device.h"
#include "power.h"

static CRGB leds[NUM_LEDS];

// ---- Non-blocking button (active-low, INPUT_PULLUP) ----
static bool     btnStable    = HIGH;
static bool     btnLastRead  = HIGH;
static uint32_t btnEdgeMs    = 0;
static uint32_t btnDownMs    = 0;
static bool     longHandled  = false;

// Short press = next mode; long press (held) = next brightness step. Both count
// as activity so the device wakes into a full live window rather than napping.
static void serviceButton(uint32_t now) {
  bool raw = digitalRead(BUTTON_PIN);

  if (raw != btnLastRead) {
    btnLastRead = raw;
    btnEdgeMs = now;
  }

  if ((now - btnEdgeMs) >= DEBOUNCE_MS && raw != btnStable) {
    btnStable = raw;
    if (btnStable == LOW) {
      btnDownMs = now;
      longHandled = false;
    } else {
      if (!longHandled) {
        Mode m = stateNextMode();
        powerNoteActivity();
        Serial.printf("[tealight] mode -> %u (%s)\n", static_cast<unsigned>(m), modeName(m));
      }
    }
  }

  if (btnStable == LOW && !longHandled && (now - btnDownMs) >= LONGPRESS_MS) {
    longHandled = true;
    uint8_t b = stateNextBrightness();
    powerNoteActivity();
    Serial.printf("[tealight] brightness -> %u\n", b);
  }
}

// Hold the button through power-on for BOOT_RESET_HOLD_MS to wipe the stored
// name and force BLE re-naming. Blocking is fine — it runs once, early.
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

  powerInit();
  bool resumed = powerResumedFromSleep();

  // Only scan for a boot-hold reset on a genuine power-on or a button wake —
  // never on a bare timer wake (keeps the 30 s listen cycle cheap).
  bool resetRequested = false;
  if (!resumed || powerWokeByButton()) resetRequested = checkBootResetHold();

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  stateInit();

  // Restore the pre-sleep look so the light comes back exactly as it was.
  DeviceState restored;
  if (resumed && powerRestoreState(restored)) stateSet(restored);

  FastLED.setBrightness(stateGet().brightness);
  FastLED.clear(true);

  settingsInit();
  if (resetRequested) {
    Serial.println("[tealight] boot-hold reset: wiping name");
    settingsClearName();
  }

  // NimBLE is only needed to name a fresh unit; skip it entirely on a timer
  // wake of an already-named device so each nap-cycle stays fast and cheap.
  bool needBle = !resumed || !settingsHasName();
  if (needBle) {
    bleInit();
    if (!settingsHasName()) bleStartAdvertising();
  }

  espnowInit();

  // Open the right-sized awake window, then tell the controller we're up.
  if (resumed && !powerWokeByButton()) powerNoteShortWindow();
  else                                 powerNoteActivity();
  espnowAnnounce();

  Serial.printf("[tealight] up — fingerprint %s, name '%s', fw %s%s\n",
                getFingerprint().c_str(), settingsName().c_str(), FW_VERSION,
                resumed ? " (resumed)" : "");
}

void loop() {
  uint32_t now = millis();

  serviceButton(now);
  espnowService(now);
  bleService(now);

  // A successful BLE naming: stop advertising, announce on ESP-NOW, and drop
  // into the normal live-window/sleep regime.
  if (bleConsumeNamedEvent()) {
    bleStopAdvertising();
    espnowAnnounce();
    powerNoteActivity();
    Serial.printf("[tealight] named '%s'\n", settingsName().c_str());
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

  // Nap if idle + Solid (see power.cpp). No-op otherwise.
  powerMaybeSleep(now, stateGet(), identifyActive(now), bleIsAdvertising());
}
