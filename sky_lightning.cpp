// SPDX-License-Identifier: EUPL-1.2
// Sky Lightning: lightning as it looks on a cloudy sky. A strike picks a spot; its light spreads
// outwards from there over a fraction of a second, flickers in a few return strokes, then leaves
// an afterglow that fades slowly, brightest where the flash was, the way it sticks on the retina.
//
// Controls: Strikes (how often), Size (how far a flash spreads), Afterglow (how long it stays),
// Flicker (1-4 return strokes), Lean (how lopsided strikes may be: 0 always centred, 31 anything
// from one-sided left to one-sided right), Overlay (draw no base: for a layer over another
// segment, with the blend mode Screen or Lighten), On sound (strikes come from loud sounds on the
// mic instead of the timer; Strikes is then the sensitivity: higher strikes on quieter sounds).
// Colours: Flash (white if black), Base (the sky between strikes), Afterglow (a cool white if
// black).
#include <math.h>
#include <string.h>
#include "studio_effects.h"

namespace {

struct Strike {
  uint32_t start;      // the first stroke
  uint32_t next;       // when the next strike starts
  float centre;        // LED index: where it struck
  float left, right;   // how far the flash spreads each way, LEDs
  uint8_t strokes;     // 1..4
  bool armed;          // On sound: the level has dropped back since the last strike
};

constexpr uint32_t kSpreadMs = 150;   // the flash reaches its full size in this time
constexpr uint32_t kStrokeGapMs = 70;
constexpr float kStrokeDecayMs = 45.0f;

uint32_t or_default(uint32_t c, uint32_t fallback) { return (c & 0x00ffffff) || (c >> 24) ? c : fallback; }

uint32_t next_interval() {
  // a mean of 20 s (Strikes 0) down to 1.5 s (255), spread from 0.3 to 1.7 times the mean
  const float mean = 20000.0f - 18500.0f * SEGMENT.speed / 255.0f;
  return uint32_t(mean * (0.3f + 1.4f * hw_random16() / 65535.0f));
}

// On sound: the level (0..255) that sets off a strike, from Strikes (0: only the loudest, 255: quiet).
float sound_threshold() { return 230.0f - 200.0f * SEGMENT.speed / 255.0f; }

// AudioReactive's smoothed volume (0..255), or the sound simulation without a mic.
float volume_now() {
  um_data_t* um = nullptr;
  if (!UsermodManager::getUMData(&um, USERMOD_ID_AUDIOREACTIVE)) um = simulateSound(SEGMENT.soundSim);
  return *(float*)um->u_data[0];
}

// A random lean in -1..1 within Lean's range, weighted towards the ends: a lopsided bolt should
// look lopsided, not almost centred.
float lean() {
  const float u = 2.0f * hw_random16() / 65535.0f - 1.0f;
  const float a = sqrtf(fabsf(u));
  return (SEGMENT.custom3 / 31.0f) * (u < 0 ? -a : a);
}

void new_strike(Strike* s, unsigned len) {
  s->start = strip.now;
  s->centre = float(hw_random16(len));
  // Size sets the flash's length, 2 * reach; Lean splits it between the two sides, so a
  // one-sided bolt runs twice as far its one way
  const float reach = 2.0f + 0.5f * len * SEGMENT.intensity / 255.0f;
  const float a = lean();
  s->left = reach * (1.0f - a);
  s->right = reach * (1.0f + a);
  s->strokes = 1 + (SEGMENT.custom2 * 3 + 127) / 255;
  s->next = strip.now + s->strokes * kStrokeGapMs + next_interval();
}

}  // namespace

void mode_sky_lightning(void) {
  const unsigned len = SEGLEN;
  if (len < 1) return;
  const size_t bytes = sizeof(Strike) + sizeof(float) * len * 2;
  if (!SEGENV.allocateData(bytes)) { SEGMENT.fill(SEGCOLOR(1)); return; }
  Strike* s = reinterpret_cast<Strike*>(SEGENV.data);
  float* flash = reinterpret_cast<float*>(SEGENV.data + sizeof(Strike));
  float* glow = flash + len;
  if (SEGENV.call == 0) {
    memset(SEGENV.data, 0, bytes);
    s->next = strip.now + next_interval() / 4;  // the first strike comes sooner
    SEGENV.step = strip.now;
  }
  const float dt = fminf(200.0f, float(strip.now - SEGENV.step));
  SEGENV.step = strip.now;
  if (SEGMENT.check2) {
    // On sound: strike when the level rises past the threshold; then wait until it has dropped
    // back (to 80 %) and the strike's own strokes are over, so one loud bang is one strike
    const float level = volume_now();
    const float thr = sound_threshold();
    const bool busy = s->strokes && strip.now - s->start < s->strokes * kStrokeGapMs + kSpreadMs;
    if (level < thr * 0.8f) s->armed = true;
    if (s->armed && !busy && level >= thr) { new_strike(s, len); s->armed = false; }
  } else if (int32_t(strip.now - s->next) >= 0) {
    new_strike(s, len);
  }

  // the afterglow fades over 0.2..3 s
  const float tau = 200.0f + 2800.0f * SEGMENT.custom1 / 255.0f;
  const float keep = expf(-dt / tau);

  const uint32_t age = strip.now - s->start;
  const float grown = fminf(1.0f, (age + 20.0f) / kSpreadMs);
  for (unsigned i = 0; i < len; i++) {
    float f = 0;
    if (s->strokes) {
      const float d = fabsf(float(i) - s->centre);
      const float radius = (float(i) < s->centre ? s->left : s->right) * grown;
      const float fall = d == 0 ? 1.0f : d < radius ? powf(1.0f - d / radius, 1.5f) : 0.0f;
      for (uint8_t k = 0; k < s->strokes; k++) {
        const int32_t since = int32_t(age) - int32_t(k * kStrokeGapMs);
        if (since >= 0) f = fmaxf(f, fall * (1.0f - 0.15f * k) * expf(-since / kStrokeDecayMs));
      }
    }
    flash[i] = f;
    glow[i] = fmaxf(glow[i] * keep, f * 0.35f);   // what stays: a part of the flash, fading
  }

  const uint32_t base = SEGMENT.check1 ? 0 : SEGCOLOR(1);
  const uint32_t white = or_default(SEGCOLOR(0), RGBW32(255, 255, 255, 255));
  const uint32_t after = or_default(SEGCOLOR(2), RGBW32(150, 170, 255, 60));
  for (unsigned i = 0; i < len; i++) {
    uint32_t c = color_blend(base, after, uint8_t(fminf(1.0f, glow[i]) * 255.0f));
    c = color_blend(c, white, uint8_t(fminf(1.0f, flash[i]) * 255.0f));
    SEGMENT.setPixelColor(i, c);
  }
}
