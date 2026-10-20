# Browser 0.5.0 — Windows gameplay port

This release ports Windows 0.4.7 gameplay and menus to the browser edition. It uses a pinned snapshot of native commit `9ae0d6d28187ab09d0c29f2110286bae41f7eb4a`. The native branch and packaged Windows build remain unchanged.

## Included

- Five riders with approved portraits, Meshy bodies/boards, breathing poses, casts, locomotion and landing/death clips; Vex, Rook, Zero and Mira have their native perks and tradeoffs.
- Three two-card presets per rider, school filtering, starter unlocks, separate rider fragment wallets, rarity costs, all sixteen programs in every run's draft pool and sixteen active slots.
- Three finite ranks and two exclusive evolutions per program; two rerolls, one banish, deferred drafts and full card/evolution hover text. Rank previews show before/after damage, cooldown, count, pierces and radius. Echo Petals upgrades cannot reduce its projectile count.
- Boost, marked/nearest/strongest targeting, terrain gravity and landing feedback/reward, actual-damage attribution, companions, possession and captured routines.
- Ten cyborg archetypes, armor/shields, flankers, support channels, charge/zone windups, reserved boss admission, shield nodes and three Sovereign phases with a sweeping lane telegraph.
- Two rooftop relay hacks, early/guaranteed boss arrival, caches, clinic purchases, shrine defense, subway warnings, salvage and all seven randomized No Pain contracts. Contracts grant corruption choices with tradeoffs. Duel signs select flat ground; echoes do not repeat rider damage scaling or execute effects.
- Research, cosmetic board/district unlocks, local run history and immutable end-run rider/card snapshots for the shared leaderboard. Existing browser scores remain; older entries show that loadout details were not recorded. Personal best-three/global top-ten rules remain intact.
- Compact workbench, generated Signal Dock artwork and controls, track art, music-reactive spectrum, collapsible TRACKS list, looping selected menu track and gameplay rotation. Gameplay music continues through drafts. Click sounds use the approved files; hover is silent. Reserved mechanical cues are enemy-only.
- Independent saved music/SFX, impact intensity, zoom, quality, adaptive resolution, bloom, flash, shake, numbers, targeting and bindings. ESC backs out even with inputs focused; standard-mapped gamepad controls share the same actions.
- Illustrated card guide and a flat testing range with multiple attacks, mixed enemy tiers, groups of five, stage selection, spacing warnings and real HP/damage. Entering from a run suspends it; resume restores its clock, actors, projectiles, objectives, contracts and loadout. Practice is unranked and awards no fragments.

## Elevation navigation

`Web/src/navigation.mjs` owns collision and walkable surfaces. A street and an overpass at the same X/Z are distinct nodes. Shared reverse-Dijkstra fields use three clearance classes; movement, projectiles and sight use the same geometry. Enemies cannot step off cliffs as a shortcut. A broken overpass connection and its obstructing tower were corrected. The shrine and overpass also have alternate approaches.

Continuous movement tests cover ascent/descent and all six raised areas, including basic, elite and boss clearances. A rendered chase put the Sovereign directly beneath a stationary rider and confirmed arrival on the 6.2 m overpass.

## Browser adaptations

WebGL uses the approved assets with simplified city geometry, low LODs, bounded particle/line/field/projectile pools, shared materials, limited lights and staggered animation/steering. It does not execute UE5, Nanite, Lumen, Niagara or Animation Blueprints. Windows retains that rendering pipeline.

Browser display options are browser-window/fullscreen. OS exclusive/borderless modes and guaranteed application exit are native features; when a browser blocks `window.close`, the UI explains how to close the tab. Native and browser progression saves are separate. Physical gamepad hardware and lower-end mobile hardware were not available for validation.

## Validation and rollback

See `Docs/Browser-0.5.0-Validation/` for test logs, performance reports and rendered checks. Stress profiles use all sixteen rank-three programs with both sets of evolutions. Results are machine-specific and limited by the test display's refresh ceiling.

The original browser build is preserved locally at `Artifacts/Baselines/Browser-PreParity-20261020/Web-0.2.4.zip` (SHA256 `8739215c67a34f56920f2715a09f19f6b799d3ba6664ca725ca0166733d9b5da`) and in Git at `1c6e37f`. Restore into a separate directory to inspect it, or restore the browser scope from that commit and republish. Revert both the parity release and its orb-collision follow-up if using Git revert. Never overwrite the native baseline or merge the complete native branch into main for a browser update.

The Windows download points to the existing, verified [Windows 0.4.7 installer](https://github.com/ATritle/Forbidden-Code-Survivor/releases/tag/v0.4.7-windows).

## Updating the data and assets

CI runs `Scripts/export_browser_rules.py`, which deterministically reads `GameData/browser-native-source.json`; it does not compile old native sources on main. To capture a later native release, run the exporter in the full Windows workspace with `--from-native --native-commit <full SHA> --native-version <version>`, review the snapshot and port browser behavior separately.

Approved-art conversion uses `prepare_browser_parity_assets.py`, the Blender browser exporters, then `build_browser_lods.py`. These read the full Windows asset workspace and write browser outputs; no new Meshy generation or credits are required.

The live-validation follow-up fixes an inherited Overload Orb collision bug: enemy locations are at their feet, so collision now tests their vertical body. Basic enemies can rupture the orb; enemies far below a high rooftop cannot.

The same follow-up aligns spacing warnings with the tested rank/evolution, retains advanced enemies' native contact attacks outside their windups, and updates remaining ward charges in the HUD.
