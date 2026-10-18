# Impact intensity and testing area

Open **Settings / ESC** in the Codex, a run, or a level-up draft.

**Impact Particles** adjusts spark density from **0% to 150%**. At 100% it uses the slightly reduced impact pass: 324 normal sparks, 576 for large impacts and 108 for overflow. It scales shared collision sparks, gold card shards, all ten arsenal signature bursts and cinematic explosion sparks. Projectile trails, damage, hit flashes and hit markers retain their normal settings. At 0% the impact sparks are disabled and hit cues remain visible.

Changes apply to subsequent impacts and save automatically after 0.35 seconds, including while Settings pauses gameplay. Save & Return also flushes the write. The independent `CodexVFX.sav` slot contains versioned, validated intensity data; it does not alter audio, camera or deck saves. Missing or invalid data defaults to 100%. Automation uses `CodexVFX_Automation` and benchmarks use a fixed default.

## Range controls

1. Click **TESTING AREA / CARDS & TARGETS** in Settings.
2. Tick one or more unlocked attacks. Every checked attack uses its base stats and original cooldown and fires together with the other selections. **Select All** and **Clear All** support quick comparisons; an empty attack selection is rejected. Hover any attack name here or in the range's attack bar for its description, damage, cooldown, range, and structural stats.
3. Toggle any of the ten enemy types. Each enabled type can spawn **5–50** targets in steps of five; the total budget is **200**. Defaults are five of every type, covering all four tiers. Invalid or empty configurations are rejected before changing the session.
4. Set grid spacing to **200–500 cm**. The default is 220 cm, allowing chain and area attacks to reach neighbors. Increase spacing to isolate impacts and examine projectile travel.
5. Click **APPLY SETUP & ENTER RANGE**. If a selected chain/rebound attack cannot reach the next grid cell, a warning lists the affected attacks and their jump distances. Choose **BACK & ADJUST TARGET SPACING** or explicitly **CONTINUE WITH THIS SPACING**. No setup changes apply before confirmation. WASD moves, mouse wheel zooms and selected attacks fire automatically. Approach enemies to test Talisman Guard contact hits.

The warning uses live profile tuning: Arc Relay 320 cm, Ghost Link 240 cm and Ricochet Seal 350 cm. Spacing equal to the reach also warns because targeting uses a strict distance comparison. Cancelling and editing options recomputes the warning; a previous confirmation does not authorize changed options. Pulse/Prism/Kinetic attacks that do not chain do not produce a chain warning.

Targets cannot chase or attack. The tier profiles retain their real health, dimensions and hit collision. Each grid slot displays remaining/max HP; floating numbers show actual HP removed, capped by remaining health. The HUD totals damage, hits and defeats. Rapid hits on the same target can combine into a single readable number. There is no XP, card reward, run kill credit or damage to the practice player.

**Reboot defeated targets** restores their full tier health after 1.5 seconds. Disable it to leave defeated slots empty. Reapply a setup to reset all targets, stats and selected attacks, including spent talisman charges. ESC opens Settings so intensity can be tuned between volleys or the attacks/targets changed. Selected checkboxes remain checked when the configuration is reopened.

Use **EXIT TESTING & RETURN TO SESSION** in Settings or the range configuration to leave. The original pawn, health, deck mutations, target health/positions, active projectiles, run clock, pending wave delay and any open card draft are retained. Practice uses a separate pawn, 200-target pool, 256-projectile pool and arsenal director in an isolated neon grid above the city. A full paused gameplay projectile pool cannot starve practice attacks. The original enemies, projectiles, gems and progression ticks are suspended until exit. It does not save a new starting deck. User changes to impact/audio preferences still persist normally.

## Implementation and verification

- `AMortisTestingDirector` owns the bounded 200-target pool, target grid, actual-damage telemetry, reboot scheduling and original-session suspension/restoration.
- `UNeonTestingWidget` supplies attack checkboxes with hover panels, enemy toggles, snapped per-type counts, spacing, reboot, modal chain-spacing confirmation and enter/exit controls.
- `TestingCardInfo` formats shared descriptions/stats and derives chain reach from the same profiles used by the weapon implementation. The HUD uses it for live attack-name hover panels too.
- `UNeonSettingsWidget` binds the intensity slider to `ANeonSpellImpactDirector`, which manages debounced saving through `UCodexVFXSettingsSave`.
- `AMVPHUD` renders HP labels, floating damage, practice totals and hoverable attack names only while the range is active. Mixed-loadout floating numbers use a neutral color to avoid attributing combined damage to one attack.
- Gold and arsenal Niagara signature bursts expose `User.SparkCount`; regenerate them using `SpellAuthoring -CardsOnly` and `SpellAuthoring -ArsenalOnly` after building.

Run `Scripts/Test-TestingArea.ps1 -IncludeRegressions`. It runs `-TestingMultiSmoke` for combined casting, hover stats and spacing warnings, followed by `-TestingAreaSmoke` for settings persistence, individual cards and original-session restoration. The checks exercise actual settings controls, paused saving and world-restart persistence, actual scaled Niagara parameter values, all ten static profiles, all sixteen individual cards, real damage numbers, pool limits/reboots, and returning to the main menu, an upgraded live run and an open draft. Screenshots are saved under `Saved/Screenshots`.

Verified October 18, 2026: Development Editor build and `TESTING_AREA_PASS`. At 50% intensity the submitted spark counts were 162 for shared impacts, 18 for gold shards and 60 for compact cinematic bursts. At 0%, the same hit cue remained visible without a spark burst. A 63% preference survived a fresh world reload. All sixteen cards produced positive measured target damage and damage-number records in the range. The ten profiles spawned as an interleaved, stationary grid with five of each type; boss-only configuration restored real 6,000 HP on reboot. Invalid counts and more than 200 targets were rejected. The original main menu, live run health/weapon mutations/enemy position and wave timing, and open draft choices/pause ownership restored correctly.

The settings/audio persistence, sixteen-card combat feedback, five-second health-alarm fade and city-navigation regression checks also passed on the final build. See `Saved/TestingAreaValidationConsole.log` and the corresponding `Saved/Testing*Regression.log` files.

The multi-attack update passed `TESTING_MULTI_PASS`: four selected weapons produced simultaneous damage; Select All equipped all sixteen with unchanged base tuning; each checkbox supplied a tooltip and the in-range Pulse Dart name opened its stats panel. At 300 cm only Ghost Link warned; at 400 cm Arc Relay, Ghost Link and Ricochet Seal warned. Confirmation/cancellation and editing after cancellation were verified without changing the active session prematurely. The isolated 256-shot practice pool kept firing with the entire original 256-shot gameplay pool occupied, then restored those paused shots. The range, settings, combat, alarm and navigation regressions passed after the update.
