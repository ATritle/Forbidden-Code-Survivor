# Cinematic explosion test — UE 5.8.2

## Run it

Open PowerShell in `C:\dev\DeckBuilder`:

```powershell
.\Play-CinematicTest.ps1 -Enemies 300
```

Choose a starting deck and enter the arena. **F** creates a test explosion; Ember Fan and explosive mutations use the same effect automatically. The normal `Play-MVP.ps1` retains its inexpensive renderer and impact pulses.

For a higher-resolution hero simulation:

```powershell
.\Play-CinematicTest.ps1 -Enemies 300 -FluidResolution 96 -FluidGrids 1
```

## Implemented assets and lifecycle

- `/Game/Cinematic/NS_CinematicExplosion`: Epic's 3D gas explosion template duplicated into the project. It retains its GPU source particles, simulation stages, temperature/density fields and volume material/renderer. This is real simulated volumetric fire and smoke.
- `/Game/Cinematic/NS_ExplosionSparks`: one GPU emitter, instantaneous `User.SparkCount` burst (2,000), random spherical directions, 750 cm/s launch speed, gravity, drag 0.65, HDR orange particle color, fading size/alpha, lifetime range 1–2.8 seconds and GPU distance-field collision. Restitution 0.45 and friction 0.3. Collision is placed immediately before Solve Forces and Velocity, following [Epic's module ordering requirement](https://dev.epicgames.com/documentation/unreal-engine/particle-update-group-reference-for-niagara-effects-in-unreal-engine?lang=en-US).
- `ACinematicExplosionDirector`: eight preallocated component pairs and lights; at most two simultaneous 64³ fluid grids and two shadow-casting flashes by default. Admitted effects beyond the fluid budget retain sparks/light. Requests beyond eight occupied slots are dropped. Counters expose both admission and fluid admission, independently of gameplay damage.
- Flash: warm movable point light, shared peak budget 4,000 lumens, quadratic decay to zero in 0.2 seconds. No per-spark shadow lights. Sparks are emissive; the pooled flash directly illuminates the arena.
- Four-second slot watchdog; both Niagara components stop on reuse/teardown. Camera-distance admission limit 6,000 cm. Spark bounds cover the complete ballistic trajectory. The source explosion is one-shot; its density can linger until the watchdog stops it.

The cinematic launcher explicitly selects DX12 **and SM6**, mesh distance fields, Lumen GI/reflections and virtual shadow maps. DX12 alone was insufficient: this project otherwise chose SM5. Hardware ray-traced collisions are not enabled. GPU distance fields approximate static scene geometry and do not give exact animated skeletal collision or gameplay-authoritative physics.

The existing crowd remains the imported low-poly Meshy batch with crowd shadows disabled. This VFX test does not substitute high-poly characters or certify a finished cinematic art direction. A 64³ grid is intentionally soft; the 96³ option allows a quality/cost comparison. Source scale is authored in the template; the C++ Radius argument controls domain size and flash reach rather than uniformly scaling every fluid source attribute.

## Reproduce profiling

```powershell
.\Profile-Explosions.ps1 -Name Baseline -Enemies 300 -ExplosionRate 0
.\Profile-Explosions.ps1 -Name Combat300 -Enemies 300 -ExplosionRate 2
.\Profile-Explosions.ps1 -Name Combat1000 -Enemies 1000 -ExplosionRate 2
.\Profile-Explosions.ps1 -Name Overload -Enemies 300 -ExplosionRate 1 -Burst 32
.\Profile-Explosions.ps1 -Name Hero96 -Enemies 300 -FluidResolution 96 -FluidGrids 1
```

Each run uses a real rendered offscreen viewport, 1920×1080 output, 75% internal screen percentage (nominal 1440×810), uncapped frame rate and VSync off. Fifteen seconds warm up precede thirty seconds of samples. Run cases sequentially without another GPU workload. The benchmark automatically enters combat, stops player spell damage and waves, prevents player death, and keeps the exact requested enemy count alive. Enemies continue their normal chase and skeletal animation ticks; inner ranks recycle to the outer visible ring to prevent all actors stacking at the player's feet. This is a repeatable crowd/VFX workload, not a complete endgame combat benchmark.

`Saved/Benchmarks/<Name>.csv` contains per-frame wall time, raw stat-unit game/render/GPU timings, active enemy count and cumulative effect counters. The PowerShell runner writes a percentile summary beside it. Game/render timings can include synchronization and must not be added to GPU time. Samples exclude startup and the later three screenshot captures. GPU timing is engine-reported and can lag the corresponding CPU frame. First-run shader compilation and PSO costs are outside this warmed steady-state comparison; packaged Development profiling remains required before release.

After the measured window, three captures show an isolated burst at ignition, fire/sparks and smoke ages with the animated crowd still running. They are written to `Saved/Screenshots/<Name>-1.png` through `-3.png`. A separate `-DebugEffects` pass logs Niagara execution state and reported particle counts; leave it off during timing runs because its synchronization/logging adds overhead. The validation run reported 2,000 spark particles at age 0.5 seconds.

## Measured results — 2026-10-17

Hardware: AMD Ryzen 9 7900X3D, Radeon RX 7900 XTX (24 GB), approximately 32 GB system RAM, Windows 11. UE 5.8.2 Development Editor running standalone `-game`; Epic scalability (quality 3), software Lumen, virtual shadow maps, DX12/SM6. All rows below use 1080p output / 75% internal resolution and the warmed 30-second method above. These are individual local runs, not cross-device guarantees.

| Case | Frame median / p95 / p99 (ms) | GPU median (ms) | Accepted / dropped / fluid admissions in sample |
| --- | --- | ---: | --- |
| 300 enemies, no explosions | 5.171 / 5.876 / 6.273 | 3.526 | 0 / 0 / 0 |
| 300 enemies, 2 requests/sec, two 64³ grids | 5.732 / 6.407 / 6.798 | 4.192 | 60 / 0 / 14 |
| 1,000 enemies, 2 requests/sec, two 64³ grids | 16.828 / 18.354 / 19.437 | 7.308 | 59 / 0 / 14 |
| 300 enemies, 32 simultaneous requests every second | 5.753 / 6.421 / 6.754 | 4.122 | 56 / 904 / 14 |
| 300 enemies, 2 requests/sec, one 96³ grid | 5.673 / 6.331 / 6.690 | 4.147 | 60 / 0 / 7 |

The counter for active enemies stayed at the requested count for every measured frame. The overload row deliberately rejects most visual requests: its stable timings must not be interpreted as rendering 32 complete explosions per second. The four-second occupancy and eight-slot cap bound admission to about two effects per second over time. Only the fluid-admitted subset contains the gas core; the others contain sparks and flash lighting.

At 1,000 enemies the game-thread median was 16.794 ms, versus GPU median 7.308 ms. That scene does **not** hold steady 60 FPS (16.67 ms budget). Crowd animation/tick work needs further optimization before increasing crowd fidelity; a full trace should separate those costs before choosing animation sharing, significance-based update rates or Mass. The 300-enemy effect added roughly 0.56 ms median frame time over this baseline.

The 96³ row trades the second simultaneous fluid core for detail in a single core; it is not a same-grid-count resolution comparison. It admitted seven fluid cores versus fourteen in the two-grid rows. Small timing differences between these individual runs should not be treated as statistically significant.

Raw evidence: `Saved/Benchmarks/E300_Baseline.{csv,json}`, `Final_E300.{csv,json}`, `Final_E1000.{csv,json}`, `Final_Burst32.{csv,json}`, `Final_Hero96.{csv,json}` and matching `Saved/*.log`. Preview and earlier non-Final experimental runs are not the final configuration.

## Verification

Final Development Editor build succeeded (`Saved/VFXBuild.log`). Asset generation completed with `NIAGARA_AUTHOR_OK`, zero commandlet errors and zero warnings (`Saved/AuthorExplosion.log`). The rendered cinematic `-MVPSmoke` run passed (`Saved/ExplosionSmoke.log`), including combat, Codex/level-ups, mutations, minions, pool admission (8 accepted / 24 dropped from 32 simultaneous requests), at most two shadow lights and complete flash fade-out. Its pool-cap assertion waits for preceding effects' four-second lifetime before issuing the burst. Both native tests passed: `Codex.Runtime.DeckAndMutations` and `Codex.Runtime.PoolLifecycle` (`Saved/ExplosionAutomation.log`). Fire, sparks and smoke captures were visually inspected. No packaged distribution or cross-hardware performance certification was performed.

## Regenerate the assets

`Source/DeckBuilderVFXEditor` is an editor-only module; no Niagara graph-editing dependencies enter the runtime module. Close the editor before rebuilding. The commandlet overwrites the two generated project assets, so preserve manual art edits before regenerating.

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' .\DeckBuilder.uproject -run=ExplosionAuthoring -sm6 -unattended -nop4 '-ini:Engine:[/Script/Engine.RendererSettings]:r.GenerateMeshDistanceFields=True'
```

Require `NIAGARA_AUTHOR_OK` and no compilation errors. The commandlet also writes `Saved/NiagaraStructure.txt` for inspection. It rebuilds the system's emitter graph through Niagara editor utilities, explicitly binds SparkCount, selects GPU Distance Fields using the installed enum, and saves compiled assets. Engine templates are never modified. `NiagaraFluids` must remain enabled; `/Game/Cinematic` is included in the existing cook directories. A packaged build has not been validated.
