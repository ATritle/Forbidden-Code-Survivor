# Necromancy and cinematic test framework — UE 5.8

## Delivered scope and production gate

The running game includes a Raise Skeletons card, bounded minion formation, three minion mutations, a paginated Codex, and a separate cinematic launch profile. The C++ explosion director preallocates Niagara components and movable point lights, enforces admission limits, and fades flashes in 0.2 seconds. `M_StoneTest` is a lit test material. This is a compiled test framework, not a finished blockbuster art pack or a certified performance target.

The existing Meshy assets are the earlier low-poly batch. Dedicated skeleton attack animations, high-poly hero assets and the production master-material graphs below still require authoring. The Niagara explosion is now implemented: `NS_CinematicExplosion` contains a real 3D gas simulation and volume renderer; `NS_ExplosionSparks` contains 2,000 GPU particles with distance-field collision. Both are generated reproducibly by the editor-only `ExplosionAuthoring` commandlet. See [explosion test and profiling](ExplosionProfiling.md) for the current measured configuration. No new Meshy credits were spent.

Run `Play-MVP.ps1` for normal gameplay. Run `Play-CinematicTest.ps1 -Enemies 300 -ScreenPercentage 75` for the separate DX12/SM6/Lumen/VSM experiment. It prewarms the requested enemy count, spawns that count outside view on entering the arena, and displays CPU/GPU timings. Press **F** during combat to trigger the complete fire/smoke/spark explosion; explosive spells also use it. A 1000-enemy option is an experiment, not an FPS promise. Shader compilation and distance-field construction can make the first launch slow.

## 1. Pooling and spawning

The complete implementation lives in `Plugins/CodexRuntime/Source/CodexRuntime/{Public,Private}/EnemySpawner.*` and `PooledEnemy.*`. The project's existing enemy base is named `APooledEnemy`, equivalent to the requested `AEnemyBase`; retaining it avoids a second incompatible enemy hierarchy.

```cpp
// Blueprint-callable API on the existing AEnemySpawner:
bool InitializePool();
APooledEnemy* SpawnEnemyFromPool(FVector Location);
bool ReturnEnemyToPool(APooledEnemy* Enemy);

// Example in a gameplay owner, after class/size/cap configuration:
Spawner->InitializePool();
APooledEnemy* Enemy = Spawner->SpawnEnemyFromPool(Location);
if (!Enemy) { /* Cap reached or pool exhausted: skip this spawn. */ }
// Death uses Enemy->ReturnToPool(), never Destroy().
```

Initialization reserves arrays and preallocates actors. Activation pops the free list, sets the transform, registers the actor active, restores ticks/collision/visibility, then calls the Blueprint reset hook. Return rejects foreign/double returns, snapshots enabled component ticks, invokes the reset hook, clears object-bound timers, disables components/collision, hides the actor and restores its free-list entry. External destruction reduces capacity rather than silently allocating replacements. BeginPlay must not initiate AI before activation.

Camera spawning intersects all four viewport corners with the gameplay plane, takes the furthest planar distance, and adds a margin plus randomized ring width. This supports perspective/orthographic top-down views; invalid ground intersections skip a wave. Tall models and shadows need a larger margin. The open arena has no navigation obstacles: production maps need bounded collision/navigation placement checks. Never run unbounded retry loops.

Production expansion: partition the active registry into XY cells, update cell membership only when crossing a boundary, and query adjacent cells for minions/projectiles. The current nearest query is linear; pooling removes actor churn, not AI, animation, draw-call or collision cost. At larger populations move distant agents to Mass/instanced animation representations. Preserve generation IDs when actors are reused; cached targets must match the current life.

Runtime actor components use `CreateDefaultSubobject`, `TObjectPtr`, and reflected ownership; the explosion director demonstrates `FObjectInitializer`. Subobject-data interfaces are editor authoring infrastructure, not a replacement for runtime actor/component ownership. Do not add an editor-only SubobjectData dependency to the shipping gameplay module.

## 2. Codex and minion architecture

