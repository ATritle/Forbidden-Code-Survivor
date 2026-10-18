# Neon Arsenal — ten additional cards

The pre-run collection now has sixteen unlocked cards across Pulse, Prism, Kinetic, and Barrier. New cards use the same validated deck flow, save format, cooldown timers, owned-spell mutations, and three-choice drafts as the original six.

| Card | School | Damage | Cooldown | Attack and animation |
|---|---|---:|---:|---|
| Arc Relay | Pulse | 22; ×0.82 per hop | 1.30s | Blue irregular energy links across 4 unique enemies; 320cm maximum jump. Colliding GPU sparks at every hit. Cast #133 pose. |
| Ghost Link | Barrier | 14; ×0.90 per hop | 1.90s | Six violet spectral links; 240cm jumps, slower lingering strands and ghost-shaped spark rings. #250 attack pose. |
| Neon Cataclysm | Prism | 78 per enemy | 3.80s | Hot-pink charge flies to the target and detonates after 0.45s in a 300cm radius; expanding shock disk, 96 colliding sparks, and 0.2s pink flash. #135 attack pose. |
| Orbit Razor | Kinetic | 28 per blade | 1.50s | Two lime crescent blades spin outward, return after 0.65s, and hit up to 4 unique enemies each. Rotating crescent material and fading trails. Main cast pose. |
| Ricochet Seal | Kinetic | 24 per hit | 0.95s | Jade hexagonal seal rebounds to up to 3 additional foes within 350cm; rotating circuitry and green shattered-glyph impacts. Main cast pose. |
| Strobe Rail | Pulse | 46 per enemy | 2.20s | Ice-white rail reaches 1200cm, 32cm total width, piercing all targets along the line until a wall. Thin flashing core and ring sparks. #133 attack pose. |
| Echo Petals | Prism | 16 per petal | 1.40s | Six coral energy petals spiral outward with curved trails; one target each. Petal-shaped GPU fragments. #135 attack pose. |
| Event Horizon | Barrier | 16 × 4 pulses | 3.20s | Violet 230cm field pulses at 0, 0.35, 0.70, 1.05 seconds; animated concentric glyph rings remain for 1.4s. #250 attack pose. |
| Shatter Sigil | Kinetic | 34 + five 12-damage fragments | 1.80s | Amber triangular sigil splits on first contact into a five-fragment fan; children cannot recursively split or re-hit that enemy life. Main cast pose. |
| Satellite Choir | Pulse | 18 × 3 orbs | 1.60s | Aquamarine satellites orbit for 0.28s, then steer toward nearby visible enemies; pulsing cores and comet streaks. #133 attack pose. |

Ranges and radii are centimetres. Chain falloff includes the original target at full damage. Each projectile snapshots its mutations when fired. Cast poses reuse the approved player animation set; the ten projectile, field, and impact animations are procedural and distinct.

## Balance and UI

Draft costs are now **18, 28, 42, 60, 82...** essence, using `18 + 8*(Level-1) + 2*(Level-1)^2`. Excess essence carries over and multiple earned levels queue safely. Enemy essence rewards increase by tier, separately from gem diameter or pickup behavior.

The main menu contains the full illustration inside a square 184px panel, with the title and description beside it. Rendering preserves texture aspect ratio and original colors. Titles and descriptions wrap. The combat weapon HUD wraps sixteen entries into two rows.

## Architecture and bounds

- `NeonArsenal::Profiles()` is the shared immutable tuning table. `ANeonArsenalSpell` resolves a card ID and uses the normal base cooldown timer.
- `ANeonArsenalDirector` prewarms **32 gameplay slots plus 32 visual streak slots per new spell**, with instanced body and streak renderers. Streak saturation cannot consume projectile slots. No shot actors are spawned.
- Four preassigned Niagara GPU burst components per spell: **40 total**. Lifetime is at most 0.65s, recycling delay 0.7s, fixed bounds 650cm, distance cull 2600cm. Overflow drops cosmetic bursts; chain damage still resolves. Existing distance-field collision is inherited from the explosion spark template.
- Two shared unshadowed flash lights cap overlapping Cataclysm flashes. The larger original cinematic explosion system remains available on Prism Burst.
- World-static traces block acquisition, chain links, area line-of-sight and projectile travel. Chains deduplicate enemy generations; projectiles snapshot their own hit-generation maps so recycled pooled enemies are independent lives.
- Count mutations add hops, projectiles, rails or field copies according to the spell. Piercing adds target capacity. Radius upgrades multiply size or link reach by **1.25**, including through the normal deck selection path. Explosive mutations deal 30% area damage for chains/rails, 40% for moving projectiles.

## Art and reproducibility

Ten square card illustrations were generated individually with the **built-in image-generation tool**. Final files and complete prompt set: [generation-prompts.json](C:/dev/DeckBuilder/ContentSource/UI/NeonArsenal/generation-prompts.json). Source images are adjacent to that manifest. Enemy concepts, prompts, source audit and approved Meshy job ledger are under `ContentSource/Enemies/CyberRoster`; see [the complete illustrated enemy roster](C:/dev/DeckBuilder/Docs/CyberEnemyRoster.md).

Run `Scripts/author_neon_arsenal.py` in Unreal to create materials and import card icons, then the `SpellAuthoring -ArsenalOnly` commandlet to rebuild GPU sparks. `Scripts/import_cyber_roster.py` imports Meshy rigs, saves skeletons explicitly, validates matching clips, assigns PBR and neon emission materials, and generates three LODs per enemy.

`-MVPSmoke -ArsenalSmoke` validates actual collision, damage, chain counts, mutations, pool exhaustion, draft thresholds, unique choices, all ten imported rigs, sampled bone motion, timed unlocks and single-boss admission. Automation uses separate save slots.

## Verified build

The editor build, arsenal smoke test, settings persistence regression, and city terrain/navigation regression passed. The city test includes the largest boss navigating around the shopping block, physical ramp movement, landing, gem pickup and paused three-card drafting. Fresh-process animation validation reports no missing-skeleton errors.

Final 1600×900 uncapped benchmark: **180 active cyborgs throughout**, all fifteen offensive spells, rain, Lumen, and eight cinematic explosion requests per second. The verified mix contains all ten archetypes: basic 34/31/30/31, medium 18/18, elite 5/6/6, and one boss. After five seconds of warmup, 1,101 frames over ten seconds averaged **9.09ms / 110 FPS**, with **10.80ms p95** and **4.79ms median GPU time** on this PC. These are prototype measurements, not a guarantee for other hardware or resolutions.

Fixed effect budgets stayed enabled. At sample end the cinematic director recorded 35 accepted requests, 114 dropped requests and eight fluid admissions; cosmetic saturation does not suppress gameplay damage. [Raw CSV](C:/dev/DeckBuilder/Saved/Benchmarks/NeonArsenal180.csv) and [full timing summary](C:/dev/DeckBuilder/Saved/Benchmarks/NeonArsenal180-summary.json).

Review [the corrected original card layout](C:/dev/DeckBuilder/Saved/Screenshots/NeonCodex-SquareArt.png), [new cards in the actual menu](C:/dev/DeckBuilder/Saved/Screenshots/NeonCodex-NewCards.png), and [the imported cyborg roster in-game](C:/dev/DeckBuilder/Saved/Screenshots/CyberRoster-InGame.png).
