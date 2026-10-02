#pragma once
#include <FastLED.h>
#include "config.h"

// Effect modes, in button-cycle order. Adding a new one = one enum entry +
// one name in modeName() + one case in renderMode() — nothing else changes.
enum class Mode : uint8_t {
  CandleColor,  // warm flicker with a slow color drift (the signature look)
  ColorCycle,   // smooth rainbow hue sweep
  Breathing,    // brightness ease in/out on a drifting hue
  Solid,        // static color
  COUNT
};

// Runtime-tunable look, settable over the REST API (and by the standalone
// button for mode only — color/speed/intensity default and stay put until a
// controller sets them).
struct EffectParams {
  uint8_t hue       = 20;   // 0-255 color wheel position
  uint8_t sat       = 230;  // 0-255 saturation
  uint8_t speed     = 128;  // 0-255 animation rate; 128 = original default pace
  uint8_t intensity = 160;  // 0-255 flicker/breathing amplitude
};

// Each renders one frame into `leds`. Call once per FRAME_MS tick.
// `t` is millis() at the current frame so effects stay time-based, not
// frame-count based (steady speed regardless of loop jitter).
void fxCandleColor(CRGB* leds, uint32_t t, const EffectParams& p);
void fxColorCycle(CRGB* leds, uint32_t t, const EffectParams& p);
void fxBreathing(CRGB* leds, uint32_t t, const EffectParams& p);
void fxSolid(CRGB* leds, uint32_t t, const EffectParams& p);

// Dispatch helper.
void renderMode(Mode m, CRGB* leds, uint32_t t, const EffectParams& p);

// Brief "locate me" flash, overrides whatever mode is active.
void renderIdentify(CRGB* leds, uint32_t elapsedMs);

// Human-readable name, e.g. for GET /api/modes.
const char* modeName(Mode m);
