// SPDX-License-Identifier: EUPL-1.2
// Studio cue: a short gauge drawn on top of whatever the strip shows, without a segment.
//
// A cue is a few LEDs, `lit` of `len` in one colour and the rest in another, shown for a while and
// then gone. Drawn as a segment, it changes the device's segment list: the looks running under it
// lose their current preset, other software sees segments come and go, and its index depends on how
// many segments the running look has. Drawn here, in WLED's overlay hook (after every segment is
// blended into the frame, before it is sent), it leaves the segments alone. Global brightness and
// the current limiter still apply, as they do to segments.
//
// Set through the JSON API (POST /json/state; a preset may carry it too):
//   {"cue": {"start": 30, "len": 9, "lit": 3, "rev": true,
//            "on": [255, 200, 0], "off": [0, 0, 0, 24], "ms": 3000}}
//   start  first LED (absolute index on the strip)       len  LEDs in the gauge, 1-64 (default 9)
//   lit    how many are lit, from the gauge's first LED  rev  the gauge's first LED is start+len-1
//   on     colour of the lit LEDs (default white)        off  colour of the others; leave it out to
//          as [r,g,b] / [r,g,b,w] or hex "RRGGBB" /           let the strip show through them
//          "WWRRGGBB"                                     ms   how long, up to 60000 (default 3000)
//   {"cue": {"ms": 0}} clears it. A new cue replaces the running one.
// /json/info lists "Studio cue" with "showing" or "idle".
//
// Not compiled for WebAssembly (the effect preview and the virtual twin): there is no strip output
// to draw over there, and the preview shim has no overlay hook.
#ifndef __EMSCRIPTEN__
#include "wled.h"

namespace {

constexpr uint16_t kMaxLen = 64;
constexpr uint32_t kMaxMs = 60000;

// [r,g,b(,w)] or "RRGGBB" / "WWRRGGBB"; `fallback` if missing or malformed
uint32_t parse_colour(JsonVariant v, uint32_t fallback, bool* given) {
  *given = false;
  if (v.is<JsonArray>()) {
    JsonArray a = v.as<JsonArray>();
    if (a.size() < 3) return fallback;
    *given = true;
    return RGBW32(a[0] | 0, a[1] | 0, a[2] | 0, a.size() > 3 ? (a[3] | 0) : 0);
  }
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    size_t n = s ? strlen(s) : 0;
    if (n != 6 && n != 8) return fallback;
    char* end = nullptr;
    uint32_t c = strtoul(s, &end, 16);
    if (end != s + n) return fallback;
    *given = true;
    return c;  // WLED's colour layout is 0xWWRRGGBB, as written
  }
  return fallback;
}

}  // namespace

class StudioCue : public Usermod {
 public:
  void setup() override {}

  void loop() override {
    if (active_ && (long)(millis() - until_) >= 0) {
      active_ = false;
      strip.trigger();  // redraw without it, even if nothing animates
    }
  }

  void readFromJsonState(JsonObject& root) override {
    JsonObject c = root["cue"];
    if (c.isNull()) return;
    uint32_t ms = c["ms"] | (uint32_t)3000;
    if (ms == 0) {
      if (active_) { active_ = false; strip.trigger(); }
      return;
    }
    active_ = false;  // the draw skips while the fields change
    start_ = c["start"] | 0;
    len_ = constrain((int)(c["len"] | 9), 1, (int)kMaxLen);
    lit_ = constrain((int)(c["lit"] | 0), 0, (int)len_);
    rev_ = c["rev"] | false;
    bool given;
    on_ = parse_colour(c["on"], RGBW32(255, 255, 255, 0), &given);
    off_ = parse_colour(c["off"], 0, &given);
    drawOff_ = given;
    until_ = millis() + min(ms, kMaxMs);
    active_ = true;
    strip.trigger();
  }

  void handleOverlayDraw() override {
    if (!active_) return;
    const unsigned total = strip.getLengthTotal();
    for (unsigned i = 0; i < len_; i++) {
      const bool lit = i < lit_;
      if (!lit && !drawOff_) continue;
      const unsigned idx = start_ + (rev_ ? len_ - 1 - i : i);
      if (idx < total) strip.setPixelColor(idx, lit ? on_ : off_);
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    JsonArray arr = user.createNestedArray(F("Studio cue"));
    arr.add(active_ ? F("showing") : F("idle"));
  }

 private:
  volatile bool active_ = false;  // written from the web server's task, read in the loop
  unsigned long until_ = 0;
  uint16_t start_ = 0;
  uint16_t len_ = 9;
  uint16_t lit_ = 0;
  bool rev_ = false;
  bool drawOff_ = false;
  uint32_t on_ = 0;
  uint32_t off_ = 0;
};

static StudioCue studio_cue;
REGISTER_USERMOD(studio_cue);
#endif  // __EMSCRIPTEN__
