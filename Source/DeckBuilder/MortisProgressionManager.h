#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "MortisProgressionManager.generated.h"
class AEnemySpawner;
class UCardDataAsset;

UENUM(BlueprintType)
enum class EMortisMapState : uint8 { Waiting, Combat, Cleared, Traveling, Expired };

UCLASS()
class DECKBUILDER_API UMortisProgressSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) TArray<FName> UnlockedCardIDs;
};

/** Optional per-zone objective actor. Place one in a zone; GameMode forwards pooled deaths.
 * Single-player/server authoritative. Time pauses with the game and starts only on BeginSession. */
UCLASS(Blueprintable)
class DECKBUILDER_API AMortisProgressionManager : public AActor
{
    GENERATED_BODY()
public:
    AMortisProgressionManager();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objectives", meta=(ClampMin="1")) int32 CurrentWaveIndex = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objectives", meta=(ClampMin="1")) int32 TargetKillCount = 250;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objectives") int32 CurrentKillCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objectives", meta=(ClampMin="0")) float SessionDuration = 600.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objectives") float RemainingSessionTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objectives") EMortisMapState MapState = EMortisMapState::Waiting;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Travel") TSoftObjectPtr<UWorld> NextZone;
    UFUNCTION(BlueprintCallable, Category="Objectives") bool BeginSession(AEnemySpawner* Spawner);
    UFUNCTION(BlueprintCallable, Category="Objectives") void OnEnemyKilled();
    UFUNCTION(BlueprintImplementableEvent, Category="Objectives") void OnLevelObjectivesCleared();
    UFUNCTION(BlueprintImplementableEvent, Category="Objectives") void OnSessionExpired();
    /** Reachable nav point plus floor trace; returns false rather than an unsafe origin fallback. */
    UFUNCTION(BlueprintCallable, Category="Travel") bool FindExitGround(FVector Origin, float Radius, FVector& GroundLocation) const;
    /** Call only after the player accepts the reward and enters the exit. Idempotent reward IDs.
     * Returns false on missing map, invalid reward, save failure, or rejected travel. */
    UFUNCTION(BlueprintCallable, Category="Travel") bool CommitRewardAndTravel(UCardDataAsset* Reward);
private:
    UPROPERTY(Transient) TObjectPtr<AEnemySpawner> EnemySpawner;
};
