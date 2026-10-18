# Meshy monsters in UE5

Use Meshy to produce source assets, then import/cook them into Unreal assets. Runtime soft references point to cooked `UStaticMesh`/`USkeletalMesh` objects, not raw FBX files. No Meshy generation job or external upload is needed for this code framework.

## Import and prepare

1. Keep a consistent scale, forward direction and floor pivot across generated models. Clean detached fragments, inverted normals, redundant materials and needless geometry in a DCC tool; verify UVs before baking or texturing.
2. For animated monsters, generate/prepare a rig and skin weights and export a skeletal FBX. An unrigged static model cannot use an animation Blueprint merely by switching its reference. Reuse a skeleton only when bone hierarchy/bind pose are compatible; otherwise use an appropriate animation set or retarget with IK Rig/IK Retargeter.
3. Import as Static Mesh for rigid props/weapons, Skeletal Mesh for rigged characters. Verify centimeter scale, orientation, normals, skeleton assignment, material slots and animation playback. UE's documented FBX import pipeline uses FBX 2020.2; exporting incompatible versions can cause problems. [Epic FBX pipeline](https://dev.epicgames.com/documentation/unreal-engine/fbx-content-pipeline)
4. Create lightweight shared material instances. Use a capsule/sphere for enemy gameplay collision, with collision disabled on both visual components. Avoid per-poly collision and unnecessary simulated bodies on crowds. Keep physics assets only where needed.
5. Create a Data Table using `EnemyVisualRow`. Each row selects static/skeletal type, mesh soft reference, compatible animation Blueprint class, visual transform, health, speed and collision radius. Add the table and referenced assets to your cook configuration.

## Modular Blueprint structure

Create `BP_ModularEnemy : PooledEnemy` with:

```text
Root
  GameplayCollision (Sphere or Capsule)
  VisualRoot (Scene)
    StaticVisual (StaticMeshComponent; no collision)
    SkeletalVisual (SkeletalMeshComponent; no collision)

Instance-editable variables:
  EnemyDefinition (DataTableRowHandle for EnemyVisualRow)
  CachedRow (EnemyVisualRow)
  VisualReady (bool, initially false)
  VisualRequestSerial (integer)
  LoadedStatic / LoadedSkeletal / LoadedAnimClass (hard object/class references)
```

Separate editor preview from runtime loading. Construction Scripts cannot host latent async loading; keep them deterministic and do not create a new component every time construction runs.

```text
Construction Script
  Get Data Table Row(EnemyDefinition)
  If missing: clear both visuals; return
  Set VisualRoot relative transform = Row.VisualOffset
  Set GameplayCollision radius = Row.CollisionRadius
  Preview only: Resolve Soft Reference for selected visual
  If already loaded: ApplyLoadedVisual
  Else: leave a placeholder (optional Call-In-Editor Preview function may load synchronously)
```

Runtime custom event framework (the events below are Blueprint work to create, not C++ functions supplied by the plugin):

```text
PrepareVisual(NewRowHandle)
  Increment VisualRequestSerial
  VisualReady = false
  Hide BOTH visuals and disable their ticks
  Clear previous meshes and animation class; clear hard references
  Get Data Table Row -> CachedRow
  If invalid: remain hidden; report row error; return
  Capture request serial AND a copy of row/row key for this request
  Switch CachedRow.VisualType
    Static:
      Async Load Asset(CapturedRow.StaticMesh)
      On Complete: validate request and actor; retain LoadedStatic; ApplyLoadedVisual
    Skeletal:
      Async Load Asset(CapturedRow.SkeletalMesh)
      Async Load Class Asset(CapturedRow.AnimationClass), if nonempty
      Wait for both completions; validate request and actor
      Retain LoadedSkeletal and LoadedAnimClass; ApplyLoadedVisual

ApplyLoadedVisual
  Ignore callback unless captured serial equals current serial AND row key still matches
  Validate loaded asset type; on failure leave placeholder/error and keep VisualReady false
  Set VisualRoot relative transform and collision radius
  Static: Set Static Mesh; clear/hide SkeletalVisual; disable its tick
  Skeletal: Set Skeletal Mesh Asset; Set Animation Mode = Use Animation Blueprint
            Set Anim Instance Class (must match skeleton); clear/hide StaticVisual
  VisualReady = true
  Show selected component ONLY if IsPoolActive
  Enable skeletal tick ONLY if selected, ready and IsPoolActive
```

Implement per-request captured values using a small Blueprint request UObject or an equivalent C++ weak callback; do not compare against a shared variable overwritten by the next request. Cancellation/serial validation prevents a late response from replacing a newer monster definition. Keep strong references while the pool needs the assets, so streaming garbage collection does not unload them.

For this fixed-class spawner, the preferred production path is one row per enemy Blueprint subclass and preloading that row once in a loading coordinator **before spawning the pool**. All instances then share the loaded assets. `BeginPlay` resolves those preloaded references and applies visuals synchronously; `OnPoolActivated` only resets state. Disable spawner auto-start until readiness succeeds. If you support changing rows at runtime, use the asynchronous framework and keep the enemy inactive until completion; do not synchronously load on every wave. The supplied spawner does not automatically wait for Blueprint asset-load events.

