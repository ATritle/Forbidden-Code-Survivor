#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "PooledEnemy.h"
#include "SpellBase.h"
#include "MortisProjectileBase.h"
#include "ForbiddenProfile.h"
#include "MVPGame.generated.h"

class ABloodSurfaceDirector;
class AForbiddenDistrictDirector;
class AMVPEnemy;
class UForbiddenChoiceWidget;
class AForbiddenSpirit;
class UForbiddenScoreWidget;
class UNeonChallengeComponent;
class ANeonAudioDirector;
class UNeonSettingsWidget;
class UNeonTestingWidget;
class AMortisTestingDirector;
class AKineticCardVFXDirector;
class ANeonSpellImpactDirector;
class UFont;
class UTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ANeonArsenalDirector;
class UMortisTerrain;
class UCodexDeckComponent;
class UStaticMeshComponent;
class USphereComponent;
class UFloatingPawnMovement;
class AEnemySpawner;
class UCardDataAsset;
class UNecromancyVisualComponent;
class AMVPArenaGameMode;
class ASkeletonSummonSpell;
class ASkeletonMinion;
class ACinematicExplosionDirector;
class USpringArmComponent;
class UCameraComponent;
class UCodexCardSelectionWidget;
class UPointLightComponent;

UCLASS()
class DECKBUILDER_API AMVPPlayer : public APawn
{
    GENERATED_BODY()
public:
    AMVPPlayer();
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    void MoveForward(float Value);
    void MoveRight(float Value);
    void ZoomCamera(float Value);
    void UpdateGroundPhysics(float Dt);
    void TriggerSpellCastVisual(FName Style=NAME_None);
    void InitializeNeonVFX();
    void UpdateNeonVFX(float Dt,bool bVisible);
    /** Fixed prewarmed charges; refills occur only through deck selections. */
    UFUNCTION(BlueprintCallable,Category="Talismans") int32 AddTalismans(int32 Count);
    UFUNCTION(BlueprintPure,Category="Talismans") int32 GetTalismanCount() const;
    FVector GetTalismanLocation(int32 Index) const;
    static constexpr int32 MaxTalismans=16;
    void ClearTalismans();
    UPROPERTY(BlueprintReadOnly,Category="Movement") bool bFalling=false;
    UPROPERTY(BlueprintReadOnly,Category="Movement") float VerticalSpeed=0.f;
    UPROPERTY(BlueprintReadOnly,Category="Movement") int32 LandingCount=0;
    UFUNCTION(BlueprintImplementableEvent,Category="Movement") void OnLandingImpact(float ImpactSpeed,float FallHeight);
    UFUNCTION(BlueprintCallable, Category="Camera") bool SaveCameraPreference();
    UFUNCTION(BlueprintCallable, Category="Camera") void LoadCameraPreference();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") float PreferredCameraDistance = 2100.f;
    void ReceiveContactDamage(float Amount);
    void Boost();
    void CycleTargetPriority();
    void MarkTarget();
    UPROPERTY(BlueprintReadOnly) float BoostCooldown=0.f;
    UPROPERTY(BlueprintReadOnly) float LandingCharge=0.f;
    UPROPERTY(BlueprintReadOnly) int32 TargetPriority=0;
    TWeakObjectPtr<AMVPEnemy> MarkedEnemy;
    uint32 MarkedGeneration=0;
    float LandingRewardCooldown=0.f;
    float BoostRemaining=0.f;
    float TalismanRecoveryRemaining=18.f;
    FVector BoostDirection=FVector::ZeroVector;
    /** Contract penalties deliberately bypass ordinary contact invulnerability. */
    void ReceiveChallengeDamage(float Amount,bool bPlayHitFeedback=true);
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Challenges") TObjectPtr<UNeonChallengeComponent> Challenges;
    UFUNCTION(BlueprintCallable, Category="Vitals")
    void TakeDamageAmount(float Damage) { ReceiveContactDamage(Damage); }
    UFUNCTION(BlueprintPure, Category="Combat Automation")
    AActor* GetClosestEnemy(float ScanRadius) const;
    /** Remaining health fraction; damage is already clamped and validated. */
    UFUNCTION(BlueprintImplementableEvent, Category="Vitals|VFX")
    void OnPlayerDamagedVFX(float HealthRatio);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat|VFX")
    void OnWeaponAim(FVector Origin, FVector Direction);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vitals", meta=(ClampMin="1"))
    float MaxHealth = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> FollowCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|VFX")
    TObjectPtr<USceneComponent> WeaponMuzzle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCodexDeckComponent> Deck;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UFloatingPawnMovement> Movement;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float Health = 100.f;
    bool bBenchmarkInvulnerable = false;
private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, Category="Art", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UNecromancyVisualComponent> CharacterArt;
    UPROPERTY() TObjectPtr<UNiagaraComponent> LandingDust;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> NeonTalismans;
    UPROPERTY() TObjectPtr<UNiagaraComponent> NeonWake;
    UPROPERTY() TObjectPtr<UPointLightComponent> NeonHoverLight;
    TArray<bool> TalismanCharges;
    TArray<FVector> TalismanPrevious;
    float NeonWakeRefresh=0.f;
    float NeonPhase=0.f;
    float FallStartZ=0.f;
    float LandingPulse=0.f;
    float LandingStrength=0.f;
    float LastLandingOffset=0.f;
    float InvulnerabilityRemaining = 0.f;
    bool bZoomDirty = false;
    float ZoomSaveDelay = 0.f;
    FString CameraSaveSlot() const;
};

