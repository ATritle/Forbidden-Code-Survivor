# Forbidden Code: Survivor — browser edition

Run `node serve.mjs` from this folder, or double-click `../Play-Browser.cmd`. Open `http://127.0.0.1:4173`.

This is a self-contained WebGL 2 game. Deploy this directory with GitHub Pages; no UE runtime, bundler, database or paid streaming host is needed. `../.github/workflows/deploy-browser.yml` supplies deployment after the repository exists and Pages uses GitHub Actions.

Controls: WASD/arrows, wheel zoom, E contracts, Esc settings. A touch stick appears on touch devices. Choose one or two pre-run cards; collect essence for three-card upgrade selections. Settings saves music, SFX, particle intensity, zoom and quality. The menu's testing range supports multiple attacks, static tier targets and chain-range warnings.

Run `node --test tests/rules.test.mjs`. See `../Docs/TwoEditions.md` for shared tuning, asset rebuilding, release steps and current parity limits. Three.js licensing is in `vendor/three/LICENSE`; asset provenance is in the model/art manifests.

Performance controls: use Low with Adaptive Resolution enabled. Settings displays gameplay FPS and render resolution. The browser 0.2.1 pass batches scenery, uses animated low LODs, pools chain effects, and fixes frame-dependent slow motion. See `../Docs/BrowserPerformance.md` for measured results and preset tradeoffs.

Browser 0.2.2 adds the shared leaderboard and optional end-of-run username submission. See `../Docs/Leaderboard.md`.