`OnPoolReturned` should invalidate any pending request when appropriate, hide/stop visual effects, stop animation work, reset physics and halt movement. `OnPoolActivated` restores only the selected ready visual and resets health from the cached row. Do not let a loading callback re-enable ticks on a dormant actor. Visual component ticking is also suspended by the C++ pool base.

## Crowd optimization starting points

These are profiling starting points, not universal best settings. Measure game thread, animation, render thread and GPU costs independently with Unreal Insights, `stat unit`, `stat gpu`, and a representative crowded arena.

| Area | Initial setup for hundreds of stylized units |
| --- | --- |
| Conventional skeletal LODs | Try 100%, 50%, 25%, 12.5% triangles across four LODs; inspect silhouettes at actual camera distance. Tune screen-size thresholds, reduce unnecessary bones and skin influences at lower LODs. |
| Animation | Simple state machine; avoid per-unit IK, cloth, complex Control Rig and unnecessary curves. Evaluate update-rate optimization or Animation Budget Allocator. |
| Materials/textures | Prefer one opaque material slot and shared instances; begin with 512–1024 textures for small enemies and test close-up readability. Reduce translucent layers and particle overdraw. |
| Shadows | Start with dynamic shadows disabled on common fodder; selectively enable for important units if budget permits. |
| Collision/AI | Simple collision shapes; disable mesh collision; avoid pairwise crowd collision when design permits. Pooling does not reduce active movement/AI cost. |
| Rigid repeated art | Consider instanced static meshes for repeated non-skeletal props. For very large animated crowds, evaluate animation sharing or baked vertex animation as a separate architecture. |

Conventional skeletal mesh LODs support geometry/bone reduction; choose reductions based on the visible silhouette and animation, not triangle percentage alone. [Epic skeletal LOD guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletal-mesh-lods-in-unreal-engine)

