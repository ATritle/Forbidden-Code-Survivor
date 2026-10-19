#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "NeonChallenge.generated.h"

class AMVPPlayer;
class AMVPEnemy;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UNeonChallengeWidget;
struct FSpellModifiers;

/** Stable IDs shared by random offers, world markers and run-only modifiers. */
UENUM(BlueprintType)
enum class ENeonChallenge : uint8
{
    OverloadOrb, UnstableEcho, HostileBroadcast,
    DeadmanBattery, GlassProtocol, JackpotCourier, LockdownDuel
};

UENUM(BlueprintType)
enum class ENeonContractState : uint8 { Ready, Offered, Active, Succeeded, Failed, Aborted };

UCLASS(BlueprintType)
class DECKBUILDER_API UNeonChallengeDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) ENeonChallenge Contract=ENeonChallenge::OverloadOrb;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Title;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Reward;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Risk;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float Duration=45;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FLinearColor Accent=FLinearColor(1,0.025f,0.45f);
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float DamageMultiplier=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="1")) float PenaltyHealthFraction=0.5f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) int32 HitsPerEcho=3;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float EchoRadius=180;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0.1")) float EchoDelay=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0.05")) float EchoCooldown=0.5f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0")) float EchoDamageMultiplier=2;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) int32 EssenceMultiplier=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float HunterInterval=15;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1",ClampMax="8")) int32 HuntersPerWave=2;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="1")) float HealthDrainPerSecond=0;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="1")) float HealthRestoredPerKill=0;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float IncomingDamageMultiplier=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="12")) int32 ExtraPiercing=0;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float PursuerSpeedMultiplier=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="100")) float DuelRadius=900;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0.1")) float OutsideGrace=2;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0")) int32 RewardEssence=0;
};

/** Solid, lit 3D reconstruction of the approved sign, with separately animated text. */
UCLASS()
class DECKBUILDER_API ANeonChallengeStation : public AActor
{
    GENERATED_BODY()
public:
    ANeonChallengeStation();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    void Configure(UNeonChallengeDefinition* InDefinition);
    void SetContractState(ENeonContractState NewState);
    bool CanInteract(const AMVPPlayer* Player) const;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UNeonChallengeDefinition> Definition;
    UPROPERTY(BlueprintReadOnly) ENeonContractState State=ENeonContractState::Ready;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float InteractRadius=310;
    UPROPERTY() TObjectPtr<UTextRenderComponent> StatusText;
    UPROPERTY() TObjectPtr<UPointLightComponent> SignLight;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> MeshyHousing;
    UPROPERTY(BlueprintReadOnly) bool bUsingMeshyHousing=false;
private:
    void BuildSign();
    UPROPERTY() TArray<TObjectPtr<UActorComponent>> Parts;
};

/** Small prewarmed effect actor; the marker is gameplay information, independent of particle density. */
UCLASS()
class DECKBUILDER_API ANeonChallengeEffect : public AActor
{
    GENERATED_BODY()
public:
    ANeonChallengeEffect();
    virtual void BeginPlay() override;
    void ShowOrb(FVector Position,FVector PlayerPosition,float Age);
    void ShowEcho(FVector Position,float Radius,float Charge,bool bDetonated=false);
    void ShowObjective(FVector Position,float Radius,FLinearColor Color,bool bBeacon);
    void ShowPlayerShell(FVector Position,bool bGlass,float Pulse);
    void HideEffect();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Marker;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Tether;
    UPROPERTY() TObjectPtr<UPointLightComponent> Light;
private:
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShieldMaterial;
};

