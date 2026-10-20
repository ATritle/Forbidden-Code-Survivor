#include "EnemySpawner.h"
#include "NavigationSystem.h"
#include "PooledEnemy.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AEnemySpawner::AEnemySpawner() { PrimaryActorTick.bCanEverTick = false; }
void AEnemySpawner::BeginPlay()
{
    Super::BeginPlay();
    InitializePool();
    if (bAutoStart) StartWaves();
}

bool AEnemySpawner::InitializePool()
{
    if (bInitialized) return Pool.Num() > 0;
    if (bInitializing || bEndingPlay || !GetWorld() || !EnemyClass || PoolSize <= 0) return false;
    TGuardValue<bool> Guard(bInitializing, true);
    Pool.Reserve(PoolSize);
    AvailableEnemies.Reserve(PoolSize);
    ActiveEnemies.Reserve(PoolSize);
    for (int32 Index = 0; Index < PoolSize; ++Index)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        APooledEnemy* Enemy = GetWorld()->SpawnActor<APooledEnemy>(EnemyClass, FTransform::Identity, Params);
        if (!IsValid(Enemy)) continue;
        Enemy->PoolOwner = this;
        Enemy->SetPoolActive(false);
        if (!IsValid(Enemy)) continue;
        Enemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleEnemyDestroyed);
        Pool.Add(Enemy);
        AvailableEnemies.Add(Enemy);
    }
    bInitialized = true;
    return Pool.Num() > 0;
}

APooledEnemy* AEnemySpawner::ActivateEnemyFromPool(const FTransform& Transform)
{
    if (!bInitialized || bEndingPlay || ActiveEnemies.Num() >= FMath::Max(0, MaxActiveEnemies)) return nullptr;
    while (!AvailableEnemies.IsEmpty())
    {
        APooledEnemy* Enemy = AvailableEnemies.Pop(EAllowShrinking::No);
        if (!IsValid(Enemy)) continue;
        Enemy->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
        ActiveEnemies.Add(Enemy); // Register before Blueprint callbacks, which can return it immediately.
        Enemy->SetPoolActive(true);
        return IsValid(Enemy) && Enemy->IsPoolActive() ? Enemy : nullptr;
    }
    return nullptr;
}

bool AEnemySpawner::ReturnEnemyToPool(APooledEnemy* Enemy)
{
    if (bEndingPlay || !IsValid(Enemy) || Enemy->PoolOwner.Get() != this || !ActiveEnemies.Remove(Enemy)) return false;
    Enemy->SetPoolActive(false);
    if (IsValid(Enemy)) AvailableEnemies.Add(Enemy);
    return true;
}

void AEnemySpawner::StartWaves()
{
    if (bEndingPlay || (!bInitialized && !InitializePool())) return;
    GetWorldTimerManager().SetTimer(WaveTimer, this, &AEnemySpawner::SpawnWave, FMath::Max(0.05f, WaveInterval), true);
}
void AEnemySpawner::StopWaves() { GetWorldTimerManager().ClearTimer(WaveTimer); }
void AEnemySpawner::SetWaveInterval(float Seconds)
{
    if (!FMath::IsFinite(Seconds)) return;
    WaveInterval = FMath::Max(0.05f, Seconds);
    if (GetWorldTimerManager().IsTimerActive(WaveTimer)) StartWaves();
}

bool AEnemySpawner::GetSpawnRing(FVector& Center, float& Radius) const
{
    APlayerController* PC = IsValid(PlayerController) ? PlayerController.Get() : UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->GetPawn()) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return false;
    Center = PC->GetPawn()->GetActorLocation();
    Center.Z = ArenaZ;
    Radius = 0.f;
    // Intersect all viewport corners with the arena plane. Works for orthographic
    // and downward perspective cameras, including offsets and changing zoom.
    for (int32 Corner = 0; Corner < 4; ++Corner)
    {
        FVector Origin, Direction;
        if (!PC->DeprojectScreenPositionToWorld((Corner & 1) ? Width : 0, (Corner & 2) ? Height : 0, Origin, Direction)
            || FMath::Abs(Direction.Z) < KINDA_SMALL_NUMBER) return false;
        const double Distance = (ArenaZ - Origin.Z) / Direction.Z;
        if (Distance < 0.0) return false; // No finite ground footprint: skip this wave safely.
        Radius = FMath::Max(Radius, static_cast<float>(FVector::Dist2D(Center, Origin + Direction * Distance)));
    }
    Radius = FMath::Max(FMath::Max(0.f, MinimumSpawnRadius), Radius + FMath::Max(0.f, CameraMargin));
    return true;
}

