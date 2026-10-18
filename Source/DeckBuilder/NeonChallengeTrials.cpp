#include "NeonChallenge.h"
#include "NeonChallengeWidget.h"
#include "MVPGame.h"
#include "SpellBase.h"
#include "EnemySpawner.h"
#include "MortisTerrain.h"
#include "NeonSpellImpactDirector.h"
#include "NeonAudioDirector.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

float UNeonChallengeComponent::GetIncomingDamageMultiplier() const
{
    return IsGameplayRunning() && ActiveStation && ActiveStation->Definition ? FMath::Clamp(ActiveStation->Definition->IncomingDamageMultiplier,1.f,10.f) : 1.f;
}
float UNeonChallengeComponent::GetPursuerSpeedMultiplier() const
{
    return IsGameplayRunning() && bCarryingCache && ActiveStation && ActiveStation->Definition && ActiveStation->Definition->Contract==ENeonChallenge::JackpotCourier ? FMath::Clamp(ActiveStation->Definition->PursuerSpeedMultiplier,1.f,3.f) : 1.f;
}
FSpellModifiers UNeonChallengeComponent::ModifySpellSnapshot(const FSpellModifiers& Snapshot) const
{
    FSpellModifiers Result=Snapshot;
    if(IsGameplayRunning() && ActiveStation && ActiveStation->Definition)
        Result.PiercingCount=FMath::Clamp(Result.PiercingCount+ActiveStation->Definition->ExtraPiercing,0,64);
    return Result;
}
bool UNeonChallengeComponent::FindTrialGround(FVector Origin,float MinRadius,float MaxRadius,FVector& Position) const
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav || !Player.IsValid()) return false;
    FNavLocation Start;if(!Nav->ProjectPointToNavigation(Player->GetActorLocation(),Start,FVector(100,100,250))) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ContractObjectiveFloor),false,GetOwner());
    for(int32 Attempt=0;Attempt<48;++Attempt)
    {
        const float A=FMath::FRandRange(0,2*PI),R=FMath::FRandRange(MinRadius,MaxRadius);
        const FVector Candidate=Origin+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0);
        FHitResult Ground;FNavLocation OnNav;
        if(!GetWorld()->LineTraceSingleByObjectType(Ground,Candidate+FVector(0,0,1000),Candidate-FVector(0,0,1200),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
            || Ground.ImpactNormal.Z<0.85f || !Nav->ProjectPointToNavigation(Ground.ImpactPoint,OnNav,FVector(80,80,100))) continue;
        if(GetWorld()->OverlapAnyTestByObjectType(OnNav.Location+FVector(0,0,100),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(72,95),Q)) continue;
        UNavigationPath* Path=Nav->FindPathToLocationSynchronously(GetWorld(),Start.Location,OnNav.Location);
        if(!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength()<MinRadius*0.8f || Path->GetPathLength()>MaxRadius*2.5f) continue;
        Position=OnNav.Location;return true;
    }
    return false;
}
AMVPEnemy* UNeonChallengeComponent::SpawnTrialElite(FVector Position,int32 Archetype)
{
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());if(!G || !G->Spawner) return nullptr;
    const bool bBossSpawned=G->bBroadcastBossSpawned;
    auto* Elite=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Position+FVector(0,0,90)));
    G->bBroadcastBossSpawned=bBossSpawned;
    if(!Elite)
    {
        // Reassign an existing non-boss slot if the normal active cap is full.
        for(TActorIterator<AMVPEnemy> It(GetWorld());It;++It)
            if(It->IsPoolActive() && !It->bTrainingTarget && It->EnemyTier<3 && FVector::DistSquared2D(It->GetActorLocation(),Player->GetActorLocation())>FMath::Square(900.f))
            {Elite=*It;++Elite->Generation;break;}
    }
    if(!Elite) return nullptr;
    Elite->ApplyEnemyArchetype(FMath::Clamp(Archetype,6,8));Elite->SetActorLocation(Position+FVector(0,0,90));
    Elite->SetCombatHitFlash(FLinearColor::Black,0);Elite->SetActorTickEnabled(true);
    if(auto* Terrain=Elite->FindComponentByClass<UMortisTerrain>()) Terrain->ResetPath();
    return Elite;
}
bool UNeonChallengeComponent::PrepareTrial(ANeonChallengeStation* Station)
{
    if(!Station || !Station->Definition || !Player.IsValid()) return false;
    const auto Kind=Station->Definition->Contract;
    if(Kind==ENeonChallenge::JackpotCourier)
    {
        if(!FindTrialGround(Station->GetActorLocation(),1000,1900,ObjectiveLocation))
        {if(OfferWidget) OfferWidget->ShowOfferError(TEXT("No safe cache route found. Try Accept again or Decline; the sign is not consumed."));return false;}
    }
    else if(Kind==ENeonChallenge::LockdownDuel)
    {
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation Start;
        if(!Nav || !Nav->ProjectPointToNavigation(Player->GetActorLocation(),Start,FVector(100,100,200)))
        {if(OfferWidget) OfferWidget->ShowOfferError(TEXT("No navigable duel site here. Move onto the ground and retry."));return false;}
        DuelCenter=Start.Location;
        bool bSiteFound=false;
        for(int32 I=0;I<8 && !bSiteFound;++I)
            bSiteFound=FindTrialGround(DuelCenter,300,600,ObjectiveLocation) && FMath::Abs(ObjectiveLocation.Z-DuelCenter.Z)<100 && FVector::DistSquared2D(ObjectiveLocation,DuelCenter)<FMath::Square(Station->Definition->DuelRadius*0.8f);
        if(!bSiteFound)
        {if(OfferWidget) OfferWidget->ShowOfferError(TEXT("No clear elite spawn inside the ring. Try Accept again or Decline."));return false;}
        AMVPEnemy* Elite=SpawnTrialElite(ObjectiveLocation,FMath::RandRange(6,8));
        if(!Elite) {if(OfferWidget) OfferWidget->ShowOfferError(TEXT("No enemy slot available for the duel. Decline and retry after clearing enemies."));return false;}
        DuelTarget=Elite;DuelTargetGeneration=Elite->Generation;
    }
    return true;
}
void UNeonChallengeComponent::OnEnemyKilled(AMVPEnemy* Enemy,uint32 Generation)
{
    if(!IsGameplayRunning() || !IsValid(Enemy) || Enemy->bTrainingTarget || Enemy->IsPoolActive() || Enemy->Health>0 || Enemy->Generation!=Generation) return;
    const uint32* Prior=RewardedLives.Find(Enemy);if(Prior && *Prior==Generation) return;
    RewardedLives.Add(Enemy,Generation);
    if(!ActiveStation || !ActiveStation->Definition) return;
    auto* D=ActiveStation->Definition.Get();
    if(D->HealthRestoredPerKill>0)
    {
        Player->Health=FMath::Min(Player->MaxHealth,Player->Health+Player->MaxHealth*FMath::Clamp(D->HealthRestoredPerKill,0.f,1.f));KillFlash=1;
        if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector) G->SpellImpactDirector->PlayFlightTrail(ENeonSpellStyle::Barrier,Player->GetActorLocation()+FVector(0,0,45));
    }
    if(D->Contract==ENeonChallenge::LockdownDuel && DuelTarget.Get()==Enemy && DuelTargetGeneration==Generation)
    {
        const bool bPlayerInside=FVector::DistSquared2D(Player->GetActorLocation(),DuelCenter)<=FMath::Square(D->DuelRadius) && FMath::Abs(Player->GetActorLocation().Z-DuelCenter.Z)<200;
        const bool bTargetInside=FVector::DistSquared2D(Enemy->GetActorLocation(),DuelCenter)<=FMath::Square(D->DuelRadius) && FMath::Abs(Enemy->GetActorLocation().Z-DuelCenter.Z)<200;
        if(!bPlayerInside || !bTargetInside || RemainingTime<=0) {ResolveTrialFailure(TEXT("DUEL FAILED / FINISH THE ELITE INSIDE THE RING"));return;}
        const int32 Essence=D->RewardEssence;
        FinishContract(ENeonContractState::Succeeded,TEXT("DUEL WON / BONUS DRAFT + ESSENCE"));AwardTrialReward(Essence);
    }
}
void UNeonChallengeComponent::AwardTrialReward(int32 Essence)
{
    if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && !G->bRunEnded)
    {++BonusDraftsAwarded;G->AwardChallengeReward(FMath::Max(0,Essence));}
}
void UNeonChallengeComponent::ResolveTrialFailure(const FString& Message)
{
    if(!ActiveStation || !ActiveStation->Definition || !Player.IsValid()) return;
    const float Damage=Player->MaxHealth*FMath::Clamp(ActiveStation->Definition->PenaltyHealthFraction,0.f,1.f);
    FinishContract(ENeonContractState::Failed,Message);
    if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector) G->SpellImpactDirector->PlayImpact(ENeonSpellStyle::Prism,Player->GetActorLocation());
    Player->ReceiveChallengeDamage(Damage);
}
void UNeonChallengeComponent::UpdateTrial(float Dt)
{
    if(!ActiveStation || !ActiveStation->Definition || !Player.IsValid()) return;
    auto* D=ActiveStation->Definition.Get();KillFlash=FMath::Max(0.f,KillFlash-Dt*2);
    const FVector P=Player->GetActorLocation();
    switch(D->Contract)
    {
    case ENeonChallenge::DeadmanBattery:
        if(TrialVisual) TrialVisual->ShowPlayerShell(P,false,KillFlash);
        Player->ReceiveChallengeDamage(Player->MaxHealth*FMath::Clamp(D->HealthDrainPerSecond,0.f,1.f)*Dt,false);break;
    case ENeonChallenge::GlassProtocol:
        if(TrialVisual) TrialVisual->ShowPlayerShell(P,true,Elapsed);break;
    case ENeonChallenge::JackpotCourier:
    {
        if(RemainingTime<=0) break;
        if(!bCarryingCache && FVector::DistSquared2D(P,ObjectiveLocation)<FMath::Square(115.f) && FMath::Abs(P.Z-ObjectiveLocation.Z)<160 && UMortisTerrain::ClearSight(Player.Get(),ObjectiveLocation+FVector(0,0,60)))
        {bCarryingCache=true;if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->AudioDirector) G->AudioDirector->PlayUI(TEXT("UI_Select"));}
        if(GoalVisual) GoalVisual->ShowObjective(bCarryingCache ? P+FVector(-65,0,80+10*FMath::Sin(Elapsed*4)) : ObjectiveLocation,80,FLinearColor(1,0.08f,0.45f),true);
        if(bCarryingCache)
        {
            if(TrialVisual) TrialVisual->ShowObjective(ActiveStation->GetActorLocation()+FVector(-200,0,0),120,FLinearColor(0.05f,0.8f,1),false);
            if(FVector::DistSquared2D(P,ActiveStation->GetActorLocation())<FMath::Square(310.f) && FMath::Abs(P.Z-ActiveStation->GetActorLocation().Z)<160 && ActiveStation->CanInteract(Player.Get()))
            {FinishContract(ENeonContractState::Succeeded,TEXT("CACHE DELIVERED / BONUS DRAFT"));AwardTrialReward(0);}
        }
        break;
    }
    case ENeonChallenge::LockdownDuel:
    {
        if(!DuelTarget.IsValid() || DuelTarget->Generation!=DuelTargetGeneration || !DuelTarget->IsPoolActive()) {ResolveTrialFailure(TEXT("DUEL FAILED / MARKED TARGET LOST"));break;}
        const bool bInside=FVector::DistSquared2D(P,DuelCenter)<=FMath::Square(D->DuelRadius) && FMath::Abs(P.Z-DuelCenter.Z)<200;
        OutsideTime=bInside ? 0.f : OutsideTime+Dt;
        if(TrialVisual) TrialVisual->ShowObjective(DuelCenter,FMath::Max(100.f,D->DuelRadius),OutsideTime>0 ? FLinearColor(1,0.4f,0.02f) : FLinearColor(0.05f,0.8f,1),false);
        if(GoalVisual) GoalVisual->ShowObjective(DuelTarget->GetCombatVFXLocation()+FVector(0,0,95),55,FLinearColor(1,0.03f,0.4f),true);
        if(OutsideTime>FMath::Max(0.1f,D->OutsideGrace)) ResolveTrialFailure(TEXT("DUEL FAILED / LEFT THE RING"));
        break;
    }
    default: break;
    }
}
