# Forbidden Code: Survivor — two editions

The Windows edition keeps Unreal Engine 5, Nanite, Lumen and Niagara. The independent browser edition uses WebGL 2 / Three.js, glTF characters, physically shaded materials, bloom and bounded particle pools. It executes locally in the browser and needs no GPU streaming server. Its static files can be hosted on GitHub Pages.

## Repository layout and branches

Keep both editions in one repository, with `main` holding the native source/configuration and `Web/` for the browser. This separates the implementations without forking the tuning. When releases need separate stabilization, create `release/windows` and `release/web` from `main`; do normal shared development on `main` and merge fixes back. The public repository is https://github.com/ATritle/Forbidden-Code-Survivor and the browser deployment is https://ATritle.github.io/Forbidden-Code-Survivor/. Large native asset folders remain local for this publication; see `GitHubPublishing.md`.

`GameData/gameplay.json` and its browser copy contain the current sixteen card definitions, ten enemy profiles, seven contracts and essence/wave tuning. Run `python Scripts/export_browser_rules.py` after changing the native authored definitions. The exporter validates library counts and records source hashes. UE C++ remains the balance source; browser mechanics interpret the exported snapshot. Changes to a spell's C++ behavior still need an equivalent JavaScript change; sharing numbers does not automatically translate implementations.

Internal `DeckBuilder` module, class and asset paths and existing save-slot names remain stable so existing Unreal assets and player saves still load. Display titles, UI, window title, project metadata, browser document title and installer use **Forbidden Code: Survivor**.

## Browser play

Double-click `Play-Browser.cmd`. It uses Node.js to serve only `Web/` on loopback and opens the game. Alternatively, run `node Web/serve.mjs`; visit `http://127.0.0.1:4173`. No package installation or build step is required: Three.js 0.186.1 and its MIT license are vendored locally. `Scripts/setup_browser_vendor.py` downloads the official npm archive and verifies its published integrity hash.

Use `Stop-Browser.cmd` to stop the local server. `Artifacts/Browser/ForbiddenCode-Survivor-Web.zip` contains the static hosting files; serve its extracted contents over HTTP rather than opening `index.html` directly.

The browser game includes pre-run school filters and a saved two-card loadout; automatic attacks; all sixteen spell identities; nova, explosive and piercing mutations; unique three-card drafts; the slower 18/28/42/60/82 essence curve; enemy pursuit and collision; ramps and weighted player falls; timed cyborg tiers and a Sovereign at six minutes; three randomly sampled distinct challenge signs from all seven contracts; separate audio controls, impact intensity, zoom and quality preferences; low-health audio and screen edges; and a static testing range with multiple spells, stats tooltips, real damage, health labels and chain-spacing confirmation.

The browser edition is an initial playable port, not complete feature or rendering parity. It caps active enemies at 90, approximates native Niagara with 3,000 bounded bouncing sparks, and uses simplified flow-field navigation and adapted city geometry. Multi-level overpass navigation is simpler than Unreal's navmesh. Testing currently selects one enemy archetype per preset and starts from the menu; it does not preserve an active run for practice. Native testing supports mixed-archetype grids and suspending/restoring a run. Browser saves use versioned localStorage, independently of Windows `.sav` files. Touch movement is included; broad device coverage and long-run balancing still need playtesting.

Existing Meshy assets are converted by `Scripts/prepare_browser_assets.py`: the actual rigged hoverboard rider, ten animated cyborgs and three city models. The model and art manifests preserve source paths, units, animation names and file sizes. The browser player includes Attack135, Cast133 and Attack250. Music uses the user-provided Monume menu track and Humanstudioed gameplay track; gameplay music continues during drafts/settings.

## GitHub and Pages

1. Create the repository using `GameData/repository-description.txt` as its description.
2. Install Git LFS before adding Unreal assets. The tracked `.gitattributes` scopes LFS to native/source binaries; `Web/` stays ordinary Git files so Pages can serve the models directly. Build outputs, caches, local tools and save games are ignored.
3. Push the repository with `main` as its default branch.
4. In repository **Settings → Pages → Build and deployment**, select **GitHub Actions**.
5. Run **Deploy Forbidden Code browser edition** from Actions, or push a `Web/` change. `.github/workflows/deploy-browser.yml` tests the browser rules and deploys only `Web/`.