UCLASS()
class DECKBUILDER_API AMVPEnemy : public APooledEnemy
{
    GENERATED_BODY()
public:
    AMVPEnemy();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnPoolActivated_Implementation() override;
    virtual void OnPoolReturned_Implementation() override;
    void ReceiveSpellDamage(float Amount);
    FVector GetCombatVFXLocation() const;
    void SetCombatHitFlash(FLinearColor Color,float Strength);
    UFUNCTION(BlueprintCallable, Category="Combat")
    void TakeSpellDamage(float DamageAmount, FVector ImpactVelocity, FName ProgramID=NAME_None, bool TriggerEcho=true);
    float StaggerRemaining=0.f;
    UFUNCTION(BlueprintImplementableEvent, Category="Combat|VFX")
    void PlayHighDetailDeathVFX(FVector Location, FVector ImpactVelocity);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1"))
    float MaxHealth = 28.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0"))
    float BaseMovementSpeed = 195.f;
    UPROPERTY(BlueprintReadOnly,Category="Progression") int32 EnemyTier=0;
    UPROPERTY(BlueprintReadOnly,Category="Progression") int32 EssenceReward=1;
    UPROPERTY(BlueprintReadOnly,Category="Progression") float ContactDamage=12.f;
    UFUNCTION(BlueprintCallable,Category="Progression") void ApplyEnemyTier(int32 Tier);
    UFUNCTION(BlueprintCallable,Category="Progression") void ApplyEnemyArchetype(int32 Index);
    UPROPERTY(BlueprintReadOnly,Category="Progression") int32 EnemyArchetype=INDEX_NONE;
    static int32 SelectTimedTier(float Seconds,float Roll);
    uint32 Generation = 0;
    bool bTrainingTarget=false;
    float Health = 28.f;
private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, Category="Art", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UNecromancyVisualComponent> CharacterArt;
    UPROPERTY() TObjectPtr<UMortisTerrain> Terrain;
    TWeakObjectPtr<AMVPPlayer> Target;
    FVector CachedChaseDirection = FVector::ZeroVector;
    float DirectionUpdateRemaining = 0.f;
};

/** Compatibility class used by the existing prewarmed pool. */
UCLASS()
class DECKBUILDER_API AMVPProjectile : public AMortisProjectileBase
{
    GENERATED_BODY()
};

UCLASS()
class DECKBUILDER_API AMVPImpactPulse : public AActor
{
    GENERATED_BODY()
public:
    AMVPImpactPulse();
    void Activate(FVector Position, float Radius);
    virtual void Tick(float DeltaSeconds) override;
    bool bActive = false;
private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Visual;
    float Age = 0.f;
    float TargetRadius = 120.f;
};

UCLASS()
class DECKBUILDER_API AMVPGem : public AActor
{
    GENERATED_BODY()
public:
    AMVPGem();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Activate(FVector Position, int32 Value);
    bool bActive = false;
    int32 XPValue = 1;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Aura;
    float PulseAge=0.f;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Visual;
};

