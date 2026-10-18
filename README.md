# Forbidden Code: Survivor

A cyberpunk deckbuilding survivor. Ride a hoverboard through rain-soaked neon streets, combine forbidden energy spells, draft mutations and fight escalating cyborg hordes.

**[Play in your browser](https://ATritle.github.io/Forbidden-Code-Survivor/)** · **[Download the UE5 Windows installer](https://github.com/ATritle/Forbidden-Code-Survivor/releases/download/v0.2.0/ForbiddenCode-Survivor-Setup.exe)** · [Release notes and checksum](https://github.com/ATritle/Forbidden-Code-Survivor/releases/tag/v0.2.0)

## Two playable editions

- **Browser:** Complete self-contained WebGL 2 game. Sixteen energy spells, ten cyborg types, seven risk contracts, structural mutations, a saved pre-run loadout, a multi-spell testing range, settings and animated Meshy models. Rendering and navigation are adapted for the browser, with a 90-enemy cap.
- **Windows:** Unreal Engine 5 Shipping edition with Nanite, Lumen, Niagara and the full native city. Download the 464 MiB installer from Releases and launch using the Start Menu shortcut. The installer is an early playable test release.

Both editions share authored card/enemy tuning. Their saved games are separate. See [architecture and parity limits](Docs/TwoEditions.md).

## Controls

WASD / arrows move; mouse wheel zooms; E interacts with challenge signs; Esc opens settings. Choose up to two starting spells, collect essence and draft a card or mutation when you level up. A touch movement stick is included.

## Run locally

With Node.js installed, run `node Web/serve.mjs`, then open http://127.0.0.1:4173. Windows users can double-click `Play-Browser.cmd`; `Stop-Browser.cmd` stops that local server. No npm install or build step is needed. Modern WebGL 2 support is required.

## Development and publishing

Run `node --test Web/tests/rules.test.mjs`. After changing native authored tuning, run `python Scripts/export_browser_rules.py` and commit both generated snapshots. Push relevant changes to `main` to deploy the browser game through GitHub Actions.

This repository includes browser assets and native source/configuration. Large native Unreal assets and original source art/audio remain in the development workspace, so a fresh clone cannot rebuild the Windows game without those folders. The Windows installer contains the cooked assets needed to play. See [publishing notes](Docs/GitHubPublishing.md).

Three.js is vendored under its MIT license in `Web/vendor/three/LICENSE`. Game art provenance is recorded in `Web/assets/model-manifest.json` and `Web/assets/art-manifest.json`. No license for the original game code or art is granted by the bundled third-party license.

## Browser performance update (0.2.1)

Use **Low + Adaptive Resolution** for responsiveness. Settings now displays gameplay FPS and render resolution. The renderer batches the city, uses animated low LODs and lightweight shading on Low, pools chain effects, and fixes slow motion caused by low frame rates. See [measurements and quality budgets](Docs/BrowserPerformance.md). The Windows download remains the UE5 v0.2.0 build.