Animation Budget Allocator requires its plugin and `SkeletalMeshComponentBudgeted` components; it does not automatically budget ordinary skeletal components. Configure a measured animation time budget and significance policy. [Epic Animation Budget Allocator](https://dev.epicgames.com/documentation/unreal-engine/animation-budget-allocator-in-unreal-engine)

Nanite is a benchmark choice for these assets, not a substitute for limiting animation, material, collision or gameplay costs. UE 5.8 documentation includes skeletal Nanite support, so the blanket claim that Nanite never supports skeletal meshes is outdated. Verify your exact engine version and target renderer/platform. Compare conventional LODs with Nanite in the same crowd scene; for tiny low-poly meshes, the winner is workload-dependent. Skeletal Nanite uses animation LODs rather than conventional geometry LODs. [Epic Nanite documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)

Run a cooked build with 100, 300 and your intended maximum active units. Test pooling repeatedly, camera zoom changes and several concurrent mutated spells; do not infer a crowd budget from an empty editor viewport.

## Character and level-up UI integration (UE 5.8)

Double-click `Play-MVP.cmd` for ordinary play; `Play-CinematicTest.cmd` remains the opt-in stress/rendering test. Both forward arguments to their adjacent PowerShell scripts and apply execution-policy bypass only to that PowerShell process.

`AMortisCharacter` is a Blueprintable subclass of the existing `AMVPPlayer` (an APawn with planar FloatingPawnMovement), not a new ACharacter. This preserves the tested collision, gem pickup, saves, spell ownership and minions. `Scripts/prepare_character.py` creates `/Game/MVP/BP_MortisCharacter` and `/Game/MVP/BP_MortisArena`, and assigns the latter to Arena. CameraBoom is 1300 cm at -60 degrees, absolute rotation, with collision testing off. The Meshy visual rotates independently of the fixed camera. Health initializes in BeginPlay from the Blueprint MaxHealth. Damage rejects invalid/nonpositive values, respects contact invulnerability and invokes OnPlayerDamagedVFX with remaining health fraction before game-over. `Health` remains the canonical current-health property used by the HUD.

GetClosestEnemy queries only the spawner's active pool; friendlies and inactive actors cannot become targets. SpellBase already uses the same registry. OnWeaponAim supplies the actual shot origin and direction: in BP_MortisCharacter rotate a weapon/muzzle scene component using Make Rot From X(Direction), rather than rotating the camera or entire pawn. Explosive impacts still route through the bounded cinematic director, or the inexpensive pulse when launched without CinematicFX/CinematicTest.

AMVPEnemy retains the pooled actor implementation instead of adding CharacterMovement and a controller to every enemy. Chase direction refreshes every 50 ms with activation-time staggering, while movement integrates every frame. TakeSpellDamage receives impact velocity; death deactivates through ReturnToPool before VFX/reward callbacks. PlayHighDetailDeathVFX receives captured world position and velocity: spawn independent world-space effects there, not attached to the now-reusable enemy. For directional sparks, normalize velocity for direction, clamp magnitude, and bind a Niagara User.ImpactVelocity parameter into the emitter's initial velocity module. Merely setting an unbound user parameter does not change the existing omnidirectional explosion asset. Do not set bCanEverTick=false on return; the pool toggles tick enabled state and stops animations, and activation restores them.

### Editable UMG layout

The native UCodexCardSelectionWidget is already playable and creates three real buttons, a dimmed Gaussian BackgroundBlur layer, and card labels. FCardDisplayData exposes title, mutation-aware description, a registered MagicSchool gameplay tag, rarity color and UTexture icon/render-target reference. UCardDataAsset now supplies RarityColor and Icon. Existing cards have no authored icons; labels render without them.

For a designed Widget Blueprint, derive WBP_CodexCardSelection from UCodexCardSelectionWidget. Use this hierarchy:

- Full-screen Overlay; first child BackgroundBlur (fill, strength 8).
- Translucent dark Border above blur, then a centered heading and HorizontalBox.
- Three Buttons named exactly CardOption_A, CardOption_B, CardOption_C, marked Is Variable.
- Inside each button: Image, school TextBlock, title TextBlock, description TextBlock, rarity Border.

Keep the widget focusable. In OnCardDisplayUpdated(Index, Data), switch on Index and assign those child widgets; use the icon as the Image brush resource (Texture2D or TextureRenderTarget2D), and collapse an absent icon. Do not replace the button's existing OnClicked binding: C++ uses AddUniqueDynamic and validates against the pending deck offer. Set CardSelectionWidgetClass on BP_MortisArena to this Widget Blueprint. Native fallback content only builds when there is no Blueprint root, so a designer layout must include all three named buttons.

InitializeSelectionScreen accepts up to three unique cards in precisely the current hand order; it does not draw new cards or trust arbitrary CardIDs. Missing choices are hidden. An exhausted deck shows recovery in button A. A successful click goes through GameMode.ChooseCard -> Deck.AddSelectedCardSpell, consumes the offer once, removes the widget, heals 12 health, and either opens the next queued level or restores game input and unpauses. The old widget rejects stale/repeated clicks. Destruction never blindly unpauses, which avoids resuming during another modal or level transition. Keyboard 1/2/3 works through focused widget handling and the controller's existing paused bindings.

### Optional post-process material blur

The working native menu uses UMG BackgroundBlur; no custom post-process material asset is installed by this change. For a cinematic art variant, create M_PP_CodexBlur with Material Domain Post Process. Use SceneTexture PostProcessInput0 and its InvSize for pixel offsets; use a 9-tap Gaussian horizontal/vertical approximation or separable user-scene-texture passes. Keep weights normalized. Expose BlurPixels and BlendAmount, and output lerp(original scene, blurred scene, BlendAmount) to Emissive. Set the blendable location after tonemapping for display-space menu blur (the equivalent scene-color-after-tonemapping option in the current editor). Never sample scene color from a UI-domain material.

Create one MID when opening the menu and add it to FollowCamera via AddOrUpdateBlendable. Start with BlurPixels=3 and BlendAmount=1; restore/remove that exact blendable on final close or teardown. Retain it between queued offers, and avoid clearing unrelated camera effects. If animating while paused, drive the UI with real-time/Slate animation rather than a paused actor timeline. Render the UMG cards above the post-process and disable the UMG BackgroundBlur when testing the material alternative to avoid double blur. Check split-screen/viewport UVs and resolution scaling before shipping.

### Meshy, Nanite and shadow limits

The current necromancer FBX is one skinned mesh, not a set of separate armor assets. The Blueprint uses that imported mesh with its existing validated idle/walk/run sequences; it does not invent or regenerate armor. For rigid armor, export separate opaque static meshes, enable Nanite on each in the Static Mesh Editor, and attach StaticMeshComponents to the appropriate skeletal sockets. A static armor piece cannot be assigned as the skeletal body. For deforming armor, use a skinned mesh with the matching skeleton and validate UE 5.8's Nanite skeletal workflow independently on your target hardware.

Use shared material instances and keep material slots low. A common atlas alone does not merge independently animated skeletal actors into a single draw call; static props already use ISMs, while crowd skinning/animation still has costs. The existing minor enemies reuse the same imported mesh/material assets. Do not generate a unique dynamic material or scene capture per enemy.

In cinematic mode the player's skeletal visual now casts shadows. The explosion director already enables CastShadows on admitted flashes, caps simultaneous shadow lights at two, and disables them after 0.2 seconds. Lumen GI and direct-light shadows are separate features. Play-MVP now enables CinematicFX with DX12/SM6, distance fields, Lumen and virtual shadows, while retaining normal enemy waves. Enabling shadows on every enemy needs new profiling and is not done here.

References: [Epic BackgroundBlur](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-background-blur-widget-in-unreal-engine), [Epic Nanite content](https://dev.epicgames.com/documentation/unreal-engine/working-with-naniteenabled-content), [Epic instanced static meshes](https://dev.epicgames.com/documentation/unreal-engine/instanced-static-mesh-component-in-unreal-engine).
