# Lower-city street lighting

`ANeonCityDirector` creates `ANeonStreetLighting` when the saved Neon District starts. No map regeneration or asset generation is needed. The regular `Play-MVP.cmd` launcher includes it automatically.

The pass adds 27 instanced fixtures beside the plaza, market lanes, tower approaches, elevated promenade/service deck, shrine, and subway. The 390 cm street poles have dark metal housings, pale trim, overhanging lamp heads, luminous diffusers, and curb footings. Underpass brackets put their light below the bridge. Fixtures have no collision and cannot affect navigation, preserving existing ramps and crowd routes.

Warm-white 1,800 lumen spotlights project down and slightly into the street, with broad soft cones and 1,350 cm attenuation. The underpass uses cool white. A restrained 0.85 lux, blue-white, shadowless directional fill exposes dark building silhouettes without changing camera exposure or the neon material palette.

Every 0.2 seconds, the actor selects up to ten nearby spotlights and at most two near shadow casters. Lights fade with player distance from 2,300 to 3,900 cm; all are disabled when the player is in the distant elevated testing range. Small fixture geometry uses three instanced mesh batches. Existing combat effect limits are unchanged. `Brightness`, `MaxActiveLights`, and `MaxShadowLights` are exposed for editor/Blueprint tuning, clamped to 0–2, 0–16, and 0–3 respectively.

Run `Scripts/Test-StreetLighting.ps1` for matching plaza/market/subway captures and the existing city navigation, collision, landing and combat regression. `-IncludeBenchmark` also compares the existing 180-enemy, fifteen-spell, eight-explosion-requests-per-second scenario with the lighting enabled and disabled. The test-only `-StreetLightingBefore` switch omits this entire new pass. Captures go to `Saved/Screenshots/StreetLighting-*-Before.png` and `*-After.png`; logs go to `Saved/StreetLighting*.log`. Tests use automation saves.

## Verified October 18, 2026

`Saved/StreetLightingBuild.log` records the successful final editor build. The final before/after capture harness passes in `Saved/StreetLightingFinalValidationConsole.log`, including fixture counts, local/shadow light budgets and distant-range culling. Random challenge signs and their lights are suspended in these comparison views. `Saved/StreetLightingCityRegression.log` passes the existing six-route navigation, physical ramp ascent, wall collision, landing, auto-combat, unique draft and offscreen spawning checks.

The final 1600×900 standalone combat sample kept 180 cyborgs active with fifteen offensive spells, rain, Lumen and eight cinematic explosion requests per second. After five seconds of warmup, 1,108 sampled frames over ten seconds averaged 9.03 ms (about 111 FPS), with p95 10.09 ms and mean GPU time 5.00 ms. The lighting-disabled baseline averaged 8.73 ms, p95 9.78 ms, and mean GPU 4.77 ms. These are local editor-launched measurements, not a packaged-build or other-hardware guarantee; normal random gameplay and effect admission counts differ slightly between runs. Raw samples and summaries are `Saved/Benchmarks/StreetLighting180Before.csv`, `StreetLighting180After.csv` and the corresponding `-summary.json` files.