UCLASS()
class DECKBUILDER_API AMVPBoltSpell : public ASpellBase
{
    GENERATED_BODY()
public:
    AMVPBoltSpell();
protected:
    virtual APooledEnemy* ResolveTarget() const override;
    virtual void FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot) override;
    bool bBlood=false;
    EProjectileType ShotType=EProjectileType::ConcentratedElemental;
    FLinearColor BoltColor = FLinearColor(1.f, 0.025f, 0.5f);
    FName AttackSound=TEXT("Pulse");
    ENeonSpellStyle SpellStyle=ENeonSpellStyle::Pulse;
};

UCLASS()
class DECKBUILDER_API AMVPEmberSpell : public AMVPBoltSpell
{
    GENERATED_BODY()
public:
    AMVPEmberSpell();
};

UCLASS()
class DECKBUILDER_API AMVPFrostSpell : public AMVPBoltSpell
{
    GENERATED_BODY()
public:
    AMVPFrostSpell();
};

UCLASS()
class DECKBUILDER_API AMVPLightningSpell : public AMVPBoltSpell
{
    GENERATED_BODY()
public:
    AMVPLightningSpell();
};
UCLASS()
class DECKBUILDER_API AMVPBloodSpell : public AMVPBoltSpell
{
    GENERATED_BODY()
public:
    AMVPBloodSpell();
};

/** A deck-owned consumable barrier, deliberately without a recharge timer. */
UCLASS()
class DECKBUILDER_API ATalismanGuardSpell : public ASpellBase
{
    GENERATED_BODY()
public:
    virtual void InitializeSpell(UCardDataAsset* Card,AEnemySpawner* Registry) override;
    virtual void StartCasting() override {} // Charge consumption is event driven, never timed.
    virtual TArray<FGameplayTag> GetAvailableMutationTags() const override;
    virtual bool ApplyMutation(FGameplayTag Tag,float Value) override;
};

UCLASS()
class DECKBUILDER_API AMVPController : public APlayerController
{
    GENERATED_BODY()
public:
    AMVPController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaSeconds) override;
    void SetMenuInput(bool bMenu);
    UFUNCTION(BlueprintCallable,Category="Settings") void ToggleSettings();
    void CloseSettings();
    UFUNCTION(BlueprintCallable,Category="Help") void OpenHelp();
    void CloseHelp();
    UFUNCTION(BlueprintCallable,Category="Menu") void CloseGame();
    UPROPERTY(Transient) TObjectPtr<class UForbiddenHelpWidget> HelpWidget;
    bool IsSettingsOpen() const { return SettingsWidget!=nullptr; }
    bool IsChallengeOpen() const;
    void InteractChallenge();
    void OpenHideout();
    void OpenLeaderboard();
    UPROPERTY(Transient) TObjectPtr<UForbiddenScoreWidget> ScoreWidget;
    UPROPERTY(Transient) TObjectPtr<UNeonSettingsWidget> SettingsWidget;
    UPROPERTY(Transient) TObjectPtr<UNeonTestingWidget> TestingWidget;
    void OpenTestingArea();
    void CloseTestingConfiguration();
    bool StartTestingSession(FName CardID,const TArray<int32>& Counts,bool bAutoReset,float GridSpacing=220.f);
    bool StartTestingCards(const TArray<FName>& Cards,const TArray<int32>& Counts,bool bAutoReset,float GridSpacing=220.f);
    void ExitTestingArea();
private:
    bool bSettingsOwnPause=false;
    bool bHelpWasPaused=false;
    void TickSettingsSmokeTest();
    void TickNeonPolishSmokeTest();
    void ChooseOne();
    void ChooseTwo();
    void ChooseThree();
    void Click();
    void RestartRun();
    void BeginSelectedRun();
    void ScrollMusicUp();
    void ScrollMusicDown();
    void TestExplosion();
    void TickSmokeTest();
    void TickCitySmokeTest();
    int32 SmokePhase = 0;
    double SmokePhaseTime = 0.0;
    FVector SmokeStartPosition;
    float SmokePausedTime = 0.f;
    bool bSmokeTest = false;
};

