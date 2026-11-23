# CLAUDE.md (wled-studio-effects)

Own WLED effects, as one usermod library, for wled-studio. Public, EUPL-1.2.

## Rules

- **Public repo: nothing about anyone's home.** No hosts, room sizes, wall names or LED counts of a
  real installation, in code, tests or docs.
- **No WLED code copied.** Write against WLED's effect API (`SEGMENT`, `SEGENV`, `strip`, colour
  helpers); the preview shim in wled-studio-builder is the reference for what is available, but
  **the gate's real build is the check**: the shim can be looser than WLED. Learned 2026-11-23:
  WLED's `mode_ptr` is `WS2812FX::mode_ptr` (so take a plain `void (*)(void)`), and a `Usermod` must
  override both `setup()` and `loop()` (pure virtual). The shim now rejects both too.
- **Fixed ids, in `studio_effects.cpp` only, ascending.** Each effect has a `constexpr uint8_t
  k...Id`, a `const char k...Data[] PROGMEM` with its metadata, and an `add_fixed(k...Id, &fn,
  k...Data)` call, in that shape: wled-studio-builder's manifest reads it. Ids never move once
  released; a removed effect leaves its id unused.
- **Sound:** read AudioReactive's data with `UsermodManager::getUMData(&um,
  USERMOD_ID_AUDIOREACTIVE)` and fall back to `simulateSound(SEGMENT.soundSim)`, as WLED's own
  effects do (FX.cpp's `getAudioData()` is static there).
- **State** goes in `SEGENV.data` (`allocateData`), initialised when `SEGENV.call == 0`; time from
  `strip.now`, so the effect runs at any frame rate.

## Git

- Push directly to `main`; atomic conventional commits, the body explains *why*. AI config
  (`CLAUDE.md`, `.claude/`) in its own commit.

## Test

- Every effect has frame tests in `test/` (Node, against the compiled preview). Set the metadata's
  defaults in the test: the preview doesn't apply them.
- After a visual change, render a time-lapse (one row per frame) and look at it; tests don't see
  whether it looks right.
- CI pins wled-studio-builder by commit (`BUILDER_COMMIT`); move it on deliberately.
