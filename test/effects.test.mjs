// SPDX-License-Identifier: EUPL-1.2
// The effects, run frame by frame in wled-studio-builder's preview (WebAssembly, in Node).
// PREVIEW_DIR is the folder with the compiled preview.mjs (see the README).
import assert from "node:assert/strict";
import { test } from "node:test";
import path from "node:path";
import { pathToFileURL } from "node:url";

const dir = process.env.PREVIEW_DIR;
if (!dir) throw new Error("set PREVIEW_DIR to the folder with preview.mjs");
const createModule = (await import(pathToFileURL(path.join(dir, "preview.mjs")).href)).default;

const KEY = { sx: 0, ix: 1, c1: 2, c2: 3, c3: 4, o1: 5, o2: 6, o3: 7, col0: 9, col1: 10, col2: 11, si: 12 };
const rgb = (c) => [(c >>> 16) & 255, (c >>> 8) & 255, c & 255];

async function effect(id, len, settings = {}) {
  const m = await createModule();
  m._preview_setup(len);
  const names = [];
  for (let i = 0; i < m._preview_effect_count(); i++) names.push(m.UTF8ToString(m._preview_effect_name(i)));
  assert.equal(m._preview_select(id), id);
  for (const [k, v] of Object.entries(settings)) m._preview_set(KEY[k], v);
  const frame = (t) => {
    const ptr = m._preview_frame(t) >>> 2;
    return Array.from(m.HEAPU32.subarray(ptr, ptr + len));
  };
  return { m, names, frame };
}

test("both effects sit at their fixed ids, behind hidden placeholders", async () => {
  const { m, names } = await effect(240, 10);
  assert.equal(names[240], "Studio VU");
  assert.equal(names[241], "Sky Lightning");
  assert.ok(names.slice(1, 240).every((n) => n === "RSVD"));
  assert.match(m.UTF8ToString(m._preview_effect_metadata(240)), /^Sensitivity,Fall,Peak hold,Yellow at,Red at,Smooth;Low,Mid,High;;1v;/);
});

// --- Studio VU ---------------------------------------------------------------------------

// the metadata's defaults (the preview doesn't apply them)
const vu = (settings = {}) => effect(240, 100, { sx: 128, ix: 128, c1: 128, c2: 153, c3: 26, col0: 0, col1: 0, col2: 0, ...settings });
const lit = (px) => px.filter((c) => c !== 0).length;

test("VU: the bar follows the volume, coloured by position: green, yellow, red", async () => {
  const { m, frame } = await vu({ c1: 0 });
  m._preview_audio(0);
  assert.equal(lit(frame(0)), 0);
  m._preview_audio(255);
  const px = frame(25);
  assert.equal(lit(px), 100);
  assert.deepEqual(rgb(px[10]), [0, 255, 0]);      // low zone: green
  assert.deepEqual(rgb(px[70]), [255, 200, 0]);    // from 60 %: yellow
  assert.deepEqual(rgb(px[95]), [255, 0, 0]);      // from ~84 %: red
  m._preview_audio(127.5);
  // it falls, it doesn't jump: a moment later it's still above half, and later at half
  assert.ok(lit(frame(50)) > 50);
  assert.equal(lit(frame(2000)), 50);
});

test("VU: sensitivity scales the volume", async () => {
  const { m, frame } = await vu({ sx: 64, c1: 0 });   // half the gain
  m._preview_audio(200);
  assert.equal(lit(frame(0)), Math.round(100 * (200 / 255) * 0.5));
});

test("VU: the peak dot holds, then drops", async () => {
  const { m, frame } = await vu({ c1: 255, ix: 255 }); // a 2 s hold, the fastest fall
  m._preview_audio(255);
  frame(0);
  m._preview_audio(0);
  const soon = frame(600);
  assert.equal(soon.slice(0, 98).filter((c) => c).length, 0);   // the bar has fallen
  assert.notEqual(soon[99], 0);                                  // the peak still at the top
  assert.equal(lit(frame(3500)), 0);                             // after the hold it has dropped too
});

