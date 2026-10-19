#include "MVPGame.h"
#include "ForbiddenDistrictDirector.h"
#include "ForbiddenProfile.h"
#include "CyberEnemyRoster.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "NeonChallenge.h"
#include "MortisTestingDirector.h"
#include "NeonSpellImpactDirector.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"

void AMVPArenaGameMode::RecordProgramDamage(FName ID,float Amount)
{
    if(!bRunEnded && !ID.IsNone() && FMath::IsFinite(Amount) && Amount>0 && !(TestingDirector && TestingDirector->IsActive())) RunRecord.DamageByCard.FindOrAdd(ID)+=Amount;
}
void AMVPArenaGameMode::ContractSucceeded(FName ID)
{
    if(bRunEnded) return;
    ++CompletedContracts;RunSalvage+=12;
    ++PendingCorruptRewards;
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data) ++Profile->Data->Research.FindOrAdd(ID);
}
void AMVPArenaGameMode::OnArchetypeKilled(int32 Archetype,FVector Position)
{
    if(bRunEnded || (TestingDirector && TestingDirector->IsActive())) return;
    if(Kills%5==0) ++RunSalvage;
    const auto& P=CyberEnemies::Roster()[FMath::Clamp(Archetype,0,9)];
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data) ++Profile->Data->Research.FindOrAdd(P.ID);
    if(Archetype==9) FinishRun(true);
    else if(P.Tier>=1 && DistrictDirector && Kills%3==0) DistrictDirector->OfferCapture(Archetype,Position);
}
void AMVPPlayer::Boost()
{
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    if(G && G->bInCodex) {G->StartSelectedRun();return;}
    if(!G || G->bInCodex || G->bRunEnded || G->bChoosingCard || BoostCooldown>0 || bFalling || UGameplayStatics::IsGamePaused(this)) return;
    BoostDirection=Movement->Velocity.GetSafeNormal2D();
    if(BoostDirection.IsNearlyZero()) BoostDirection=GetActorForwardVector();
    BoostRemaining=0.22f;BoostCooldown=3.f;
    // A swept acceleration burst, not a teleport: walls and arena boundaries still stop the board.
}
void AMVPPlayer::CycleTargetPriority()
{
    TargetPriority=(TargetPriority+1)%3;
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data)
    {Profile->Data->Settings.Add("TargetPriority",float(TargetPriority));Profile->Save();}
}
void AMVPPlayer::MarkTarget()
{
    if(!Deck || !Deck->EnemySpawner) return;
    AMVPEnemy* Best=nullptr;double Distance=FMath::Square(140.f);
    if(auto* PC=Cast<APlayerController>(GetController()))
    {
        float X=0,Y=0;PC->GetMousePosition(X,Y);
        for(APooledEnemy* Actor:Deck->EnemySpawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(Actor);E && E->IsPoolActive())
        {
            FVector2D Screen;if(PC->ProjectWorldLocationToScreen(E->GetCombatVFXLocation(),Screen))
            {const double D=FVector2D::DistSquared(Screen,FVector2D(X,Y));if(D<Distance) {Distance=D;Best=E;}}
        }
    }
    if(!Best) Best=Cast<AMVPEnemy>(Deck->EnemySpawner->FindNearestEnemy(GetActorLocation(),1400,true));
    MarkedEnemy=Best;MarkedGeneration=Best ? Best->Generation : 0;TargetPriority=2;
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data)
    {Profile->Data->Settings.Add("TargetPriority",2.f);Profile->Save();}
}
