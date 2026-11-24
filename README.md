# wled-studio-effects

Own effects for [WLED](https://github.com/wled/WLED), made with
[wled-studio](https://github.com/Laffs2k5/wled-studio) and built into firmware with
[wled-studio-builder](https://github.com/Laffs2k5/wled-studio-builder). One WLED usermod
(`wled-studio-effects`) that registers each effect at a **fixed effect id**, so presets that store
the id keep working across firmware builds.

| Id | Effect | What it does |
|---|---|---|
| 240 | **Studio VU** | A classic VU meter on the mic: a bar grows from the segment's start with the volume, coloured by position from Low through Mid to High (green, yellow, red by default). It rises at once, falls at an adjustable rate, and a peak dot holds before it drops. Two segments running it on the same sound stay in step. |
| 241 | **Sky Lightning** | Lightning as it looks on a cloudy sky: a strike's light spreads outwards from a spot, flickers in a few return strokes, then leaves an afterglow that fades slowly, brightest where the flash was. Overlay mode draws no base, for a layer over another segment (blend mode Screen or Lighten). |

## Controls

**Studio VU** (`1v`: 1D, sound-reactive)
- Sensitivity: the gain (128 = 1).
- Fall: how fast the bar falls, 0.2 to 5 bar lengths a second.
- Peak hold: 0 to 2 s; 0 shows no peak dot.
- Yellow at, Red at: where the Mid and High zones start.
- Smooth: blend between the zone colours instead of hard steps.
- Colours Low, Mid, High: a black slot means the classic green, yellow, red.
- As a layer over another segment it needs no Overlay option: unlit LEDs are black, and with the
  blend mode Lighten or Screen black leaves what's beneath unchanged. (Sky Lightning needs the
  option only because its Base colour would otherwise paint over the layer below.)

**Sky Lightning** (`1`: 1D)
- Strikes: how often, from about every 20 s down to every 1.5 s (with random spread).
- Size: how far a flash spreads.
- Afterglow: how long it stays, 0.2 to 3 s.
- Flicker: 1 to 4 return strokes.
- Overlay: draw no base colour.
- Colours Flash (white if black), Base (the sky between strikes), Afterglow (a cool white if black).

## Use

- **In a firmware build:** pass this folder to wled-studio-builder,
  `studio/gate.py pins/<release>.json --target <board> --usermod <path to this repo>`, or use the
  wled-studio app's Firmware view with this repo cloned into its `effects/` folder. The builder's
  effect manifest reads `studio_effects.cpp` to know where each effect lands.
- **In the browser:** clone it into wled-studio's `effects/` folder and open the Effect studio;
  the effects compile to WebAssembly and run with their own controls, and can be streamed to a
  strip before anything is flashed.
- **Ids:** 240 and up are this library's. Don't build it together with other fixed-id usermods
  (such as the builder's example at 254): the order in which usermods set up isn't defined, and the
  padding only works upwards, so whichever registers second can be refused.

## Test

The tests run each effect frame by frame in wled-studio-builder's preview (WebAssembly in Node):

```bash
source ~/emsdk/emsdk_env.sh                       # Emscripten 7.0.1
../wled-studio-builder/preview/build.sh /tmp/fx .
PREVIEW_DIR=/tmp/fx node --test test/*.test.mjs
```

CI does the same, with the builder at a pinned commit (`.github/workflows/ci.yml`).

**In real firmware:** `.github/workflows/gate.yml` builds the effects into QuinLED's Dig-Next-2
16.0.1 image through wled-studio-builder's gate (the stock image checked first, only this usermod's
objects added, the image within the app slot) and checks that the image's effect list holds them at
240 and 241. It keeps the reports and the effect list; it publishes no firmware.
It passed on 2026-11-23: only this library's three objects are new, and the image is
1,338,496 bytes. Keep the gate's work folder outside this repo's folder (the workflow uses
`RUNNER_TEMP`): PlatformIO compiles every source file under a library's folder, a toolchain
included. A pass in the preview doesn't prove the real build; the gate does.

## Ideas, not built yet

**Sky Lightning: lopsided strikes** (asked for 2026-11-24). Today a flash grows symmetrically from
its start LED and is lopsided only where the strip's end cuts it off. Wanted: each strike gets a
random lean, anywhere from "grows only to the left" through centred to "grows only to the right".
A one-sided flash reads as a bolt running across the sky from where it struck. Sketch:
- per strike, draw a lean `a` in [-1, 1] and split the flash's length between the sides:
  `left = reach * (1 - a)`, `right = reach * (1 + a)`. The total stays as Size sets it, so a
  one-sided bolt is twice as long on its one side. The afterglow follows the same shape;
- a new slider **Lean** in the free slot (custom 3, 0-31): 0 = always centred, as today; higher
  values widen the random range, up to the full left-to-right range at 31. Maybe weight the draw
  towards the extremes, so one-sided bolts don't drown among almost-centred ones;
- open: a checkbox for leaning one way only, if a strip's one end is "up";
- the id (241) doesn't change; the metadata gains the slider's name, and the frame tests gain a
  case for each extreme.

## Licence

EUPL-1.2 ([LICENSE](LICENSE)), the same as WLED. The effects are written against WLED's effect
API; no WLED code is copied.