test("VU: own colours, and smooth blending between zones", async () => {
  const { m, frame } = await vu({ col0: 0x000000ff, col1: 0x0000ff00, col2: 0x00ff0000, o1: 1, c1: 0 });
  m._preview_audio(255);
  const px = frame(0);
  const first = rgb(px[0]);                       // starts at (nearly) Low
  assert.ok(first[2] > 240 && first[0] === 0 && first[1] < 15, `first ${first}`);
  const mid = rgb(px[30]);                        // halfway to "Yellow at": a blend of Low and Mid
  assert.ok(mid[1] > 50 && mid[2] > 50, `blend ${mid}`);
  assert.deepEqual(rgb(px[99]), [255, 0, 0]);     // High at the end
});

// --- Sky Lightning ----------------------------------------------------------------------

const BASE = 0x00402060;  // a dark lilac
const sky = (settings = {}) => effect(241, 120, { sx: 255, ix: 200, c1: 128, c2: 0, col0: 0, col1: BASE, col2: 0, ...settings });
const brightness = (c) => rgb(c).reduce((a, b) => a + b, 0);

/** Run frames every 20 ms from 0 to `until`, returning them by time. */
function run(frame, until) {
  const out = new Map();
  for (let t = 0; t <= until; t += 20) out.set(t, frame(t));
  return out;
}

test("sky: the base colour between strikes, then a flash that spreads from one spot", async () => {
  const { frame } = await sky();
  const frames = run(frame, 30000);
  const first = frames.get(0);
  assert.ok(first.every((c) => c === BASE), "before the first strike: all base");
  // find the first frame with a flash, and the spread over the next 140 ms
  const times = [...frames.keys()];
  const start = times.find((t) => frames.get(t).some((c) => brightness(c) > 600));
  assert.ok(start !== undefined, "a strike happens within 30 s at Strikes 255");
  // the lit area (flash and what it leaves) grows from the spot outwards
  const wide = (t) => frames.get(t).filter((c) => brightness(c) > brightness(BASE) + 5).length;
  assert.ok(wide(start + 140) > wide(start) + 5, `it spreads: ${wide(start)} -> ${wide(start + 140)}`);
  const peak = frames.get(start).map(brightness);
  const centre = peak.indexOf(Math.max(...peak));
  assert.ok(peak[centre] > peak[Math.min(119, centre + 10)] || peak[centre] > peak[Math.max(0, centre - 10)],
    "brightest at the spot");
});

test("sky: an afterglow stays where the flash was, and fades", async () => {
  const { frame } = await sky({ c1: 255 });          // the longest afterglow (3 s)
  const frames = run(frame, 30000);
  const times = [...frames.keys()];
  const start = times.find((t) => frames.get(t).some((c) => brightness(c) > 600));
  const glowAt = (t) => Math.max(...frames.get(t).map(brightness));
  const flash = glowAt(start);
  const after = glowAt(start + 600);                  // the flash is long gone
  const later = glowAt(start + 2400);
  assert.ok(after > brightness(BASE) + 20 && after < flash, `afterglow ${after} below the flash ${flash}`);
  assert.ok(later < after, "and it fades");
});

test("sky: Overlay draws no base, for layering", async () => {
  const { frame } = await sky({ o1: 1 });
  assert.ok(frame(0).every((c) => c === 0));
});

test("sky: Flicker gives several strokes", async () => {
  const strokes = async (c2) => {
    const { frame } = await sky({ c2, ix: 255 });
    const frames = run(frame, 30000);
    const times = [...frames.keys()];
    const start = times.find((t) => frames.get(t).some((c) => brightness(c) > 600));
    // count rises in total brightness during the strike's first 400 ms
    let rises = 0, prev = 0, falling = false;
    for (let t = start; t <= start + 400; t += 20) {
      const total = frames.get(t).map(brightness).reduce((a, b) => a + b, 0);
      if (total > prev * 1.05 && falling) rises++;
      falling = total < prev;
      prev = total;
    }
    return rises;
  };
  assert.equal(await strokes(0), 0);
  assert.ok(await strokes(255) >= 2);
});
