#include "MortisProgressionManager.h"
#include "CardDataAsset.h"
#include "SpellBase.h"
#include "EnemySpawner.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AMortisProgressionManager::AMortisProgressionManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}
bool AMortisProgressionManager::BeginSession(AEnemySpawner* Spawner)
{
    if (!HasAuthority() || MapState != EMortisMapState::Waiting || !IsValid(Spawner)) return false;
    EnemySpawner = Spawner;
    TargetKillCount = FMath::Max(1, TargetKillCount);
    CurrentKillCount = 0;
    RemainingSessionTime = FMath::IsFinite(SessionDuration) ? FMath::Max(0.f, SessionDuration) : 0.f;
    MapState = EMortisMapState::Combat;
    SetActorTickEnabled(RemainingSessionTime > 0.f); // zero duration means no time limit
    return true;
}
void AMortisProgressionManager::OnEnemyKilled()
{
    if (!HasAuthority() || MapState != EMortisMapState::Combat) return;
    CurrentKillCount = FMath::Min(CurrentKillCount + 1, TargetKillCount);
    if (CurrentKillCount < TargetKillCount) return;
    MapState = EMortisMapState::Cleared; // latch before callbacks; no duplicate clear/rewards
    SetActorTickEnabled(false);
    if (IsValid(EnemySpawner)) EnemySpawner->StopWaves();
    OnLevelObjectivesCleared();
}
void AMortisProgressionManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || MapState != EMortisMapState::Combat) return;
    RemainingSessionTime = FMath::Max(0.f, RemainingSessionTime - DeltaSeconds);
    if (RemainingSessionTime > 0.f) return;
    MapState = EMortisMapState::Expired;
    SetActorTickEnabled(false);
    if (IsValid(EnemySpawner)) EnemySpawner->StopWaves();
    OnSessionExpired();
}
bool AMortisProgressionManager::FindExitGround(FVector Origin, float Radius, FVector& GroundLocation) const
{
    GroundLocation = FVector::ZeroVector;
    if (MapState != EMortisMapState::Cleared || Origin.ContainsNaN() || !FMath::IsFinite(Radius) || Radius <= 0.f) return false;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) return false;
    for (int32 Attempt = 0; Attempt < 12; ++Attempt)
    {
        FNavLocation Point;
        if (!Nav->GetRandomReachablePointInRadius(Origin, Radius, Point)) continue;
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(MortisExitFloor), false, this);
        if (!GetWorld()->LineTraceSingleByObjectType(Hit, Point.Location + FVector(0,0,100),
            Point.Location - FVector(0,0,200), FCollisionObjectQueryParams(ECC_WorldStatic), Params)) continue;
        if (Hit.ImpactNormal.Z < 0.8f) continue;
        // Room for a 120cm-wide portal/player capsule; do not place inside a wall or ceiling.
        if (GetWorld()->OverlapBlockingTestByChannel(Hit.ImpactPoint + FVector(0,0,105), FQuat::Identity,
            ECC_Pawn, FCollisionShape::MakeCapsule(60.f, 95.f), Params)) continue;
        GroundLocation = Hit.ImpactPoint;
        return true;
    }
    return false;
}
bool AMortisProgressionManager::CommitRewardAndTravel(UCardDataAsset* Reward)
{
    if (!HasAuthority() || MapState != EMortisMapState::Cleared || !IsValid(Reward) || Reward->CardID.IsNone() || !Reward->SpellClass) return false;
    const FString Package = NextZone.ToSoftObjectPath().GetLongPackageName();
    if (!Package.StartsWith(TEXT("/Game/")) || !FPackageName::DoesPackageExist(Package)) return false;
    auto* Mode = GetWorld()->GetAuthGameMode();
    if (!Mode) return false;
    const FString Slot = FParse::Param(FCommandLine::Get(),TEXT("MVPSmoke")) ? TEXT("MortisProgress_Automation") : TEXT("MortisProgress");
    UMortisProgressSave* Save = nullptr;
    if (UGameplayStatics::DoesSaveGameExist(Slot,0))
    {
        Save = Cast<UMortisProgressSave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
        if (!Save || Save->Version != 1) return false; // never overwrite an unreadable profile
    }
    else Save = Cast<UMortisProgressSave>(UGameplayStatics::CreateSaveGameObject(UMortisProgressSave::StaticClass()));
    Save->UnlockedCardIDs.AddUnique(Reward->CardID);
    if (!UGameplayStatics::SaveGameToSlot(Save,Slot,0)) return false;
    Mode->bUseSeamlessTravel = true;
    MapState = EMortisMapState::Traveling;
    UGameplayStatics::SetGamePaused(this,false);
    if (GetWorld()->ServerTravel(Package + FString::Printf(TEXT("?Wave=%d"),CurrentWaveIndex + 1))) return true;
    MapState = EMortisMapState::Cleared; // reward remains owned; retry doesn't duplicate it
    return false;
}
