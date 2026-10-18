#include "PooledEnemy.h"
#include "EnemySpawner.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

APooledEnemy::APooledEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void APooledEnemy::ReturnToPool()
{
    if (PoolOwner.IsValid()) PoolOwner->ReturnEnemyToPool(this);
}

void APooledEnemy::OnPoolActivated_Implementation() {}
void APooledEnemy::OnPoolReturned_Implementation() {}

void APooledEnemy::SetPoolActive(bool bActive)
{
    bPoolActive = bActive;
    SetLifeSpan(0.f);
    if (bActive)
    {
        SetActorHiddenInGame(false);
        SetActorEnableCollision(true);
        SetActorTickEnabled(bRestoreActorTick);
        for (const auto& Component : EnabledComponentTicks)
            if (Component.IsValid()) Component->SetComponentTickEnabled(true);
        OnPoolActivated();
    }
    else
    {
        // Snapshot before the hook, which may disable movement/component ticks itself.
        bRestoreActorTick = IsActorTickEnabled();
        EnabledComponentTicks.Reset();
        TInlineComponentArray<UActorComponent*> Components(this);
        for (UActorComponent* Component : Components)
            if (Component->IsComponentTickEnabled()) EnabledComponentTicks.Add(Component);
        OnPoolReturned();
        GetWorldTimerManager().ClearAllTimersForObject(this);
        for (UActorComponent* Component : Components)
        {
            GetWorldTimerManager().ClearAllTimersForObject(Component);
            Component->SetComponentTickEnabled(false);
        }
        SetActorTickEnabled(false);
        SetActorEnableCollision(false);
        SetActorHiddenInGame(true);
    }
}