`UCardDataAsset : UPrimaryDataAsset` contains CardID, localized Name/Description, EMagicSchool, damage, cooldown, spell class and art references. `MeshyVisual` is an `FSoftObjectPath` restricted in the editor to static/skeletal meshes; legacy `MeshyModel` remains compatible. Resolve art through Asset Manager/StreamableManager during pre-run loading, hold strong references for the run, and include the referenced content in cooking. Do not synchronously load on first impact.

`UCodexDeckComponent` deduplicates unlocked/equipped cards by CardID, samples without replacement, and keeps the pending hand stable. Three eligible cards yield three unique choices; a smaller eligible set produces a smaller hand. Selection validates the pending offer and consumes it exactly once. Owned spells ask their subclass for eligible mutation tags. Save only stable IDs; active weapons, actor pointers and mutations are run state. The MVP saves starting IDs in `CodexDeck.sav`.

```text
Codex screen -> validated starting IDs -> equip spell actors
Level-up -> sample eligible unique CardIDs -> freeze hand + mutation tags
Choice -> add spell OR apply offered mutation -> clear hand -> resume
Death -> stop spells + deactivate summons -> return to Codex
```

`ASkeletonSummonSpell` fires without requiring an enemy. Every 3 seconds it replenishes a two-ally formation from four actors prewarmed by the game mode. Summons are persistent and untargetable in this prototype. `ASkeletonMinion` searches at 5 Hz, follows a formation offset, attacks in melee, teleports back if separated by 1500 cm, and validates enemy activation generations. Movement/attack rendering does not alter enemy collision. The current visual reuses the approved Gravebound rig with a cyan ally marker and a brief procedural strike lean; replace that lean with an attack montage and synchronized notify when dedicated animation exists.

| Mutation | Behavior |
| --- | --- |
| Bone Archers | Replace melee with pooled bone projectiles; 460 cm attack reach |
| Grave Guard | Orbit the player and restrict target pursuit to a 280 cm defensive zone |
| Grave Burst | Melee and ranged hits splash for 60% damage within 120 cm |

Mutations combine and affect existing summons immediately. Ordinary projectile mutations are not offered to the summon card. Dead/returned targets are discarded, and death shuts down both actor and animation ticks. Open-arena direct steering is deliberate; add navigation/crowd avoidance before introducing solid obstacles. Enemy target selection currently remains player-focused; this is not yet a tank/aggro or minion-health system.

## 3. Meshy material, Nanite, lighting and ground pipeline

### Import and geometry tiers

Keep source high-poly FBX and 4K/8K textures outside cooked Content. Validate scale, tangents, UVs, skin weights, holes and material-slot counts. Nanite does not manufacture geometric detail missing from the source. Use a single hero for close-up quality review before replacing the entire crowd.

Enable Nanite on suitable opaque static props and inspect cluster/overdraw visualizations. UE 5.8 also documents Nanite skeletal meshes with animation LODs and animation-bank instancing; compare that path against the current conventional skeletal meshes rather than assuming a geometry toggle fixes animation cost. Static/runtime Nanite displacement are documented as experimental. [Epic: Nanite content](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-naniteenabled-content)

Keep conventional skeletal fallbacks with authored 100%, 50%, 20% geometry LODs and reduced bone influence counts. These percentages are proposed starting points. Use animation significance tiers: near full-rate, mid throttled/shared, far reduced pose updates, dormant zero ticks. Validate silhouettes and pose transitions in the actual top-down camera. GPU sparks never become skeletal actors.

### Master materials to author

