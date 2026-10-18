#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PooledEnemy.generated.h"

class AEnemySpawner;
class UActorComponent;

/** Lightweight enemy base: movement, damage, and visuals are supplied by a child Blueprint.
 * Pool hooks run on the game thread. Never Destroy() an enemy on death.
 */
UCLASS(Blueprintable)
class CODEXRUNTIME_API APooledEnemy : public AActor
{
    GENERATED_BODY()
public:
    APooledEnemy();

    UFUNCTION(BlueprintPure, Category="Pool")
    bool IsPoolActive() const { return bPoolActive; }

    UFUNCTION(BlueprintCallable, Category="Pool")
    void ReturnToPool();

    /** Reset health, velocity, targeting, animation and effects here, then start behavior. */
    UFUNCTION(BlueprintNativeEvent, Category="Pool")
    void OnPoolActivated();
    virtual void OnPoolActivated_Implementation();

    /** Stop movement, physics, audio, Niagara and external/anonymous timers here. */
    UFUNCTION(BlueprintNativeEvent, Category="Pool")
    void OnPoolReturned();
    virtual void OnPoolReturned_Implementation();

private:
    friend class AEnemySpawner;
    void SetPoolActive(bool bActive);
    TWeakObjectPtr<AEnemySpawner> PoolOwner;
    TArray<TWeakObjectPtr<UActorComponent>> EnabledComponentTicks;
    bool bRestoreActorTick = false;
    bool bPoolActive = false;
};
