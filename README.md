# Forbidden Code: Survivor

A cyberpunk deckbuilding survivor. Ride through rain-soaked neon streets, combine forbidden energy programs and fight escalating cyborg hordes.

**[Play browser 0.5.0](https://atritle.github.io/Forbidden-Code-Survivor/)** · **[Download Windows 0.4.7](https://github.com/ATritle/Forbidden-Code-Survivor/releases/download/v0.4.7-windows/ForbiddenCode-Survivor-0.4.7-Setup.exe)** · [Windows release/checksum](https://github.com/ATritle/Forbidden-Code-Survivor/releases/tag/v0.4.7-windows)

## Two editions

- **Browser:** Native WebGL 2, five riders with perks, three two-card presets per rider, rider-specific starter unlocks, all sixteen programs in the draft, thirty-two evolutions, seven risk contracts, district objectives, phased bosses, captures, research, settings, a music player and a multi-card testing range. Enemies route through ramps to the rider's elevation, including bridges with a separate street beneath them.
- **Windows:** Unreal Engine 5 Shipping build with the native city, Nanite, Lumen and Niagara. The existing verified installer is 731,296,939 bytes. The [Windows source branch](https://github.com/ATritle/Forbidden-Code-Survivor/tree/windows/forbidden-routines-overhaul) is the native 0.4.7 source; main is the browser publishing branch.

Rendering budgets and display controls are adapted for browsers. The editions keep separate progression saves. [Release details, adaptations and rollback](Docs/Browser-0.5.0.md).

## Controls and progression

WASD/arrows move; Shift boosts; wheel zooms; E interacts; middle mouse marks; Q changes target priority; Tab inspects or resumes a deferred draft; ESC backs out of menus. Touch movement and standard-mapped gamepads are supported.

Choose a rider and one of three saved presets. Click to add or remove exactly two starting cards. Four starters are free per rider; other starters cost 75/150/300 fragments by rarity. Every program remains available in the in-run draft. Programs reach rank three, then choose an exclusive evolution. Runs have two rerolls and one banish.

The shared leaderboard keeps each browser identity's best three submitted runs and displays the global top ten. New entries include immutable rider/card/rank/evolution details; hover, focus or tap a score to inspect them. Practice is unranked. Older scores are preserved with a legacy-details notice.

## Run and validate locally

Run `node Web/serve.mjs`, then open `http://127.0.0.1:4173`. On Windows, `Play-Browser.cmd` and `Stop-Browser.cmd` manage the local server. No npm installation or build is required; enable WebGL 2 hardware acceleration.

```text
python Scripts/export_browser_rules.py
node --test Web/tests/*.test.mjs
node --test Services/Leaderboard/tests/*.test.mjs
```

Use Low + Adaptive Resolution on slower hardware. `?profile=1&quality=low&evolution=1` runs an unranked stress profile; use high and/or evolution=2 for the alternate budgets/branches. `?qa=1` adds visible diagnostics and isolates saves in session storage.

## Publishing and assets

Main deploys `Web/` through GitHub Pages after checks pass. Native authored data is pinned in `GameData/browser-native-source.json`; the exporter reads that snapshot so CI does not need the full Windows asset tree. See the release notes before capturing a newer native snapshot.

Approved art provenance is recorded in `Web/assets/model-manifest.json`, `art-manifest.json` and `parity-manifest.json`. Native assets are required to rebuild Windows and regenerate the Meshy conversions; the installer already contains the cooked assets needed to play. Three.js is vendored under its MIT license in `Web/vendor/three/LICENSE`. That third-party license grants no additional rights to the game code or art.
