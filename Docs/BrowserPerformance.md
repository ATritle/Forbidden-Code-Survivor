# Browser performance pass — 0.2.1

The original Low setting only bypassed postprocessing and shadows. It retained roughly 1,400 visible draw calls, full-resolution PBR shading, all street/impact point lights, full enemy geometry and per-frame animation/steering. The old 35 ms delta clamp also slowed the game itself when frames took longer.

The new renderer batches shared city boxes into instanced draws, caches materials, and keeps movable challenge signs separate. Low uses textured Phong lighting and animated geometry LODs (roughly 980 triangles per normal enemy, 1,880 for the boss and 2,930 for the rider/board). Original Meshy GLBs, rigs, textures and attack clips are preserved. Low geometry is rebound to the original skeleton by bone name. High restores the original geometry and PBR materials.

| Budget | Low | High |
| --- | --- | --- |
| Maximum render pixels | 960 × 540 equivalent | 1920 × 1080 equivalent |
| Dynamic street lights / impact lights | 0 / 0 | 4 / 2 |
| Bloom / dynamic shadows | Off | On |
| Spark slots / rain points | 900 / 450 | 3,000 / 1,500 |
| Nearby enemy animation | 20 Hz | 30 Hz |
| Enemy steering | 10 Hz | 15 Hz |

Resolution preserves the viewport's aspect ratio; these are pixel budgets, not a forced widescreen crop. UI text and controls remain at native CSS resolution. Adaptive resolution is on by default and can lower each dimension to 60% of its preset budget. It reacts to sustained frame times, ignores tab-switch stalls, and recovers resolution slowly. Settings includes the last gameplay FPS and internal resolution. Low is the default for new preferences; existing saved quality choices remain respected.

Enemy movement and combat run at a fixed 60 Hz with bounded catch-up. Steering and animation update less often, while movement still advances every simulation step. Line of sight uses exact segment/rectangle tests; the flow field rebuilds only after the player changes cells. This keeps the existing 90-enemy cap and spell tuning. Long stalls beyond the 100 ms catch-up limit still discard excess time to prevent a spiral of CPU work.

Chain geometry is preallocated instead of allocating and disposing GPU objects on every hit. Inactive gems are no longer submitted as hundreds of zero-scale instances. Floating labels update less often, the gameplay HUD avoids backdrop blur, and Low does not allocate the bloom compositor unless High is selected.

## Measurements and scope

Local Chromium/WebGL stress scenario: all sixteen base spells, 74 surviving moving enemies with very large health pools, five seconds of warmup, then ten seconds of measured wall-clock frames. Audio was muted and adaptive resolution disabled for measurement. Only one game tab was active. This is a short local test, not a promise for other hardware or a long-run benchmark.

| Build | Internal resolution | Mean FPS | Mean CPU/frame | Mean draw calls | Mean triangles |
| --- | --- | --- | --- | --- | --- |
| Original Low | 1280 × 720 | 10.2 | 12.44 ms | 1,437 | 383,525 |
| Batched Low | 960 × 540 | 23.8 | 4.48 ms | 130 | 346,878 |
| Low with lightweight shading and LODs | 960 × 540 | 46.6 | 3.63 ms | 130 | 137,179 |

The approximately 4.6× improvement combines batching, cheaper shading, LODs and lower resolution. It is not an equal-resolution shader-only comparison. The original test advanced roughly five seconds of game time during fifteen wall-clock seconds; the fixed-step version advanced roughly fifteen. Render and combat samples therefore differ in progression as well as presentation. The final Low measurement's 95th percentile frame interval was 27.8 ms. High remains expensive in this stress case (about 10.7 FPS locally) and is intended for stronger GPUs; use Low and adaptive resolution for responsiveness.

Raw local measurements: `Saved/BrowserPerf-Before.json`, `BrowserPerf-After-Low.json`, `BrowserPerf-After-LOD.json`, `BrowserPerf-After-High.json`. Run `?profile=1&quality=low` or `?profile=1&quality=high` to repeat the visible opt-in test without changing normal saves. It stops the simulation after measurement and displays its result. Refresh without those parameters to play normally.

Run `node --test Web/tests/*.test.mjs` for gameplay rules, pixel/adaptive budgets, exact wall occlusion and low-LOD provenance/skeleton compatibility. Rebuild geometry LODs after regenerating full models with `blender --background --python Scripts/build_browser_lods.py`. This is a local build step; the resulting geometry files and manifest are committed so Pages needs no Blender installation.

Final checks: all twelve Node tests passed; actual rendered Low and High scenes reported no browser errors; live Low to High to Low switching and adaptive preference reload passed. All sixteen base spells and all sixteen mutated spells dealt positive damage in the rendered testing range. Those combat reports are saved under `Saved/BrowserPerf-CombatRegression.json` and `Saved/BrowserPerf-MutationRegression.json`.
