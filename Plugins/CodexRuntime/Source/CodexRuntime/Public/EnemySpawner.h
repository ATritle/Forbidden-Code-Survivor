#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class APooledEnemy;
class APlayerController;

/** Single local player, flat XY arena. No actor allocation after InitializePool. */
UCLASS(Blueprintable)
class CODEXRUNTIME_API AEnemySpawner : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawning") bool bUseNavigation=false;
public:
    AEnemySpawner();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool")
    TSubclassOf<APooledEnemy> EnemyClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="1"))
    int32 PoolSize = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves", meta=(ClampMin="0"))
    int32 MaxActiveEnemies = 200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Waves", meta=(ClampMin="1"))
    int32 EnemiesPerWave = 10;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Waves", meta=(ClampMin="0.05"))
    float WaveInterval = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Waves")
    bool bAutoStart = true;
    /** Camera is resolved at wave time, so possession/camera changes are supported. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawning")
    TObjectPtr<APlayerController> PlayerController;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawning", meta=(ClampMin="0"))
    float MinimumSpawnRadius = 1000.f;
    /** Must exceed the enemy's projected visual footprint, including height/shadows. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawning", meta=(ClampMin="0"))
    float CameraMargin = 200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawning", meta=(ClampMin="0"))
    float SpawnRingWidth = 200.f;
    /** Actor origin height, not necessarily the floor height (e.g. capsule center). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawning")
    float ArenaZ = 0.f;

    /** Idempotent prewarm; partial spawn failures reduce capacity, never grow at runtime. */
    UFUNCTION(BlueprintCallable, Category="Pool")
    bool InitializePool();
    UFUNCTION(BlueprintCallable, Category="Pool")
    APooledEnemy* ActivateEnemyFromPool(const FTransform& Transform);
    UFUNCTION(BlueprintCallable, Category="Pool")
    APooledEnemy* SpawnEnemyFromPool(FVector Location) { return ActivateEnemyFromPool(FTransform(Location)); }
    UFUNCTION(BlueprintCallable, Category="Pool")
    bool ReturnEnemyToPool(APooledEnemy* Enemy);
    UFUNCTION(BlueprintCallable, Category="Waves")
    void StartWaves();
    UFUNCTION(BlueprintCallable, Category="Waves")
    void StopWaves();
    /** Preserve the pending wave delay while an isolated testing session runs. */
    void SetWavesPaused(bool bPaused);
    UFUNCTION(BlueprintCallable, Category="Waves")
    void SetWaveInterval(float Seconds);
    UFUNCTION(BlueprintCallable, Category="Waves")
    void SpawnWave();
    UFUNCTION(BlueprintPure, Category="Pool")
    int32 GetActiveEnemyCount() const { return ActiveEnemies.Num(); }
    UFUNCTION(BlueprintPure, Category="Pool")
    APooledEnemy* FindNearestEnemy(FVector Origin, float Range, bool bRequireLineOfSight=false) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool GetSpawnRing(FVector& Center, float& Radius) const;
    UFUNCTION()
    void HandleEnemyDestroyed(AActor* Actor);
    UPROPERTY(Transient)
    TArray<TObjectPtr<APooledEnemy>> Pool;
    UPROPERTY(Transient)
    TArray<TObjectPtr<APooledEnemy>> AvailableEnemies;
    UPROPERTY(Transient)
    TSet<TObjectPtr<APooledEnemy>> ActiveEnemies;
    FTimerHandle WaveTimer;
    bool bInitialized = false;
    bool bInitializing = false;
    bool bEndingPlay = false;
};