void AEnemySpawner::SpawnWave()
{
    if (!bInitialized || bEndingPlay) return;
    FVector Center;
    float Radius;
    if (!GetSpawnRing(Center, Radius)) return;
    const int32 Count = FMath::Max(0, FMath::Min3(EnemiesPerWave, MaxActiveEnemies - FMath::Max(0,ReservedSlots) - ActiveEnemies.Num(), AvailableEnemies.Num()));
    // City walls/nav exclusions can reject candidates. Bound the work but give
    // each wave enough attempts to fill its requested count at wider zooms.
    int32 Spawned=0;
    for (int32 Attempt = 0; Attempt < Count*6 && Spawned<Count; ++Attempt)
    {
        const float Angle = FMath::FRandRange(0.f, 2.f * PI);
        const float Distance = Radius + FMath::FRandRange(0.f, FMath::Max(0.f, SpawnRingWidth));
        FVector Position = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
        if(bUseNavigation)
        {
            auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
            FNavLocation Ground;
            FHitResult Floor;
            if(FMath::Abs(Position.X)>6800.f || FMath::Abs(Position.Y)>6800.f) continue;
            if(GetWorld()->LineTraceSingleByObjectType(Floor,Position+FVector(0,0,1800),Position-FVector(0,0,1800),FCollisionObjectQueryParams(ECC_WorldStatic))) Position=Floor.ImpactPoint;
            if(!Nav || !Nav->ProjectPointToNavigation(Position,Ground,FVector(100,100,150))) continue;
            Position=Ground.Location+FVector(0,0,55);
            auto* PC=UGameplayStatics::GetPlayerController(this,0);
            FVector2D Screen;int32 W=0,H=0;if(PC) PC->GetViewportSize(W,H);
            if(PC && PC->ProjectWorldLocationToScreen(Position,Screen) && Screen.X>=-30 && Screen.X<=W+30 && Screen.Y>=-30 && Screen.Y<=H+30) continue;
        }
        if(ActivateEnemyFromPool(FTransform(FRotator::ZeroRotator, Position))) ++Spawned;
    }
}

APooledEnemy* AEnemySpawner::FindNearestEnemy(FVector Origin, float Range,bool bRequireLineOfSight) const
{
    APooledEnemy* Best = nullptr;
    double BestDistance = FMath::Square(FMath::Max(0.f, Range));
    for (APooledEnemy* Enemy : ActiveEnemies)
    {
        if (!IsValid(Enemy) || !Enemy->IsPoolActive()) continue;
        const double Distance = FVector::DistSquared(Origin, Enemy->GetActorLocation());
        if (Distance <= BestDistance)
        {
            FHitResult Wall;
            if(bRequireLineOfSight && GetWorld()->LineTraceSingleByObjectType(Wall,Origin,Enemy->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic))) continue;
            Best = Enemy; BestDistance = Distance;
        }
    }
    return Best;
}
void AEnemySpawner::SetWavesPaused(bool bPaused)
{
    if(bPaused) GetWorldTimerManager().PauseTimer(WaveTimer);else GetWorldTimerManager().UnPauseTimer(WaveTimer);
}

void AEnemySpawner::HandleEnemyDestroyed(AActor* Actor)
{
    APooledEnemy* Enemy = Cast<APooledEnemy>(Actor);
    ActiveEnemies.Remove(Enemy);
    AvailableEnemies.RemoveSwap(Enemy);
    Pool.RemoveSwap(Enemy); // Exceptional external destruction shrinks capacity; no runtime replacement.
}

void AEnemySpawner::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    StopWaves();
    for (APooledEnemy* Enemy : Pool)
    {
        if (!IsValid(Enemy)) continue;
        Enemy->OnDestroyed.RemoveDynamic(this, &AEnemySpawner::HandleEnemyDestroyed);
        Enemy->PoolOwner.Reset();
        Enemy->Destroy();
    }
    Pool.Reset(); AvailableEnemies.Reset(); ActiveEnemies.Reset();
    Super::EndPlay(Reason);
}