Create `MF_MeshySurface` for common texture/UV logic, `M_MeshyOpaque` (Default Lit) for metal/stone, and a separate `M_MeshyFlesh` (Subsurface Profile). Use material instances per asset; avoid a universal shader with many runtime branches. Keep bone/armor in the opaque material. Subsurface Profile repurposes material data and does not support a normal Metallic input workflow; separate armor and skin instead of blending metallic flesh. [Epic: Subsurface Profile](https://dev.epicgames.com/documentation/unreal-engine/subsurface-profile-shading-model-in-unreal-engine)

| Parameter | Type / suggested initial value | Connection |
| --- | --- | --- |
| BaseColorTex / BaseTint | Texture2D sRGB / white | Texture RGB × tint -> Base Color |
| NormalTex / NormalStrength | Normal compressed, linear / 1 | Scale tangent XY, reconstruct/normalize Z -> Normal |
| ORMTex | Linear masks | R AO, G roughness, B metal; repack Meshy's separate maps offline |
| RoughnessScale / Bias | scalar 1 / 0 | clamp(G × scale + bias, 0.04, 1) -> Roughness |
| MetallicScale | scalar 1 | saturate(B × scale) -> Metallic on opaque master only |
| MicroNormal / DetailTiling | normal / 8 | BlendAngleCorrectedNormals with main normal |
| EmissiveMask / EmissiveTint / Strength | linear mask / jade / 0–3 | multiply -> Emissive; restrained surface accent |
| FleshProfile | Subsurface Profile asset | skin-only scattering profile with physically calibrated distances |
| SSSMask / SSSAmount | linear mask / 0.5 | saturate(mask × amount) -> Opacity scattering mask |

Use roughness variation to preserve material readability under glancing light; do not encode strong baked highlights in Base Color. Inspect normals under a rotating key light and flip the normal green channel only when the source tangent convention requires it. Use a gray-ball/color-chart reference and locked exposure to compare imports.

### Lumen and shadow profile

For the cinematic branch use deferred DX12/SM6, Generate Mesh Distance Fields, Lumen GI/reflections, and Virtual Shadow Maps. The supplied launch script overrides these settings for its process. Hardware ray tracing is a separate opt-in requiring compatible hardware and shader/settings preparation; it is not enabled by the script. Inspect Lumen Scene and Surface Cache views, and build distance fields before measuring. [Epic: Lumen](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-global-illumination-and-reflections-in-unreal-engine)

Use one shadowed moon/key directional light, a restrained fill, fixed exposure, and bounded local explosion lights. The supplied director permits two simultaneous shadow-casting flash lights; additional admitted flashes are unshadowed. Crowd shadows remain off in the baseline and should be introduced by significance tier. Do not rely on small bright emissive sparks to produce stable instantaneous GI. A deterministic direct flash is needed for the 0.2-second peak; indirect illumination can update more slowly.

### Virtual texture and displacement setup

Enable Virtual Texture Support and restart before authoring VT variants. Convert selected large surface textures to Streaming Virtual Textures, then use Virtual Color/Normal/Mask sampler types consistently in both master and instances. Use shared UVs to reduce VT stack cost. Start with 128-pixel tiles and a modest border, then validate grazing-angle filtering and residency on the target GPU. SVT streams source texture tiles; RVT caches rendered terrain/material data. [Epic: SVT](https://dev.epicgames.com/documentation/unreal-engine/streaming-virtual-texturing-in-unreal-engine?lang=en-US)

For terrain blending, create an RVT asset for Base Color/Normal/Roughness and a separate World Height RVT. Fit RVT volumes tightly to the arena, assign render-to-RVT on producers, and sample them in the ground material. RVT World Height stores height; it does not independently move vertices or update collision. Avoid making ground sample the same RVT that its output recursively depends on. [Epic: RVT](https://dev.epicgames.com/documentation/unreal-engine/runtime-virtual-texturing-in-unreal-engine)

For the experimental displacement branch, put `r.Nanite.AllowTessellation=1` in startup configuration and `r.Nanite.Tessellation=1` in the test profile. Enable material tessellation, feed linear height into Displacement, start with magnitude 1–2 cm and center 0.5, and fade displacement when subpixel. Keep normal detail for the far field. For persistent footprints/craters, stamp a bounded render-target height mask and combine it with the static height texture; cap the number and resolution of update regions. Render-only deformation must not be used as authoritative collision: deeper craters require a separately updated collision/navigation representation. Leave this branch disabled in the normal MVP.

## 4. Cinematic Niagara explosion authoring

### Asset and parameter contract

Implemented contract: Niagara Fluids is enabled; the engine's `Grid3D_Gas_Explosion` is duplicated to `/Game/Cinematic/NS_CinematicExplosion`, preserving the engine asset. The fire accepts `User.ResolutionMaxAxis`, `User.WorldSpaceSize`, `User.SourceOffset` and `User.DrawBounds`. The separate `/Game/Cinematic/NS_ExplosionSparks` accepts connected `User.SparkCount`. The director clamps domain size and terminates both systems at four seconds. Radius controls fluid domain and flash attenuation; the source particle simulation retains the template's internally consistent source-radius/temperature setup. Further source-scale art direction requires tuning these together. [Epic: Fluids reference](https://dev.epicgames.com/documentation/unreal-engine/niagara-fluids-reference-in-unreal-engine?lang=en-US)

1. **Core source:** begin with a sphere of density, temperature, fuel and outward radial velocity injected for roughly 0.05–0.12 seconds. Bind radius to the user parameter. Add low-frequency curl variation and upward buoyancy; avoid a perfectly expanding sphere.
2. **Grid simulation:** start with a 64³ domain for iteration; use a single 96³ or 128³ hero domain only after measuring. Doubling each dimension multiplies voxel count by eight. Keep world bounds tight to the expanding plume with enough headroom for smoke; do not use screen-sized empty grids for small impacts.
3. **Fire/smoke rendering:** retain the template's volume-compatible renderer/material. Map temperature to a blackbody-like ramp, density to extinction, and add scattering/albedo for smoke. Animate fuel/temperature down while density persists. Do not substitute a normal opaque surface material. Control ray-march steps/shadow sampling by quality tier.
4. **Production fallback:** bake a representative hero simulation to flipbook or sparse volume playback. Use real-time fluid simulation for a rare hero event, cached/flipbook smoke for overlapping combat effects. The generic director's eight slots are for the composite baked system, not permission to run eight 128³ solvers.

### GPU sparks

1. Add a GPUCompute emitter with an instantaneous burst driven by `User.SparkCount`, default 2000. Suggested lifetime 0.25–1.0 seconds, speed 600–1600 cm/s, gravity -980 cm/s², modest drag, and randomized radial direction from the fireball surface.
2. Use a velocity-aligned emissive sprite or very small mesh. Fade white/yellow through orange to black using normalized age. Clamp sprite size and bloom to avoid washing out silhouettes. Start with sprite widths 0.5–2 cm; art-direct only after profiling.
3. Add Collision before final position/velocity integration as required by the chosen template. Start with GPU distance-field collision for static architecture: restitution about 0.35, friction about 0.2, kill after a short post-hit age. Test thin surfaces and fast sparks; increase collision radius or reduce speed if tunneling occurs.
4. Scene-depth collision is cheaper but view-dependent and cannot reliably collide behind the camera or through occluders. Distance fields do not provide exact deformed skeletal collision. Hardware-ray-traced GPU collision is experimental and requires its own support/settings pass; compare it as a separate quality tier. These particles are visual simulation, not authoritative Chaos rigid bodies. [Epic: GPU ray-traced collision](https://dev.epicgames.com/documentation/en-us/unreal-engine/gpu-raytracing-collisions-in-niagara-for-unreal-engine)
5. Emissive sparks provide visible glow. Use the pooled point-light flash to illuminate the world; do not create thousands of shadow-casting particle lights. If extra moving light glints are essential, use a tiny bounded CPU light subset and measure it separately.

### Flash light code and Blueprint wiring

The compiled implementation is `CinematicExplosionDirector.h/.cpp`. It preallocates eight pairs of Niagara components and eight point-light components once, loads both soft systems before combat, and exposes `TriggerExplosion(Position, Radius)`. At most two pairs may activate their fluid component. It uses lumens, a warm color, a capped attenuation radius, and this decay. The implemented director additionally shares a 4000-lumen peak budget across simultaneous flashes to preserve visibility with the prototype's fixed exposure; calibrate that value against the final exposure/lighting setup.

```cpp
// Per active pooled slot, in Tick; no actor creation on impact.
const float T = FMath::Clamp(Age / 0.2f, 0.f, 1.f);
Light->SetIntensity(PeakLumens * FMath::Square(1.f - T));
if (Age >= 0.2f) {
    Light->SetIntensity(0.f);
    Light->SetVisibility(false);
    Light->SetCastShadows(false);
}
```

Assign an optional `UCurveFloat` with keys (0,1), (0.03,0.65), (0.10,0.15), (0.20,0) for art direction. A Blueprint alternative is a 0.2-second Timeline that sets pooled light intensity from the same curve, then hides the component on Finished. Acquire from the director before playing the timeline; do not SpawnActor for every hit. Trigger damage first, request visuals second. Culling or exhausted VFX slots must never suppress gameplay damage.

### Scalability contract

Create `NET_CombatExplosion`, assign it to the system, use distance significance, fixed bounds and a kill/deactivate response appropriate to nonlooping bursts. Configure system/emitter/renderer overrides and test offscreen transitions; GPU emitters still carry CPU system/emitter work. [Epic: Niagara scalability](https://dev.epicgames.com/documentation/en-us/unreal-engine/scalability-and-best-practices-for-niagara)

| Tier | Sparks per admitted burst | Composite concurrent cap | Fluid grids | Shadow flash lights |
| --- | ---: | ---: | ---: | ---: |
| Low | 128 | 4 | 0 | 0 |
| High combat | 512–1000 | 8 | 0 (baked core) | 1 |
| Cinematic test (implemented) | 2000 | 8 | at most 2 at 64³ | 2 |

Low and High combat remain proposed tiers; no baked fallback is supplied. The implemented director enforces eight slots, a 6000 cm camera distance, a 4-second watchdog, two fluid grids and two shadow flashes. When the fluid budget is occupied, accepted effects retain sparks and light; when all eight slots are occupied, additional visual requests are dropped while gameplay damage continues. An Effect Type asset, screen-size significance and spatial event clustering remain future scalability extensions; the current hard caps are enforced in C++.

## Validation and shipping gates

Capture a matrix at 120/300/600/1000 enemies with 0/1/4/8/32 requested overlapping explosions. Record native output resolution, internal screen percentage, CPU/GPU model, build configuration, shader warm state, VRAM, frame median/p95/p99, and game/render/GPU thread time. Log accepted/dropped explosions so a stable frame rate cannot hide missing effects. The 32-request case must degrade visually while preserving damage.

Use Unreal Insights (`-trace=cpu,gpu,frame,bookmark`), `stat unit`, `stat gpu`, `profilegpu`, Niagara Debugger and animation profiling. At 60 FPS the frame budget is 16.67 ms; reserve measured headroom before adding hero features. Compare Lumen, VSM, fluid grids, GPU collision and skin SSS independently, then together. Run packaged Development tests after editor validation; compile/cook shaders and prewarm representative effects to separate first-use hitches from steady-state cost.

Production readiness requires authored/licensed high-detail assets, verified normals and animation deformation, cooked content availability, explicit hardware targets, memory and frame-time budgets, save migration tests, restart/pool lifecycle tests, and platform-specific fallback tiers. Multiplayer, minion navigation/aggro, authoritative replication and packaged performance certification are not implemented by this single-player prototype.

## Arena architecture and zone progression (UE 5.8)

### What is installed versus the next map workflow

The playable MVP now has mouse-wheel camera zoom (850–2200 cm, 100 cm per wheel unit, smoothly interpolated). It saves after 0.5 seconds without zoom input, retries failed writes after 5 seconds, and flushes pending changes on normal world teardown. `Saved/SaveGames/CodexPreferences.sav` is separate from the starting deck. Camera input is ignored in menus; smoke tests use a separate automation slot and benchmarks ignore personal preferences.

The native Codex widget now uses nine imported Meshy textures under `/Game/UI/Codex`, rarity frames and labels, school icons, hover lift, and staggered real-time reveal animation while gameplay is paused. Rarity is authored display metadata, not a new rarity-weighted drafting algorithm. Common/rare/epic are used by the starter definitions; legendary is available for future cards. The approved image task `01a14a65-3524-71fd-b0ca-ffb87e1b93fa` consumed 9 credits. The source sheet and cropped PNGs live in `ContentSource/Meshy/CodexUI`; `Scripts/Split-CodexArt.ps1` and `Scripts/import_codex_ui.py` reproduce the imports without another paid generation.

`ArenaSetupCommandlet` authors `/Game/MVP/Arena`: NavMeshBounds_EntireArena is 18000 x 18000 x 4000 cm, centered at (0,0,1000); its vertical range is -1000..3000. A hidden static floor proxy has top Z=-15 and supplies saved navigation geometry for the runtime-generated visible floor. Four invisible blocking volumes have inner edges at +/-7000 cm. Rebuild via `UnrealEditor-Cmd.exe DeckBuilder.uproject -run=ArenaSetup -unattended -NullRHI` using absolute paths. The tool updates its tagged actors rather than duplicating them. Press P in the editor to inspect navigation.

These additions do not convert the existing player/enemies to grounded pathfinding. AMVPPlayer still clamps Z to 55 and uses planar FloatingPawnMovement; AMVPEnemy integrates direct XY offsets with nonblocking crowd collision. Navigation bounds alone cannot make those units climb ramps or route around obstacles. The current perimeter position clamp remains active. The elevation design below is a migration architecture, not a claim that the playable MVP already supports trenches or cliffs. Remove the flat proxy when replacing the floor with a real multi-height map, otherwise it can create an unwanted walkable layer beneath trenches.

### Geometry and navigation workflow

1. Create a new zone map, keeping the survival arena as a regression map. Block out 140 x 140 m with a central 0 m courtyard, a +2 m ritual plateau, -1.5 m trenches, and +4 m inaccessible cliffs. Use at least two 6 m-wide routes to each combat space. For a 2 m rise, start with a 6–8 m ramp rather than tiny stairs. Do not let hundreds of enemies funnel through one doorway.
2. For organic terrain use Landscape with edit layers: base shape, gameplay ramps, erosion and dressing. For a crypt/citadel use a modular Nanite kit with floor slabs, straight/corner walls, broad ramps, cliff caps and transitions. Author pivots on a common grid, matching edge heights and consistent centimeters. Import Meshy FBX into a staging folder, inspect scale/normals/material slots, and create shared material instances. Generate new assets only after a separate credit approval.
3. Supply simple convex UCX collision or carefully authored simple collision for walls/rocks. Use a separate low-complexity ramp collider under visually detailed steps. Nanite render triangles are not your collision or navigation representation. Inspect collision overlays independently of Nanite visualization.
4. Migrate the player to a grounded ACharacter/capsule with CharacterMovement. Remove plane constraints and the forced Position.Z=55 assignment. Begin with MaxStepHeight about 40 cm and WalkableFloorAngle about 40 degrees, then tune against the chosen capsule. Keep the camera boom independent of mesh rotation. Migrate minion following, enemy contact distances, projectile sweeps and gem collection to height-aware tests as well; XY-only distances can otherwise hit units on another floor.
5. For the first terrain test use pooled ACharacter enemies with AIController and path following. On activation reset movement mode, velocity, collision, target and health; on return StopMovement, clear path requests/timers, disable movement/component ticks and animation, and return through the owning pool. Repath on a staggered budget (for example 4 Hz per agent with a global request cap), not a full synchronous path search per enemy per frame. Choose Detour Crowd OR RVO for the test, then profile; do not assume enabling both improves it. For thousand-unit targets evaluate a shared flow field/Mass representation after the correctness prototype.
6. Drop a NavMesh Bounds Volume over the ENTIRE playable arena, including the lowest trench and highest reachable plateau. Match Recast agent radius/height/step/slope to the real collision capsule. Build static navigation for static terrain; use Dynamic Modifiers Only for doors/blocked areas where the base mesh already exists. Use full Dynamic only if navigation geometry itself changes. Use Nav Links only for explicit traversal actions, not as a substitute for a walkable ramp. View green coverage with P; test paths across every ramp, under ledges and around walls.
7. Replace the current flat offscreen spawn ring with offscreen candidates projected onto navigation, verified reachable from the player's nav island and checked for capsule clearance. Keep spawn retry attempts bounded. Do not simply set spawned enemy Z to the player's Z. Portal locations use the same navigation/clearance principles.

### Master landscape material and virtual textures

Enable Virtual Texture Support and restart the editor before building the production landscape graph. Keep this in a dedicated quality profile until shader compile time and memory are measured. Author MF_Stone, MF_Mud and MF_Roots with BaseColor, tangent-space Normal, Roughness and height inputs, exposed texture/scalar parameters and shared sampler usage. Give each surface independent UV scale. Pack ORM consistently and disable sRGB on masks/normals.

Use LandscapeLayerBlend for painted coverage: keep one weight-blended base layer to avoid all-zero height blends, and height-blend the stone/mud/root transitions. Add world-space macro variation at roughly 4–12 m and distance-dependent normal/detail blending; rotate or use stochastic/world-aligned sampling on cliffs, and break repeating silhouettes with geometry/decal clusters. Macro variation and texture blending suppress tiling; VT alone does not.

For RVT blending create RVT_ArenaSurface with a base-color/normal/roughness layout and a Runtime Virtual Texture Volume enclosing the entire landscape. Write those attributes through Runtime Virtual Texture Output from the terrain. For rocks and modular skirt meshes, use a matching Runtime Virtual Texture Sample Parameter to blend the foot region with the underlying terrain. Use a world-height RVT only when the blend requires height; match the volume height range and material type. Avoid reading and writing the same RVT in a feedback loop. RVT is an attribute cache; Streaming Virtual Texturing is texture residency management. Neither adds physical deformation.

Keep fine cracks and dirt in normal/height maps. If using Nanite displacement/tessellation for visible ground relief, enable the version-appropriate project and material controls, set bounded displacement magnitude/center, and keep the physics/navigation surface authored separately. Do not create ankle-deep physical pits from a shading texture. Profile `stat virtualtexturing`, VT memory, material instruction cost and VSM page invalidations at near and far camera zoom.

### Collision layers and foliage

Configure these profiles in Project Settings > Collision for the new zone. Do not apply them blindly to the existing planar crowd:

| Layer | Object type / preset | Responses | Navigation | Rendering |
| --- | --- | --- | --- | --- |
| Floor, ramps, Landscape | WorldStatic / BlockAll | Block Pawn and Enemy; allow ground traces | Included, walkable by slope | Opaque lit, shadows enabled |
| Walls, rocks, tree trunks | custom Environment object channel / EnvironmentSolid profile | Block Pawn, Enemy, projectiles and visibility as appropriate | Included, obstacle collision | Nanite where appropriate, shared material instances |
| Grass, moss, carpets, small rubble | WorldStatic / NoCollision | Ignore all; overlaps disabled | CanEverAffectNavigation=false | CastShadow=true AND CastDynamicShadow=true |
| Interactive aesthetic volumes | custom trigger channel / QueryOnly | Overlap Pawn only | Excluded | Separate visual mesh, usually NoCollision |
| Outer boundary | BlockingVolume / InvisibleBoundary profile | Block Pawn and Enemy; usually block projectiles | Obstacle | Hidden in game, no visual shadow |

Both sides must agree to block. A Pawn set to Ignore Environment will still pass through an EnvironmentSolid component. Make the movement component's UpdatedComponent the actual root capsule, and sweep movement; setting a collision profile on a decorative child mesh cannot fix unswept actor offsets. Avoid per-frame overlaps on thousands of foliage instances.

For the perimeter, use four tall box BlockingVolumes just outside the walkable edge, extend them below trenches and above any climbable ledge, and overlap their corners. They are already placed in the MVP; grounded characters must use blocking responses to benefit from them. Do not confuse a NavMesh Bounds Volume with a blocking volume.

For pass-through foliage, select the FoliageType/mesh component, choose NoCollision, disable navigation relevance, enable Cast Shadow and Cast Dynamic Shadow, and use an appropriate two-sided foliage/masked material for thin leaves. Separate colliding trunks from noncolliding canopies when necessary. Keep WPO bounded and set sensible instance cull/shadow distances. Explosion point lights already use a two-shadow-light budget. VSM/dynamic direct lights produce these moving shadows; Lumen provides GI/reflections, not collision or an automatic promise of foliage shadows. Validate close-ups and the widest zoom; masked overdraw, WPO and shadow cache invalidations may dominate even on Nanite foliage.

### Progression manager and portal Blueprint

The compiled `Source/DeckBuilder/MortisProgressionManager.h/.cpp` provides CurrentWaveIndex, TargetKillCount, CurrentKillCount, RemainingSessionTime, explicit Waiting/Combat/Cleared/Traveling/Expired states, BeginSession, OnEnemyKilled, FindExitGround, CommitRewardAndTravel, and Blueprint events OnLevelObjectivesCleared/OnSessionExpired. SessionDuration=0 means untimed. BeginSession is authority-only and only accepted from Waiting. Kill notifications before start/after clear/after expiry are ignored; the clear latch is set before callbacks, then wave spawning stops. The existing GameMode forwards kills after the pooled enemy is already inactive and starts placed managers only when the selected run begins. Place exactly ONE manager per objective zone; none is placed in the endless survival arena by default.

Create BP_ZoneProgression from AMortisProgressionManager and BP_CodexExit from Actor (Scene root, gate StaticMesh, Niagara loop component, PointLight, Pawn-only overlap capsule, interaction prompt). In OnLevelObjectivesCleared:

1. Spawner.StopWaves is already called natively. Decide explicitly whether surviving enemies remain dangerous, retreat, or return to their pool; stop attack timers if presenting a reward modal. Do not globally pause before a portal's actor timeline if you expect that timeline to animate.
2. Call FindExitGround(PlayerLocation, 1500). It selects a reachable navigation point, traces the WorldStatic floor, rejects steep normals and checks capsule clearance. A false result means no exit is spawned: try a pre-authored safe exit anchor and validate it, then show a retry/error state. Never silently spawn at (0,0,0). Adapt the clearance shape to the final gate's dimensions and check for existing portal/prop occupancy.
3. Spawn BP_CodexExit at the ground point with the gate oriented toward the player. Use one cinematic reveal: gate scale/opacity 0->1, Niagara soul ribbons and embers, a brief bounded light pulse, and camera shake. Set CanEverAffectNavigation=false on its nonblocking visual so it cannot invalidate its own path. Keep the interaction capsule QueryOnly/Overlap Pawn.
4. Present one persistent Codex reward selected from an authored eligible-card catalog. This is distinct from the mid-run mutation hand. Store the chosen UCardDataAsset on the portal and disable repeat interaction while committing. Existing four starter cards are all already unlocked; author additional locked card assets/catalog rules before claiming this adds new progression content.
5. On confirmed interaction, call CommitRewardAndTravel(Reward). Configure NextZone as a soft UWorld asset on the manager beforehand. The function checks authority, Cleared state, reward ID/class, and a valid /Game map package; loads the versioned `MortisProgress.sav`; adds the stable ID uniquely; saves successfully BEFORE requesting travel; sets bUseSeamlessTravel and calls World.ServerTravel with `?Wave=N+1`. If the save fails, keep the gate usable and show a retry message. If travel is rejected, return to Cleared; the reward remains owned and cannot duplicate. Asynchronous travel failures additionally need a GameInstance travel-failure handler to restore the UI/state or offer a safe return to the hub.
6. Use a small TransitionMap in GameMapsSettings, configure all destination maps in the packaging cook list, and test with packaged standalone/listen-server builds. Do not use OpenLevel as a supposed seamless-travel implementation. The existing GameMode reads the Wave option on the next run. Different destination GameModes must do the same explicitly.
7. In the destination's pre-run load stage, load MortisProgress.UnlockedCardIDs and resolve them through the authoritative card catalog/Asset Manager before populating Deck.UnlockedCards. Do not serialize spell actors, pointers or runtime mutation instances. Preserve an active run's mutations/health/XP in a GameInstance subsystem snapshot of IDs and values if zones should form ONE continuous run; the current MVP treats a new map as a new pre-run loadout and does not yet restore that snapshot. The saved reward list is implemented, but new unlock catalog ingestion, portal presentation and continuous-run transfer are Blueprint/content integration work.

No next-zone maps, cinematic gate mesh/VFX, continuous-run snapshot or Landscape material assets are created by this framework. Those steps require authored zone content and the grounded-movement migration above. Test timeout before target, last-kill-at-timeout ordering, repeated death callbacks, stacked level-ups at clear, failed saves, missing maps, unavailable nav points, player death during gate reveal, and travel while a UI is open. The implemented state machine is single-player/server authoritative; replicated objective UI requires replicated state/GameState and OnRep or delegates.

Sources: [Epic RVT overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing), [Epic navigation avoidance](https://dev.epicgames.com/documentation/unreal-engine/using-avoidance-with-the-navigation-system-in-unreal-engine), [Epic shadowing](https://dev.epicgames.com/documentation/en-us/unreal-engine/shadowing-in-unreal-engine). Local UE 5.8 headers and editor brush-factory code were used to validate the compiled APIs.
