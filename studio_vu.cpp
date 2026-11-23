// SPDX-License-Identifier: EUPL-1.2
// Studio VU: a classic VU meter on the mic. The bar grows from the segment's start with the
// volume; its colour is set by position (low, mid, high), so it turns from green through yellow
// to red as it reaches the far end. It rises at once and falls at an adjustable rate, and a peak
// dot holds for a while before it drops. Two segments running it on the same sound stay in step.
//
// Controls: Sensitivity (gain), Fall (how fast the bar falls), Peak hold (0: no peak dot),
// Yellow at and Red at (where the zones start), Smooth (blend between the zone colours).
// Colours: Low, Mid, High; a black slot means the classic green, yellow, red.
#include <math.h>
#include "studio_effects.h"

namespace {

struct VuState {
  float level;     // 0..1, what the bar shows
  float peak;      // 0..1
  uint32_t peak_at;
  uint32_t last;
};

uint32_t or_default(uint32_t c, uint32_t fallback) { return (c & 0x00ffffff) || (c >> 24) ? c : fallback; }

float volume_now() {
  um_data_t* um = nullptr;
  if (!UsermodManager::getUMData(&um, USERMOD_ID_AUDIOREACTIVE)) um = simulateSound(SEGMENT.soundSim);
  return *(float*)um->u_data[0];  // volumeSmth, 0..255
}

}  // namespace

void mode_studio_vu(void) {
  const unsigned len = SEGLEN;
  if (len < 1) return;
  if (!SEGENV.allocateData(sizeof(VuState))) { SEGMENT.fill(0); return; }
  VuState* st = reinterpret_cast<VuState*>(SEGENV.data);
  if (SEGENV.call == 0) *st = {0, 0, strip.now, strip.now};
  const float dt = fminf(0.2f, (strip.now - st->last) / 1000.0f);
  st->last = strip.now;

  // gain 0..2 (128 = 1), then rise at once, fall at 0.2..5 bar lengths a second
  const float target = fminf(1.0f, volume_now() / 255.0f * SEGMENT.speed / 128.0f);
  const float fall = 0.2f + 4.8f * SEGMENT.intensity / 255.0f;
  st->level = target > st->level ? target : fmaxf(target, st->level - fall * dt);

  // the peak dot holds for 0..2 s, then drops at the same rate as the bar
  const float hold = 2.0f * SEGMENT.custom1 / 255.0f;
  if (st->level >= st->peak) { st->peak = st->level; st->peak_at = strip.now; }
  else if ((strip.now - st->peak_at) / 1000.0f > hold) st->peak = fmaxf(st->level, st->peak - fall * dt);

  const uint32_t low = or_default(SEGCOLOR(0), RGBW32(0, 255, 0, 0));
  const uint32_t mid = or_default(SEGCOLOR(1), RGBW32(255, 200, 0, 0));
  const uint32_t high = or_default(SEGCOLOR(2), RGBW32(255, 0, 0, 0));
  const float yellow_at = SEGMENT.custom2 / 255.0f;
  const float red_at = fmaxf(yellow_at, SEGMENT.custom3 / 31.0f);
  auto colour_at = [&](unsigned i) -> uint32_t {
    const float f = (i + 0.5f) / len;
    if (!SEGMENT.check1) return f < yellow_at ? low : f < red_at ? mid : high;
    if (f < yellow_at) return color_blend(low, mid, uint8_t(255.0f * f / fmaxf(yellow_at, 0.001f)));
    if (f < red_at) return color_blend(mid, high, uint8_t(255.0f * (f - yellow_at) / fmaxf(red_at - yellow_at, 0.001f)));
    return high;
  };

  const unsigned lit = unsigned(st->level * len + 0.5f);
  for (unsigned i = 0; i < len; i++) SEGMENT.setPixelColor(i, i < lit ? colour_at(i) : 0);
  if (SEGMENT.custom1 > 0) {
    const unsigned p = unsigned(st->peak * len + 0.5f);
    if (p > lit && p > 0) SEGMENT.setPixelColor(p - 1, colour_at(p - 1));
  }
}
