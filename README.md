# Forbidden Code: Survivor

A cyberpunk deckbuilding survivor. Ride a hoverboard through rain-soaked neon streets, combine forbidden energy spells, draft mutations and fight escalating cyborg hordes.

**[Play in your browser](https://ATritle.github.io/Forbidden-Code-Survivor/)** · **[Download the UE5 Windows installer](https://github.com/ATritle/Forbidden-Code-Survivor/releases/download/v0.2.0/ForbiddenCode-Survivor-Setup.exe)** · [Release notes and checksum](https://github.com/ATritle/Forbidden-Code-Survivor/releases/tag/v0.2.0)

## Two playable editions

- **Browser:** Complete self-contained WebGL 2 game. Sixteen energy spells, ten cyborg types, seven risk contracts, structural mutations, a saved pre-run loadout, a multi-spell testing range, settings and animated Meshy models. Rendering and navigation are adapted for the browser, with a 90-enemy cap.
- **Windows:** Unreal Engine 5 Shipping edition with Nanite, Lumen, Niagara and the full native city. Download the 464 MiB installer from Releases and launch using the Start Menu shortcut. The installer is an early playable test release.

The editions share the energy-spell catalog and cyberpunk theme. Native 0.3.0 has its own balance and progression rules. Their saved games are separate. See [architecture and parity limits](Docs/TwoEditions.md).


## Windows 0.3.0: Forbidden Routines

The native source now includes an eight-program draft pool, two starters, six active slots, finite ranks and 32 exclusive evolution branches. Relay hacks, a three-phase boss, captured enemy code, corrupted contract rewards, persistent hideout unlocks, new Meshy art/animations, accessibility controls and an independent Windows leaderboard extend the run loop.

Build with UE 5.8.2 using `Scripts/Package-Windows.ps1 -Installer`. Current outputs use separate `Windows-0.3.5` and `Installer-0.3.5` folders. `Play-Forbidden-Routines.cmd` launches the current local package; `Play-Windows-0.3.4.cmd`, `Play-Windows-0.3.3.cmd`, `Play-Windows-0.3.2.cmd`, `Play-Windows-0.3.1.cmd` and `Play-Windows-0.3.0.cmd` retain preceding builds. The published v0.2.0 download above remains available as the earlier release.

**Windows 0.3.5 media-player polish:** Signal Dock controls sit lower in the frame, square cover art has padding, the mute icon switches between slashed and unslashed speakers, and TRACKS opens a more compact thumbnail playlist. Generated artwork, the music-derived spectrum, seeking, saved preferences and click-only sounds are retained. [Details and validation](Docs/Windows-0.3.5.txt).

**Windows 0.3.1 maintenance update:** fixed missing ramp floors, added **CLOSE GAME** in both menus, and added **HOW TO PLAY** (F1), a 52-entry illustrated guide covering all programs/evolutions, enemies, district items, contracts, progression and settings. [Details and validation](Docs/Windows-0.3.1.txt).

[Change notes](Docs/WindowsOverhaul/RELEASE-NOTES.txt) · [Card and enemy tuning](Docs/WindowsOverhaul/CARDS-AND-ENEMIES.txt) · [61-item implementation record](Docs/WindowsOverhaul/implementation-status.json) · [Validation evidence](Docs/WindowsOverhaul/Validation) · [Baseline restore instructions](Docs/WindowsOverhaul/RESTORE.txt). The verified full baseline archive is retained in the development workspace; its checksum receipt is committed here.

The packaged test completed a 6:42 boss-victory run with six programs and earned XP/drafts. Offscreen Shipping stress tests at 1600×900/High sustained roughly 157–159 FPS on a Ryzen 7900X3D/Radeon 7900 XTX with 15 evolved attacks and 177–178 active enemies, with zero gameplay admission drops. These are local test results; see the recorded configuration and limitations.

## Controls

WASD / arrows move; mouse wheel zooms; E interacts with challenge signs; Esc opens settings. Choose up to two starting spells, collect essence and draft a card or mutation when you level up. A touch movement stick is included.

## Run locally

With Node.js installed, run `node Web/serve.mjs`, then open http://127.0.0.1:4173. Windows users can double-click `Play-Browser.cmd`; `Stop-Browser.cmd` stops that local server. No npm install or build step is needed. Modern WebGL 2 support is required.

## Development and publishing

Run `node --test Web/tests/*.test.mjs`. After changing native authored tuning, run `python Scripts/export_browser_rules.py` and commit both generated snapshots. Push relevant changes to `main` to deploy the browser game through GitHub Actions.

This repository includes browser assets and native source/configuration. Large native Unreal assets and original source art/audio remain in the development workspace, so a fresh clone cannot rebuild the Windows game without those folders. The Windows installer contains the cooked assets needed to play. See [publishing notes](Docs/GitHubPublishing.md).

Three.js is vendored under its MIT license in `Web/vendor/three/LICENSE`. Game art provenance is recorded in `Web/assets/model-manifest.json` and `Web/assets/art-manifest.json`. No license for the original game code or art is granted by the bundled third-party license.

## Browser performance update (0.2.1)

Use **Low + Adaptive Resolution** for responsiveness. Settings now displays gameplay FPS and render resolution. The renderer batches the city, uses animated low LODs and lightweight shading on Low, pools chain effects, and fixes slow motion caused by low frame rates. See [measurements and quality budgets](Docs/BrowserPerformance.md). The Windows download remains the UE5 v0.2.0 build.

## Shared leaderboard (browser 0.2.2)

Open **LEADERBOARD** in the top navigation or HUD. After a run ends, enter a username to publish the result or choose **SKIP & RETURN**. Scores reward kills, survival, levels, completed contracts and victory. The top fifty rank each browser profile's personal best. Testing runs are excluded. [Details and score rules](Docs/Leaderboard.md).
