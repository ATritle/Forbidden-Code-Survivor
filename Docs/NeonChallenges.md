# No Pain - No Gain

Start a regular run and explore the district. Three physical neon street shrines
spawn at new random, reachable positions each run, away from the starting point.
Each run samples three distinct offers from all seven contracts. Approach within 310 cm and press
**E** to review its reward, risk and duration. **Accept Challenge** activates it;
**Decline** or **Esc** resumes combat without consuming the station.

Only one contract can run at a time. Accepting consumes that station for this run.
An active-contract HUD tracks duration and danger. Normal settings and card-draft
pauses suspend the trial. Practice mode hides and suspends its state, then restores
it when returning to the original run. Contract progress is intentionally run-local.

| Contract | Reward | Risk |
| --- | --- | --- |
| Overload Orb | Triple outgoing spell damage for 45 seconds | After a 1-second arming grace, an enemy touching the orbiting core causes one 50%-maximum-HP rupture and ends the buff. This can kill. |
| Unstable Echo | Every third successful spell hit arms an additional blast for twice that hit's damage, 180 cm radius, for 40 seconds | The ring detonates after 1 second and costs the player 20% maximum HP if inside. At most one new echo per 0.5 seconds. Echoes cannot create echoes. Already armed rings finish their detonation after the contract expires. |
| Hostile Broadcast | Double essence dropped by enemies defeated during a 60-second trial | Two elite hunters arrive at 15, 30 and 45 seconds. A countdown warns of reinforcements. Hunters remain after the bonus expires. |
| Deadman Battery | Double outgoing spell damage and 4%-maximum-HP healing per real kill for 30 seconds | Continuously drains 2% maximum HP per second. The drain bypasses contact immunity and can kill. It does not repeatedly play hit animations. |
| Glass Protocol | 75% more outgoing damage and two extra penetrations for supported attacks for 45 seconds | All incoming damage is doubled. Fire-time penetration snapshots preserve authored card/mutation values. |
| Jackpot Courier | Deliver the marked data cache back to the accepting sign within 35 seconds for one bonus three-card draft | Carrying boosts enemy chase speed by 25%. Missing delivery causes 35%-maximum-HP backlash. Routes are checked against navigation before acceptance. |
| Lockdown Duel | Defeat a marked pooled elite inside a 900 cm radius ring within 40 seconds for a bonus draft and 24 essence | More than two seconds outside, changing elevation, losing the marked actor life, or timing out causes 35%-maximum-HP backlash. Finishing the elite outside the ring fails. |

Contract penalties bypass ordinary contact invulnerability. Protective talisman
cards continue to work; they do not remove an already armed Echo hazard. Hazard
rings and the orb remain visible at zero impact-particle intensity. Broadcast
modifies newly earned drops, not gems already lying on the floor.

The sign now uses a textured Meshy T2 housing generated from an unlit, blank-panel
edit of the approved concept. The approved job consumed 15 credits, produced an FBX
with 12,085 triangles and four 2K PBR maps, and uses Nanite in Unreal. Exact live
lettering, cyan trim, pink energy core, emissive animation and a local spill light
are added after model generation; the housing material has no emissive output.
An independent simple collider preserves gameplay collision. The native housing
is a fallback if the imported asset is unavailable. Source and import reports live
under `ContentSource/City/Meshy/ChallengeSign`; original concept:
`ContentSource/Concepts/NoPainNoGain/NoPainNoGain-Sign-v1.png`.

## Architecture and extension

`UNeonChallengeDefinition` is a Primary Data Asset containing display copy and
per-contract tuning. `UNeonChallengeComponent` belongs to the player and owns the
run state, random station placement, contract modifiers and fixed hazard pool.
`ANeonChallengeStation` owns the world prop and its status. `UNeonChallengeWidget`
owns the decision panel; it calls the component through a weak reference.

Seven definitions are created per player as transient native defaults; an authored
definition array can replace them. Seeded sampling draws three unique contract IDs
without replacement and retains their identities during placement retries and
declines. New handlers and definition assets can be added without changing saved
deck/card data or the interaction UI.

Damage multiplication happens once in the shared enemy damage path so original
projectiles, chains, beams and area attacks all use the same rule. Echo damage is
guarded against recursive triggers. Essence is multiplied at enemy defeat and
stored in the resulting pooled gem. Maximum-health penalties use a dedicated
player damage path with the normal hit/death visual and game-over hooks.

`NeonChallengeTrials.cpp` contains the four additional trial handlers. Enemy death
callbacks use the actor and pool generation to prevent duplicate healing or rewards.
Duel completion additionally validates the marked life, player/target positions,
deadline and elevation. Objective failures latch before applying backlash. Bonus
drafts use the existing eligible-card selection pipeline; they are upgrades for
the current run and do not unlock cards permanently or change the saved deck.
Courier pickup/delivery is automatic at close range with visibility/elevation
checks. Its pursuit multiplier is queried during movement, so it never modifies
the authored enemy speed. All markers suspend during settings, drafts and practice.

Placement checks a complete navigation route to the sign's approach, flat support
under the footprint, body clearance, player separation and inter-station spacing.
Missing navigation delays placement and retries. The narrow collider does not
trigger costly navigation rebuilding during combat.

Prewarmed effects: one orb, twelve Echo markers and two objective/shell effects. Hunter and duel spawns use the existing
240-unit pool / normal active cap. At capacity they replace a normal offscreen
unit with an elite. Unavailable placements or slots retry with bounded work;
the contract never raises the cap or allocates extra enemy actors.

## Validation

Build `DeckBuilderEditor Win64 Development`, then run
`Scripts/Test-Challenges.ps1 -IncludeRegressions`.

The standalone harness checks two independent random placement samples, approach
interaction, decline/reopen, accepted-offer idempotence, pause ownership, actual
shared damage multiplication, one-shot orb rupture through contact immunity,
delayed Echo enemy/player damage and recursion protection, live enemy-cap
replacement, testing-range suspension/restoration, and modifier expiry. Existing
combat, testing, settings and city smoke checks run separately. The seven-contract
harness also checks offer coverage and uniqueness, actual imported Meshy bounds,
Battery drain/kill healing and dummy exclusion, Glass damage and launched projectile
penetrations, Courier delivery/failure/practice preservation, Duel victory/escape/
deadline/recycled-target rules, one-time payouts and lethal drain cleanup.

Reproduce model normalization and its unlit inspection render with Blender's
`Scripts/prepare_challenge_sign.py`. Run `Scripts/prepare_challenge_assets.py` in
Unreal's Python commandlet to reproduce the PBR import and separate neon/hazard
materials. `HousingPrompt-v1.txt` records the built-in imagegen edit prompt. No
changes are made to the saved player loadout by contract code.

The completed seven-contract pass built successfully in `Saved/SevenChallengesBuild.log`.
`Saved/SevenChallengeValidationConsole.log` records seven passing standalone cases
plus independent random placement samples. The in-game Meshy/neon preview is saved
as `ContentSource/City/Meshy/ChallengeSign/InGame-Neon-Sign.png`.