UCLASS()
class DECKBUILDER_API AMVPHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    void ClickAt(float X, float Y);
    void ScrollMusicPlaylist(int32 Direction);
    bool IsMusicPlaylistOpen() const {return bMusicPlaylistOpen;}
    FName GetHoveredTestingCard() const {return HoveredTestingCard;}
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Neon UI") FLinearColor InterfaceAccent=FLinearColor(0.03f,0.85f,1.f);
    UFUNCTION(BlueprintCallable,Category="Neon UI") void SetInterfaceAccent(FLinearColor Color) { InterfaceAccent=Color; }
private:
    FName HoveredTestingCard=NAME_None;
    void Text(const FString& Value, float X, float Y, float Size, FLinearColor Color);
    void CenteredText(const FString& Value, float Y, float Size, FLinearColor Color);
    void Box(float X, float Y, float W, float H, FLinearColor Color);
    void DrawCodex(AMVPArenaGameMode* Game);
    void DrawMusicPlayer(AMVPArenaGameMode* Game);
    bool ClickMusicPlayer(float X,float Y,AMVPArenaGameMode* Game);
    bool bMusicPlaylistOpen=false,bDraggingMusicVolume=false,bDraggingMusicScroll=false;
    int32 MusicScrollOffset=0;
    void DrawSettingsButton();
    void PrepareNeonInterface();
    float Hover(FName Key,float X,float Y,float W,float H);
    void Glass(FName Key,float X,float Y,float W,float H,FLinearColor Accent,float Hover=0.f,bool bSelected=false,float Mode=0.f);
    void SpellArt(FName Key,UTexture* Texture,float X,float Y,float W,float H,FLinearColor Accent,float Hover);
    void DrawHealthAlert(AMVPPlayer* Player);
    void DrawChallengeHUD(AMVPPlayer* Player);
    void NeonTitle(const FString& Value,float X,float Y,float Size,FLinearColor Color);
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Neon UI",meta=(AllowPrivateAccess="true")) TObjectPtr<UMaterialInterface> GlassMaterial;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Neon UI",meta=(AllowPrivateAccess="true")) TObjectPtr<UMaterialInterface> ArtMaterial;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Neon UI",meta=(AllowPrivateAccess="true")) TObjectPtr<UMaterialInterface> BackdropMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HealthAlertMaterial;
    UPROPERTY() TObjectPtr<UFont> NeonFont;
    UPROPERTY() TMap<FName,TObjectPtr<UMaterialInstanceDynamic>> UIInstances;
    TMap<FName,float> HoverStates;
    FVector2D Cursor=FVector2D(-100,-100);
    double LastUIFrame=0;
    float UIRealDt=0.f;
    bool bInterfacePrepared=false;
    float UIScale = 1.f;
    float OffsetX = 0.f;
    float OffsetY = 0.f;
};

