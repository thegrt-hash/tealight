#include "effects.h"

// Maps the 0-255 `speed` param to a BPM for beat8/beatsin8 (both take BPM
// directly as beats/min, not a 0-255 "rate"), so each effect picks its own
// useful BPM ceiling instead of sharing one scale.
static uint8_t speedToBpm(uint8_t speed, uint8_t maxBpm) {
  return 1 + (uint16_t)speed * (maxBpm - 1) / 255;
}

// Candle flicker with a slow color drift.
// Base hue wanders around the user's chosen hue; brightness jitters via
// Perlin noise (inoise8) so the flame breathes instead of strobing.
void fxCandleColor(CRGB* leds, uint32_t t, const EffectParams& p) {
  // `speed` scales how fast the noise field is traversed; 128 reproduces the
  // original pace (t/40, t/60, t/8 divisors).
  uint32_t st = (uint64_t)t * p.speed / 128;

  // Noise band is ~0..85 (mean ~42); recenter it on the user's hue so the
  // flicker character is preserved but the color is theirs.
  int16_t  wander = (int16_t)(inoise8(st / 40) / 3) - 42;
  uint8_t  hue    = p.hue + wander;
  uint8_t  sat    = qadd8(p.sat, inoise8(st / 60, 1000) >> 5);

  // Flame brightness: Perlin noise around a bright baseline, amplitude set
  // by `intensity`, clamped so it never fully dies.
  uint8_t flame = inoise8(st / 8, 5000);
  uint8_t bri   = qadd8(96, scale8(flame, p.intensity));

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    // Per-pixel offset so a strip/ring shimmers instead of moving as one.
    uint8_t off = inoise8(st / 8, i * 700) >> 2;
    leds[i] = CHSV(hue, sat, qsub8(bri, off));
  }
}

// Smooth rainbow hue sweep across the whole color wheel.
void fxColorCycle(CRGB* leds, uint32_t t, const EffectParams& p) {
  uint8_t bpm  = speedToBpm(p.speed, 30);
  uint8_t base = beat8(bpm, 0);
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(base + (i * (256 / max<uint16_t>(NUM_LEDS, 1))), 255, 255);
  }
}

// Breathing: brightness eases in and out (sine) around a gently drifting hue.
void fxBreathing(CRGB* leds, uint32_t t, const EffectParams& p) {
  uint8_t bpm      = speedToBpm(p.speed, 30);
  uint8_t bri      = beatsin8(bpm, 20, 255);
  uint8_t driftBpm = max<uint8_t>(1, bpm / 4);
  uint8_t hue      = p.hue + (beat8(driftBpm, 0) >> 3); // small wander, stays near p.hue
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(hue, p.sat, bri);
  }
}

// Static color.
void fxSolid(CRGB* leds, uint32_t t, const EffectParams& p) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(p.hue, p.sat, 255);
  }
}

void renderMode(Mode m, CRGB* leds, uint32_t t, const EffectParams& p) {
  switch (m) {
    case Mode::CandleColor: fxCandleColor(leds, t, p); break;
    case Mode::ColorCycle:  fxColorCycle(leds, t, p);  break;
    case Mode::Breathing:   fxBreathing(leds, t, p);   break;
    case Mode::Solid:       fxSolid(leds, t, p);       break;
    default:                fxCandleColor(leds, t, p); break;
  }
}

void renderIdentify(CRGB* leds, uint32_t elapsedMs) {
  bool on = (elapsedMs / 250) % 2 == 0; // 250ms on/off blink
  CRGB color = on ? CRGB(255, 255, 255) : CRGB::Black;
  for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = color;
}

const char* modeName(Mode m) {
  switch (m) {
    case Mode::CandleColor: return "CandleColor";
    case Mode::ColorCycle:  return "ColorCycle";
    case Mode::Breathing:   return "Breathing";
    case Mode::Solid:       return "Solid";
    default:                return "Unknown";
  }
}
