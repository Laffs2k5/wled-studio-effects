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
PREVIEW_DIR=/tmp/fx node --test test/
```

CI does the same, with the builder at a pinned commit (`.github/workflows/ci.yml`).

## Licence

EUPL-1.2 ([LICENSE](LICENSE)), the same as WLED. The effects are written against WLED's effect
API; no WLED code is copied.