/** Self-contained arena, native default content, and run progression. All state is per-world. */
UCLASS()
class DECKBUILDER_API AMVPArenaGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMVPArenaGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void EnemyDefeated(FVector Position,int32 Reward=1);
    void AddXP(int32 Amount);
    /** One extra eligible draft, without altering level or saved starting loadout. */
    void AwardChallengeReward(int32 BonusEssence);
    bool ChooseCard(int32 Index);
    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UCodexCardSelectionWidget> CardSelectionWidgetClass;
    UPROPERTY(Transient, BlueprintReadOnly, Category="UI")
    TObjectPtr<UCodexCardSelectionWidget> SelectionWidget;
    void FinishRun(bool Victory=false);
    void RecordProgramDamage(FName CardID,float Amount);
    void ContractSucceeded(FName ContractID);
    void OnArchetypeKilled(int32 Archetype,FVector Position);
    void ShowDecision(const FString& Title,const TArray<FString>& Choices,TFunction<void(int32)> Callback);
    void OpenHideout();
    void OpenProgramEditor(FName ID);
    void ChooseCorruptPerk();
    void OfferCapturedCode(int32 Archetype);
    UPROPERTY() TObjectPtr<UForbiddenChoiceWidget> DecisionWidget;
    UPROPERTY(BlueprintReadOnly) TArray<FName> CorruptPerks;
    UPROPERTY(BlueprintReadOnly) int32 PendingCorruptRewards=0;
    UPROPERTY(BlueprintReadOnly) float CapturedRoutineRemaining=0;
    UPROPERTY(BlueprintReadOnly) int32 CapturedRoutine=INDEX_NONE;
    UPROPERTY() TObjectPtr<AForbiddenSpirit> SupportSpirit;
    UPROPERTY() TObjectPtr<AForbiddenSpirit> DecoySpirit;
    float PerkDamageMultiplier() const;
    float PerkIncomingMultiplier() const;
    float CapturedRoutineTimer=0;
    float EssenceRemainder=0;
    UPROPERTY(BlueprintReadOnly) FForbiddenRunRecord RunRecord;
    UPROPERTY(BlueprintReadOnly) bool bVictory=false;
    UPROPERTY(BlueprintReadOnly) int32 CompletedContracts=0;
    UPROPERTY(BlueprintReadOnly) int32 RunSalvage=0;
    UPROPERTY(BlueprintReadOnly) FString LastDamageCause=TEXT("Contact damage");
    UPROPERTY() TObjectPtr<AForbiddenDistrictDirector> DistrictDirector;
    UPROPERTY(BlueprintReadOnly) bool bBossRequested=false;
    bool bSuppressBossSelection=false;
    /** Pre-run loadout: one or two starting spells; all unlocked spells remain level-up offers. */
    bool StartSelectedRun();
    bool ToggleStartingCard(FName ID);
    bool SaveStartingDeck();
    void LoadStartingDeck();
    TArray<UCardDataAsset*> GetLibraryCards() const;
    bool bInCodex = true;
    int32 SchoolFilter = -1;
    int32 LibraryPage = 0;
    TArray<FName> StartingCards;
    TArray<FName> DraftPool;
    FString DeckMessage;
    void EmitProjectile(FVector Origin, FVector Direction, float Damage, const FSpellModifiers& Snapshot, FLinearColor Color, EProjectileType Type=EProjectileType::PhysicalMesh, bool bBlood=false,ENeonSpellStyle Style=ENeonSpellStyle::Prism);
    void EmitImpactPulse(FVector Position, float Radius);
    void ReplenishMinions(ASkeletonSummonSpell* Spell);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<ACinematicExplosionDirector> ExplosionDirector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Audio") TObjectPtr<ANeonAudioDirector> AudioDirector;
    UPROPERTY() TObjectPtr<AKineticCardVFXDirector> CardVFXDirector;
    UPROPERTY() TObjectPtr<ANeonSpellImpactDirector> SpellImpactDirector;
    UPROPERTY() TObjectPtr<AMortisTestingDirector> TestingDirector;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Energy") TObjectPtr<ANeonArsenalDirector> ArsenalDirector;
    UPROPERTY() TObjectPtr<ABloodSurfaceDirector> BloodDirector;
    AMVPPlayer* GetArenaPlayer() const;
    /** 18, 28, 42, 60, 82... essence. Bounded arithmetic also handles debug level edits. */
    int32 GetXPRequirement() const { const int32 Rank=FMath::Clamp(Level-1,0,1000);return 18+8*Rank+2*Rank*Rank; }
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<AEnemySpawner> Spawner;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 Level = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 XP = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 Kills = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bChoosingCard = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bRunEnded = false;
    float RunSeconds = 0.f;
    uint64 DroppedProjectiles=0;
    bool bBroadcastBossSpawned=false;
    int32 PendingLevels = 0;
private:
    void OpenNextOffer();
    void BuildArena();
    void InitializeRun();
    UPROPERTY() TArray<TObjectPtr<UObject>> TierArtAssets;
    UPROPERTY() TObjectPtr<UStaticMesh> SpearMesh;
    UPROPERTY() TObjectPtr<UNiagaraSystem> FireballFX;
    UPROPERTY() TObjectPtr<UNiagaraSystem> LightningFX;
    UPROPERTY() TObjectPtr<UNiagaraSystem> BloodFX;
    UPROPERTY()
    TArray<TObjectPtr<AMVPProjectile>> Projectiles;
    UPROPERTY()
    TArray<TObjectPtr<AMVPGem>> Gems;
    UPROPERTY()
    TArray<TObjectPtr<AMVPImpactPulse>> ImpactPulses;
    UPROPERTY()
    TArray<TObjectPtr<ASkeletonMinion>> Minions;
    UPROPERTY()
    TArray<TObjectPtr<UCardDataAsset>> Cards;
    FTimerHandle InitializeTimer;
    bool bRunInitialized = false;
    FString DeckSaveSlot() const;
};
