// SPDX-License-Identifier: EUPL-1.2
// wled-studio-effects: registers this library's effects, each at a fixed effect id.
//
// A fixed id keeps presets that store it working across builds, whatever WLED adds. WLED's
// addEffect() appends at the end of its list whatever id is asked for, so the list is padded with
// hidden placeholders ("RSVD", which WLED's UI and setMode() skip) up to each id. That only works
// upwards, so the effects are registered here, in one place, in ascending id order. This file is
// also what wled-studio-builder reads for its effect manifest: keep each effect's id, metadata and
// addEffect() call in this shape.
#include "studio_effects.h"

namespace {

constexpr uint8_t kStudioVuId = 240;
const char kStudioVuData[] PROGMEM =
    "Studio VU@Sensitivity,Fall,Peak hold,Yellow at,Red at,Smooth;Low,Mid,High;;1v;sx=128,ix=128,c1=128,c2=153,c3=26";

constexpr uint8_t kSkyLightningId = 241;
const char kSkyLightningData[] PROGMEM =
    "Sky Lightning@Strikes,Size,Afterglow,Flicker,Lean,Overlay,On sound;Flash,Base,Afterglow;;1;sx=96,ix=160,c1=128,c2=128,c3=16";

// The placeholder name must contain "RSVD" (hidden by WLED's UI) and must not be empty.
const char kPadName[] PROGMEM = "RSVD";
void mode_pad(void) { SEGMENT.fill(SEGCOLOR(0)); }

// Pad up to the id, then add the effect; false if WLED already has that many effects.
// fn as a plain function pointer: WLED's own type for it is nested (WS2812FX::mode_ptr)
bool add_fixed(uint8_t id, void (*fn)(void), const char* data) {
  if (strip.getModeCount() > id) return false;
  while (strip.getModeCount() < id)
    if (strip.addEffect(strip.getModeCount(), &mode_pad, kPadName) == 255) return false;
  return strip.addEffect(id, fn, data) == id;
}

}  // namespace

class StudioEffects : public Usermod {
 public:
  void setup() override {
    // in ascending id order
    registered_ = 0;
    if (add_fixed(kStudioVuId, &mode_studio_vu, kStudioVuData)) registered_++;
    if (add_fixed(kSkyLightningId, &mode_sky_lightning, kSkyLightningData)) registered_++;
  }

  void loop() override {}  // WLED's Usermod requires it

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    JsonArray arr = user.createNestedArray(F("Studio effects"));
    arr.add(String(registered_) + " of 2 registered");
  }

 private:
  int registered_ = 0;
};

static StudioEffects studio_effects;
REGISTER_USERMOD(studio_effects);
