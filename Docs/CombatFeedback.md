# Combat feedback

All sixteen cards use the shared `ANeonSpellImpactDirector` for confirmed damage feedback. Original card identities and damage values remain in the deck data. `ConfirmDamage` receives measured health loss after the enemy damage call, including secondary blast damage, chain jumps, field pulses, fragments, and consumed talismans. World impacts use `PlayImpact` without reporting damage.

## Visual layers

- The shared `NS_PhysicalHitSparks` is duplicated from the original `NS_ExplosionSparks`. GPU distance-field collision runs before the force/velocity solver, retaining gravity, 0.45 restitution and 0.3 friction. Sparks bounce against the arena environment. Their color follows the attacking card.
- Normal reserved bursts emit 324 sparks; Prism Burst and Neon Cataclysm emit 576. The shared overflow budget uses 108 shorter-lived sparks. This is a 10% density reduction from the initial amplified pass; size, brightness, trails and collision behavior retain that pass's settings. Reserved bursts live 0.75–1.5 seconds and overflow bursts 0.4–0.85 seconds. World-space bounds include the floor below torso-height impacts.
- Each damaging hit also submits a 0.32-second expanding, spell-colored ring and star flash. Its small additive plane faces the camera and ignores depth so a dense crowd cannot conceal the hit cue. This is visual confirmation, not another damage volume.
- Surviving enemies receive a 0.13-second material flash. Custom primitive data stores color and strength without creating per-enemy dynamic materials. Pool activation/return clears it, and feedback tracks actor generations to protect reused enemies.
- Moving attacks shed 28-particle bursts with short lifetimes alongside their signature ribbons and bodies. Chain and rail streaks use an emissive beam material; fades preserve beam length. Projectile cores and hit cues scale modestly with the saved camera distance for readability.
- Existing pink cinematic explosions, compact fireball lights and gold card shards remain layered over the shared feedback. At most four additional, unshadowed point lights flash for 0.16 seconds; the existing cinematic director retains its shadow/light and fluid budgets.

## Budgets and gameplay

The director prewarms 96 impact components (four reserved for each card and 32 overflow), 64 flight components, and 256 instanced hit pulses. Bursts of the same style within 65 cm and 55 ms share their GPU simulation. Every admitted nearby hit still submits a pulse, including when the GPU burst budget is full; pulse slots are reused circularly under extreme overlap. Admission never gates damage. Effects beyond 4500 cm from the player are culled.

The original spell targeting could repeatedly choose an enemy behind a wall and starve visible targets. Automatic targeting now picks the nearest enemy with a clear world-static line of sight. Original projectile blast secondary damage also checks walls before applying damage and feedback.

## Rebuild assets

After building `DeckBuilderEditor`, run the editor commandlets in this order, with the editor closed:

1. Python script `Scripts/author_combat_feedback.py` to author the three materials and hook all ten cyborg PBR materials.
2. `-run=SpellAuthoring -FeedbackOnly` to create the parameterized GPU collision system.
3. `-run=SpellAuthoring` to rebuild the strengthened original trails.

`Scripts/import_cyber_roster.py` installs the same hit-flash material hook after reimport, preserving the pipeline. No additional generated images, paid models or animations are required.

## Verification

Run `Scripts/Test-CombatFeedback.ps1 -IncludeRegressions -IncludeBenchmark`. Automation uses separate save slots. The dedicated test runs real weapon cooldowns and collision against stationary, high-health enemies, checking positive measured damage and GPU burst admission for all sixteen cards. It also verifies actual projectile targeting past an occluded nearest enemy. Per-card screenshots are saved as `Saved/Screenshots/Feedback-<CardID>.png`.

Regression cases cover the arsenal patterns/mutations, audio and settings, card drafting and city navigation. The optional benchmark uses 180 enemies across all ten cyborg archetypes, all fifteen offensive cards, and overlapping full cinematic explosions at 1600×900 with DX12/SM6, Lumen and virtual shadow maps. CSV frame measurements and per-style feedback counters are saved under `Saved`.

### Verified results — October 18, 2026

- Development Editor build succeeded. The sixteen-card standalone test passed, including actual damage, per-card GPU burst admission, visible-target fallback and deliberate GPU-pool saturation. A saturated burst still dealt its seven test damage and submitted the fallback pulse. One intentional admission drop is expected in that test.
- Arsenal, settings/audio and city-navigation regression cases passed. The final shader revision also passed the sixteen-card test and benchmark without material or Niagara errors.
- The full-density 180-enemy pass measured **105.2 FPS mean**, **9.50 ms mean frame time**, **10.81 ms p95**, and **4.85 ms mean GPU time** over 1,052 sampled frames. This is a local Development build measurement with the effect budgets enabled, not an uncapped-effects guarantee.
- All fifteen offensive cards reported positive damage during the crowd run. Through the end of warmup plus measurement, feedback submitted 1,794 pulses and 763 GPU bursts; 507 nearby hits shared existing bursts and 524 additional dense bursts were budget-limited. Those hits retained pulse/material feedback. The cinematic director separately admitted 36 explosion requests and eight fluid grids over time.
- Benchmark enemies use 1,000,000 HP, preserving small damage increments while keeping the crowd alive. The old 1,000,000,000 HP value lost small hits to floating-point rounding and understated the feedback workload.

Detailed measurements: `Saved/Benchmarks/CombatFeedback180-summary.json`. Screenshots: `Feedback-ArcRelay.png`, the other fifteen per-card captures, and `CombatFeedback180-1.png` through `-3.png` in `Saved/Screenshots`.

The subsequent 10% impact-density trim passed a fresh Development Editor build and all sixteen cards' standalone damage/feedback checks, including wall targeting and saturated-pool fallback. It retains the full-density pass's trails, brightness, pulse cues and collision physics. The frame-time results above belong to the earlier full-density configuration.
