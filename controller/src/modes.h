#pragma once

// Mirror of the device's Mode enum (tealight/device/src/effects.h), in the same
// order. The controller no longer round-trips to the device for the mode list
// (devices sleep), so it serves this static copy. Keep in sync with the device.
static const char* const TL_MODE_NAMES[] = {
  "CandleColor",
  "ColorCycle",
  "Breathing",
  "Solid",
};
constexpr int TL_MODE_COUNT = sizeof(TL_MODE_NAMES) / sizeof(TL_MODE_NAMES[0]);

inline const char* tlModeName(int i) {
  return (i >= 0 && i < TL_MODE_COUNT) ? TL_MODE_NAMES[i] : "";
}
