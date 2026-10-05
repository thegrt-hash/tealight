#include <esp_sleep.h>
#include <FastLED.h>
#include "power.h"
#include "config.h"
#include "settings.h"

// Survives deep sleep (RTC fast memory). Magic guards against a cold boot where
// this region is garbage rather than a genuine resume.
static constexpr uint32_t RTC_MAGIC = 0x7EA11697;
RTC_DATA_ATTR static uint32_t    gRtcMagic = 0;
RTC_DATA_ATTR static DeviceState gRtcState;

static esp_sleep_wakeup_cause_t gWakeCause = ESP_SLEEP_WAKEUP_UNDEFINED;
static uint32_t                 gLastActivityMs = 0;

void powerInit() {
  gWakeCause = esp_sleep_get_wakeup_cause();
}

bool powerResumedFromSleep() {
  return gWakeCause == ESP_SLEEP_WAKEUP_TIMER ||
         gWakeCause == ESP_SLEEP_WAKEUP_GPIO;
}

bool powerWokeByButton() {
  return gWakeCause == ESP_SLEEP_WAKEUP_GPIO;
}

bool powerRestoreState(DeviceState& out) {
  if (gRtcMagic != RTC_MAGIC) return false;
  out = gRtcState;
  return true;
}

void powerSaveState(const DeviceState& s) {
  gRtcState = s;
  gRtcMagic = RTC_MAGIC;
}

void powerNoteActivity() {
  gLastActivityMs = millis();
}

void powerNoteShortWindow() {
  // Pretend the live window began (LIVE_WINDOW_MS - RX_LISTEN_MS) ago, so the
  // device stays awake only for the brief listen window unless a command lands
  // (which calls powerNoteActivity() and re-opens the full window).
  uint32_t now = millis();
  gLastActivityMs = now - (LIVE_WINDOW_MS - RX_LISTEN_MS);
}

void powerMaybeSleep(uint32_t now, const DeviceState& s, bool identifyActive, bool bleAdvertising) {
  if (!settingsHasName())        return; // fresh unit stays awake for BLE naming
  if (bleAdvertising)            return;
  if (identifyActive)            return;
  if (s.mode != Mode::Solid)     return; // animated modes need the MCU awake
  if ((now - gLastActivityMs) < LIVE_WINDOW_MS) return; // still in the live window

  // Eligible — stash state and nap. The WS2812 holds its last latched colour
  // while powered, so the light stays lit through the sleep.
  powerSaveState(s);
  FastLED.show(); // make sure the final colour is on the wire before we go dark

  esp_sleep_enable_timer_wakeup((uint64_t)IDLE_WAKE_INTERVAL_S * 1000000ULL);
  // Button to GND, active-low: wake when the pin reads LOW.
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);

  Serial.flush();
  esp_deep_sleep_start(); // does not return
}
