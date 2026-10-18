# Neon interface and spell identity hooks

The playable implementation is native UE C++ and authored materials. It is not a static UI mockup. The main menu uses a new built-in imagegen backdrop (`ContentSource/UI/NeonGlass/NeonDistrict-Menu.png`); its full prompt is preserved beside it in `generation-prompt.txt`. The PNG is a menu illustration, separate from the playable 3D city.

## Menu presentation

`Source/DeckBuilder/NeonCodexPresentation.cpp` draws the menu and caches one material instance per visible control. `MVPHUD.cpp` retains the existing deck actions and coordinate system, so filtering, paging, saving, removing starter cards, entering the arena, and opening settings still use live game data. Design coordinates are 1600 × 900; input and drawing apply the same scale and letterbox offsets. Chamfered primary buttons reject clicks in their sliced corners.

The glass material samples the matching backdrop area with nine anisotropic taps: a wide horizontal filter and narrow vertical filter. A faint analytic hex grid, signed-distance frame, soft glow, and time-driven rim pulse sit above that blurred sample. The main menu's glass blur samples the generated menu image, not a capture of the live 3D arena. In-game HUD panels use the same frame with a transparent navy tint and no menu-image contribution.

School tabs have text, thin vertical separators, and a cyan active underline. Primary actions use clipped diagonal corners and a repeating internal chevron scan. Selection is represented by the card's glowing perimeter and installed label. Hover interpolation uses real elapsed time while the world is paused; cards lift, art expands slightly, and text/button rims brighten. Titles use `/Engine/EngineFonts/RobotoDistanceField` with native `FCanvasTextItem` distance-field glow and restrained RGB offsets. Body text stays stable and legible.

The HUD exposes `InterfaceAccent` and `SetInterfaceAccent(FLinearColor)` to Blueprint. Glass, art, and backdrop material references are editable defaults on a HUD Blueprint subclass. `Scripts/author_neon_interface.py` creates both Canvas (`M_NeonGlass`, Surface domain) and UMG (`M_NeonGlass_UMG`, User Interface domain) versions.

| Material parameter | Meaning |
| --- | --- |
| `Backdrop` | Menu texture, or the card's illustration for `M_NeonCardArt` |
| `Rect` | Panel's normalized X, Y, width, height in the backdrop |
| `PanelSize` | Design-space width and height, for uniform pixel borders |
| `Accent` | Neon RGB color |
| `Control.x` | Hover amount, 0–1 |
| `Control.y` | Selected/installed amount, 0–1 |
| `Control.z` | Frame mode; 2 enables action-button chamfers and scans |
| `Control.w` | Whether to sample the blurred menu backdrop |

For UMG customization, assign a dynamic `M_NeonGlass_UMG` instance to an Image or Border brush, supply its `Rect` and `PanelSize`, and animate `Control.x` from OnHovered/OnUnhovered. Keep functional labels as actual text. The existing level-up and settings widgets preserve their native input and pause ownership.

## Spell identities

`NeonSpellStyle.h` defines the palette and card-ID mapping. Each spell forwards its style into the existing pooled projectile; mutations preserve that style. Visual bob/tumble never moves the collision root.

| Spell | Color and projectile | Trail and impact |
| --- | --- | --- |
| Pulse Dart | Mint hard-light dart; rapid axial pulse | Twin dashed rails; EMP rings and cross flashes |
| Prism Burst | Hot-pink swirling plasma orb | Tapered plasma wake; pink colliding sparks and aura |
| Halo Nova | Violet rotating faceted shards | Broken angular echoes; violet crystal splinters |
| Photon Lance | Cyan elongated lance; moving energy rings | Narrow laser filament; concentric cyan ripples |
| Kinetic Cards | Electric-gold etched cards; spin and tumble | Segmented card echoes; gold diamond fragments |
| Talisman Guard | Teal orbiting protective sigils | Teal hexagonal ward shatter on contact |

`SetSpellStyle(ENeonSpellStyle)` is Blueprint-callable before visual setup/launch. `ANeonSpellImpactDirector` preassigns six effects each for Pulse, Halo, Photon, and Barrier (24 total) and four nonshadowed flash lights. Gold retains its separate twelve-slot/four-light pool. Pink plasma retains the existing cinematic director. No impact spawns replacement actors when its pool fills. Niagara spark collisions affect visuals; authoritative swept projectile collisions and damage remain C++.