All asset/module paths are relative, so project URLs such as `https://OWNER.github.io/REPOSITORY/` work. Upload the Windows installer as a GitHub Release asset and link it from the browser game's landing page after the repository/release URL exists. Do not add generated installers or Unreal intermediates to ordinary Git history.

GitHub Pages serves static files; it cannot execute the Windows game. A browser-native renderer is therefore the chosen delivery route. References: [GitHub Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages), [Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits), [Git LFS](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage). Pixel Streaming was researched as an alternative but is not used: [Epic's overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/pixel-streaming-in-unreal-engine).

## Windows package and installer

Run `Scripts/Package-Windows.ps1` in PowerShell with the existing UE 5.8 installation. It compiles the Shipping game, cooks Neon District and required art/UI/audio, stages prerequisites, and archives into `Artifacts/Windows`. The root bootstrap is renamed `ForbiddenCodeSurvivor.exe`; keep its adjacent game and Engine folders. Windows explicitly targets DX12 / SM6 so Nanite, virtual shadows and Lumen remain available.

For the installer, fetch the portable official NSIS compiler using `python Scripts/setup_windows_installer.py`, then run `Scripts/Build-WindowsInstaller.ps1`. `Packaging/ForbiddenCodeSurvivor.nsi` builds `Artifacts/Installer/ForbiddenCode-Survivor-Setup.exe`, with a per-user install directory, Start Menu shortcut, optional desktop shortcut, Windows uninstall registration, and uninstall instructions limited to the packaged file list. Saved games are retained. The compiler is kept under ignored `Tools/`, with its source URL and download hash. Future builds can use `Scripts/Package-Windows.ps1 -Installer` after compiler preparation.

## Checks

`node --test Web/tests/rules.test.mjs` checks library parity, duplicate-free drafts, progression costs, timed tier gating, structural mutations and preference persistence/clamping. JavaScript also passes Node syntax checks. The opt-in browser `?qa=1` controls exercise real gameplay functions and disable focus-loss pausing for automation; they are absent from regular play. Real UI checks cover card drafting, mutation application, independent saved audio/zoom/intensity preferences, multi-spell static targets and chain-spacing confirmation. The range's combat report lists actual hits and damage per equipped spell.

UE checks are recorded in `Saved/ForbiddenStreetsValidationConsole.log` and `Saved/ForbiddenStreetsFinalPreview.log`; the latter rejects material fallback and validates the repaired DX12 street shader. The native tests preserve all six navigation destinations, ramp ascent, walls, landing, XP drafts and pursuit. Screenshots are under `Saved/Screenshots/StreetLighting-*-After.png`, `NeonCodex-MainMenu.png` and `ForbiddenCode-Web-*.png`.

All eight browser rule tests passed. Real rendered combat confirmed damage from all sixteen base spells and all sixteen mutated spells in `Saved/BrowserCombatValidation.json` and `Saved/BrowserMutationValidation.json`. These checks exercise the actual browser combat code, in addition to the pure rule tests.

`Scripts/Test-PackagedWindows.ps1` ran the actual renamed Shipping bootstrap with DX12/SM6. `Saved/ReleaseSmoke/report.json` records `PACKAGED_RELEASE_PASS`, loaded art/deck readiness, 382 sidewalk pieces, 338 curbs, 27 lamps and live enemies. Menu and combat captures are beside the report. The NSIS installer was compiled successfully (486,135,602 bytes); its SHA256 and build metadata are in `Artifacts/Installer/release-manifest.json`. Installation into the user's Windows profile has not been exercised by this automated package check.

The isolated local 180-enemy benchmark at 1600×900 averaged 9.26 ms/frame (about 108 FPS), with a 10.65 ms 95th percentile and 4.92 ms mean GPU time over ten sampled seconds after five seconds of warmup. It used fifteen offensive spells, rain/Lumen and eight full explosion requests per second with VFX admission caps enabled. Excess effects were intentionally coalesced/dropped. This short measurement is specific to this machine and scenario, not a performance guarantee. Raw data: `Saved/Benchmarks/ForbiddenStreets180Isolated.csv`.
