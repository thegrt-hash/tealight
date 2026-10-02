#include <freertos/FreeRTOS.h>
#include "state.h"
#include "config.h"

static DeviceState gState;
static portMUX_TYPE gStateMux = portMUX_INITIALIZER_UNLOCKED;

// Brightness steps cycled by long-press (unchanged from the original firmware).
static const uint8_t kBrightSteps[] = {40, 90, MAX_BRIGHT};
static uint8_t       gBrightIdx     = sizeof(kBrightSteps) - 1;

static uint32_t gIdentifyStartMs = 0;
static bool     gIdentifying     = false;

void stateInit() {
  taskENTER_CRITICAL(&gStateMux);
  gState = DeviceState{};
  gState.brightness = kBrightSteps[gBrightIdx];
  taskEXIT_CRITICAL(&gStateMux);
}

DeviceState stateGet() {
  taskENTER_CRITICAL(&gStateMux);
  DeviceState copy = gState;
  taskEXIT_CRITICAL(&gStateMux);
  return copy;
}

void stateSet(const DeviceState& s) {
  taskENTER_CRITICAL(&gStateMux);
  gState = s;
  taskEXIT_CRITICAL(&gStateMux);
}

Mode stateNextMode() {
  taskENTER_CRITICAL(&gStateMux);
  gState.mode = static_cast<Mode>(
      (static_cast<uint8_t>(gState.mode) + 1) % static_cast<uint8_t>(Mode::COUNT));
  Mode m = gState.mode;
  taskEXIT_CRITICAL(&gStateMux);
  return m;
}

uint8_t stateNextBrightness() {
  gBrightIdx = (gBrightIdx + 1) % (sizeof(kBrightSteps) / sizeof(kBrightSteps[0]));
  uint8_t b = kBrightSteps[gBrightIdx];
  taskENTER_CRITICAL(&gStateMux);
  gState.brightness = b;
  taskEXIT_CRITICAL(&gStateMux);
  return b;
}

void identifyTrigger(uint32_t now) {
  gIdentifyStartMs = now;
  gIdentifying     = true;
}

bool identifyActive(uint32_t now) {
  if (!gIdentifying) return false;
  if (now - gIdentifyStartMs >= IDENTIFY_FLASH_MS) {
    gIdentifying = false;
    return false;
  }
  return true;
}