/** All transient run modifiers and hazards live here; saved cards never change. */
UCLASS(ClassGroup=(Gameplay),meta=(BlueprintSpawnableComponent))
class DECKBUILDER_API UNeonChallengeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UNeonChallengeComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function) override;
    UFUNCTION(BlueprintCallable) void BeginRun();
    UFUNCTION(BlueprintCallable) bool Interact();
    UFUNCTION(BlueprintCallable) bool AcceptOffer();
    UFUNCTION(BlueprintCallable) void DeclineOffer();
    UFUNCTION(BlueprintCallable) void AbortContract();
    UFUNCTION(BlueprintPure) bool IsContractActive() const {return ActiveStation!=nullptr;}
    UFUNCTION(BlueprintPure) float GetDamageMultiplier() const;
    UFUNCTION(BlueprintPure) float GetIncomingDamageMultiplier() const;
    UFUNCTION(BlueprintPure) float GetPursuerSpeedMultiplier() const;
    FSpellModifiers ModifySpellSnapshot(const FSpellModifiers& Snapshot) const;
    void OnEnemyKilled(AMVPEnemy* Enemy,uint32 Generation);
    /** Seeded, without-replacement sampling; unrelated to gameplay RNG after placement. */
    static TArray<UNeonChallengeDefinition*> DrawOffers(const TArray<TObjectPtr<UNeonChallengeDefinition>>& Library,int32 Seed,int32 Count=3);
    int32 ModifyEssenceReward(int32 BaseReward) const;
    void OnConfirmedSpellHit(FVector Position,float Damage);
    ANeonChallengeStation* GetNearbyStation() const;
    bool IsGameplayRunning() const;
    void ResolveOrbContact(AMVPEnemy* Enemy,bool bSweepConfirmed=false);
    FVector GetOrbLocation() const;
    void SetTestingSuspended(bool bSuspended);
    FString GetTrackerText() const;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Contracts") TArray<TObjectPtr<UNeonChallengeDefinition>> Definitions;
    UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<ANeonChallengeStation>> Stations;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<ANeonChallengeStation> ActiveStation;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<ANeonChallengeStation> OfferedStation;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UNeonChallengeWidget> OfferWidget;
    UPROPERTY(BlueprintReadOnly) float RemainingTime=0;
    UPROPERTY(BlueprintReadOnly) int32 HuntersSpawned=0;
    UPROPERTY(BlueprintReadOnly) int32 EchoesDetonated=0;
    UPROPERTY(BlueprintReadOnly) int32 OrbRuptures=0;
    UPROPERTY(BlueprintReadOnly) int32 HuntWarnings=0;
    UPROPERTY(BlueprintReadOnly) int32 PlacementSeed=0;
    UPROPERTY(BlueprintReadOnly) FString LastResult;
    UPROPERTY(BlueprintReadOnly) bool bCarryingCache=false;
    UPROPERTY(BlueprintReadOnly) FVector ObjectiveLocation=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector DuelCenter=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float OutsideTime=0;
    UPROPERTY(BlueprintReadOnly) int32 BonusDraftsAwarded=0;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AMVPEnemy> DuelTarget;
    UPROPERTY() uint32 DuelTargetGeneration=0;
    UPROPERTY() TObjectPtr<ANeonChallengeEffect> OrbVisual;
    static constexpr int32 EchoCapacity=12;
    int32 GetPendingEchoCount() const;
    float GetElapsed() const {return Elapsed;}
private:
    struct FEcho
    {
        float Age=-1;
        FVector Position=FVector::ZeroVector;
        float Damage=0;
        float Radius=180;
        float Delay=1;
        float PlayerPenalty=0.2f;
        bool bDetonated=false;
    };
    UPROPERTY() TArray<TObjectPtr<ANeonChallengeEffect>> EchoVisuals;
    UPROPERTY() TObjectPtr<ANeonChallengeEffect> GoalVisual;
    UPROPERTY() TObjectPtr<ANeonChallengeEffect> TrialVisual;
    UPROPERTY() TArray<TObjectPtr<UNeonChallengeDefinition>> RunOffers;
    TArray<FEcho> Echoes;
    TWeakObjectPtr<AMVPPlayer> Player;
    float Elapsed=0,HitCooldown=0,StationRetry=0,HunterRetry=0,LastResultTime=0;
    float NextHunterWave=15;
    FVector PreviousOrb=FVector::ZeroVector;
    bool bOrbPositionValid=false;
    int32 ConfirmedHitCount=0,PendingHunters=0;
    float KillFlash=0;
    int32 HealingSecond=INDEX_NONE,HealsThisSecond=0;
    TMap<TWeakObjectPtr<AMVPEnemy>,uint32> RewardedLives;
    bool bResolvingEcho=false,bOwnPause=false,bSuspended=false,bRunStarted=false;
    void CreateDefaultDefinitions();
    void TryPlaceStations();
    bool FindStationGround(FRandomStream& Random,FVector& Position) const;
    void CloseOffer();
    void FinishContract(ENeonContractState State,const FString& Message);
    void DetonateEcho(FEcho& Echo);
    void UpdateHunters(float Dt);
    bool TrySpawnHunter();
    bool PrepareTrial(ANeonChallengeStation* Station);
    bool FindTrialGround(FVector Origin,float MinRadius,float MaxRadius,FVector& Position) const;
    AMVPEnemy* SpawnTrialElite(FVector Position,int32 Archetype);
    void UpdateTrial(float Dt);
    void ResolveTrialFailure(const FString& Message);
    void AwardTrialReward(int32 Essence);
    void ClearEffects();
};