Rebuild the new materials with `Scripts/author_spell_identities.py`, then run the editor commandlet `-run=SpellAuthoring -IdentitiesOnly`. It creates three distinct ribbon systems and four bounded GPU impact systems. Gold and pink authoring workflows are unchanged.

## Audio, health, and readability

The two mechanical MP3 recordings ending in `233833` and `233841` are reserved for a future specific enemy. Normal enemy activation/contact/death, attacks, and impacts do not trigger them. `ANeonAudioDirector::PlayEnemy` is an explicit Blueprint hook for that future enemy. The old impact derived from the mechanical recording is replaced by the supplied plasma `pp-01-233828` recording. `ContentSource/Audio/manifest.json` records each source and reserved-only status.

At **0 < health < 30**, the dedicated warning voice plays `rescopicsound-ui-alert-alarm-modern-interface-warning-01-230492.mp3` once per low-health episode. It plays at full configured volume for five seconds, fades to silence over 0.75 seconds, and stays silent while health remains low. It follows the saved SFX volume; changing volume during the fade preserves the fade envelope. Playback and the five-second countdown pause during gameplay menus. Recovery to 30 HP, death or leaving the run stops and rearms it for the next low-health episode. `M_CriticalHealth` independently draws the pulsing screen-edge warning with a clear center until health recovers, even after audio stops. Gameplay music continues during drafts and settings.

The standalone `-MVPSmoke -HealthAlarmSmoke` check verifies the threshold, full five seconds, fade, silent visual warning, pause handling, SFX adjustment during the fade, recovery and rearming. It saves `Saved/AudioQA/HealthAlarmFade.wav` and `Saved/Screenshots/LowHealth-SilentWarning.png`, using automation save slots.

Verified October 18, 2026: Development Editor build and `HEALTH_ALARM_PASS`. At 5.251 seconds of unpaused playback the gain was 0.665, and after 5.75 seconds the voice stopped while the screen-edge warning remained active at 29 HP. The recorded output contained audible alarm audio and digital silence in its final 0.6 seconds. Recovery to 30 HP and a new drop below 30 correctly restarted the warning once.

Two shared unshadowed lights use lighting channel 1. Player and enemy meshes receive channels 0 and 1; city geometry uses channel 0. The fills have global illumination disabled and zero indirect intensity, so they improve character readability without adding Lumen bounce to the night environment or requiring a light on every enemy.

## Validation

`Saved/NeonPolishBuild.log` records the successful native build. `Saved/NeonPolishSmoke.log` exercises all five active projectile identities together, the teal guard, school-specific impact pools, zero playback of reserved enemy recordings, the strict below-30-HP warning threshold, recovery, character-light channels, school tabs/paging, and gameplay music during drafts. Screenshots are `Saved/Screenshots/NeonCodex-MainMenu.png` and `NeonSpells-LowHealth.png`.

`Saved/NeonPolishSettingsRegression.log` verifies actual UMG slider delegates, mute independence, autosave during pause, persistence across a map restart, and safe settings over an outstanding card choice. The tests use only automation save slots. The final recorded mix (`Saved/AudioQA/NeonPolishLowHealth.wav`) lasts about 5.67 seconds, peaks at 0.388 full scale, and contains no clipped samples. The log scan found no remaining shader compilation errors in the rendered acceptance run.

For a crowd test that keeps the five offensive spells firing, pass `-ExplosionBenchmark -BenchAllSpells -StressEnemies=180`. It excludes the consumable instant-kill guard so the crowd remains stable. This complements the ordinary benchmark mode, which stops weapons to isolate explosion costs.

`Saved/NeonPolishCrowd.log` and `Saved/Benchmarks/NeonFullSpell180.csv` record the final 1600 × 900 offscreen test with 180 active enemies, all five offensive spells, audible SFX, the new character fills, Lumen, rain, and eight full explosion requests per second. A five-second warmup precedes a ten-second sample: 1039 frames, mean 9.62 ms, median 9.50 ms, 95th percentile 11.99 ms, and median GPU time 5.11 ms. Effect pools drop overflow as designed. These are local editor-launched measurements, not a packaged-build or other-hardware guarantee. `Saved/NeonPolishCityRegression.log` also passes the physical ramp, wall navigation, camera-save, XP, and unique-draft regression checks.
