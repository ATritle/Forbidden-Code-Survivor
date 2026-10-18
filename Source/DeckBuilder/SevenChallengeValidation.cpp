#include "MVPGame.h"
#include "NeonChallenge.h"
#include "NeonChallengeWidget.h"
#include "NeonSpellImpactDirector.h"
#include "MortisTestingDirector.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

void RunSevenChallengeSmoke(AMVPController* C)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0;static double Since=FPlatformTime::Seconds();
    static TWeakObjectPtr<AMVPPlayer> Original;static TWeakObjectPtr<AMVPEnemy> Victim;
    static float Frozen=0;static int32 BeforeDrafts=0;
    auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());auto* P=Cast<AMVPPlayer>(C->GetPawn());
    if(!G || !P || !G->Spawner || !P->Challenges) return;
    auto* H=P->Challenges.Get();const double Age=FPlatformTime::Seconds()-Since;
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("SEVEN_CHALLENGES_FAIL phase=%d: %s"),Phase,Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Next=[&](int32 Step){Phase=Step;Since=FPlatformTime::Seconds();};
    auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);};
    auto Click=[&](FName Name)
    {auto* B=H->OfferWidget ? Cast<UButton>(H->OfferWidget->WidgetTree->FindWidget(Name)) : nullptr;if(!B) return false;B->OnClicked.Broadcast();return true;};
    auto Configure=[&](ENeonChallenge Kind)
    {
        if(H->Stations.IsEmpty()) return false;auto* S=H->Stations[0].Get();
        for(UNeonChallengeDefinition* D:H->Definitions) if(D && D->Contract==Kind) {S->Configure(D);S->SetContractState(ENeonContractState::Ready);}
        P->SetActorLocation(S->GetActorLocation()+FVector(-230,0,55));P->VerticalSpeed=0;return H->Interact();
    };
    auto Accept=[&]()
    {for(int32 I=0;I<12 && H->OfferWidget && !H->IsContractActive();++I) Click(TEXT("AcceptChallenge"));return H->IsContractActive() && !C->IsPaused();};
    auto FinishDrafts=[&]()
    {
        for(int32 I=0;I<12 && G->bChoosingCard;++I) if(!G->ChooseCard(0)) return false;
        for(ASpellBase* S:P->Deck->ActiveWeapons) if(S) S->StopCasting();
        return !G->bChoosingCard && !C->IsPaused();
    };
    if(Age>35) {Fail(TEXT("Timeout"));return;}
    if(Phase==0)
    {
        if(H->Definitions.Num()!=7 || !H->Stations.IsEmpty()) {Fail(TEXT("Incomplete library or stations in menu"));return;}
        TSet<ENeonChallenge> Seen;
        for(int32 Seed=1;Seed<=64;++Seed)
        {
            const auto Offers=UNeonChallengeComponent::DrawOffers(H->Definitions,Seed*997);
            TSet<ENeonChallenge> Unique;for(auto* D:Offers) {Unique.Add(D->Contract);Seen.Add(D->Contract);}
            if(Offers.Num()!=3 || Unique.Num()!=3) {Fail(TEXT("Random offers contained duplicates or wrong count"));return;}
        }
        if(Seen.Num()!=7) {Fail(TEXT("One of the seven contracts was unreachable in random draws"));return;}
        G->StartingCards={TEXT("PulseDart"),TEXT("ArcRelay")};if(!G->StartSelectedRun()) {Fail(TEXT("Run failed"));return;}
        G->Spawner->StopWaves();for(ASpellBase* S:P->Deck->ActiveWeapons) if(S) S->StopCasting();Original=P;Next(1);
    }
    else if(Phase==1 && Age>2)
    {
        if(H->Stations.Num()!=3) {Fail(TEXT("Missing random signs"));return;}
        TSet<ENeonChallenge> Offers;
        for(ANeonChallengeStation* S:H->Stations)
        {
            Offers.Add(S->Definition->Contract);
            if(!S->bUsingMeshyHousing || !S->MeshyHousing->GetStaticMesh() || FMath::Abs(S->MeshyHousing->GetStaticMesh()->GetBounds().BoxExtent.Z*2-480)>3) {Fail(TEXT("Meshy housing absent or imported at wrong scale"));return;}
        }
        if(Offers.Num()!=3 || !Configure(ENeonChallenge::DeadmanBattery) || !Accept()) {Fail(TEXT("Battery offer failed"));return;}
        P->Health=50;P->ReceiveContactDamage(1);H->TickComponent(1,LEVELTICK_All,nullptr);
        if(!FMath::IsNearlyEqual(P->Health,47,.01f) || H->GetDamageMultiplier()!=2) {Fail(TEXT("Battery did not drain through immunity or grant double damage"));return;}
        Victim=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(-300,0,0)));Victim->SetActorTickEnabled(false);Victim->Health=10;
        Victim->TakeSpellDamage(5,FVector::ZeroVector);H->OnEnemyKilled(Victim.Get(),Victim->Generation);
        if(!FMath::IsNearlyEqual(P->Health,51,.01f)) {Fail(TEXT("Real kill healing was not exactly once"));return;}
        auto* Dummy=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(-400,0,0)));Dummy->bTrainingTarget=true;Dummy->Health=10;Dummy->TakeSpellDamage(10,FVector::ZeroVector);Dummy->bTrainingTarget=false;
        if(!FMath::IsNearlyEqual(P->Health,51,.01f)) {Fail(TEXT("Training dummy paid battery healing"));return;}
        Capture(TEXT("Challenge-DeadmanBattery.png"));Next(2);
    }
    else if(Phase==2 && Age>.3)
    {
        P->Health=100;H->TickComponent(31,LEVELTICK_All,nullptr);
        if(H->IsContractActive() || H->GetDamageMultiplier()!=1 || P->Health<=0) {Fail(TEXT("Battery drain leaked past its duration"));return;}
        if(!Configure(ENeonChallenge::GlassProtocol) || !Accept()) {Fail(TEXT("Glass offer failed"));return;}
        P->Health=100;P->ReceiveChallengeDamage(10);
        Victim=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(-300,0,0)));Victim->SetActorTickEnabled(false);Victim->Health=1000;Victim->TakeSpellDamage(40,FVector::ZeroVector);
        G->EmitProjectile(P->GetActorLocation(),FVector(0,1,0),10,FSpellModifiers(),FLinearColor(0,1,1));
        bool bPierced=false;for(TActorIterator<AMVPProjectile> It(C->GetWorld());It;++It) if(It->bActive) {bPierced=It->PiercingCount==2;It->Deactivate();}
        if(P->Health!=80 || Victim->Health!=930 || !bPierced || P->Deck->ActiveWeapons[0]->Modifiers.PiercingCount!=0 || P->Deck->ActiveWeapons[0]->Damage!=30) {Fail(TEXT("Glass damage, launch penetration or immutable spell data failed"));return;}
        Next(3);
    }
    else if(Phase==3 && Age>1)
    {
        P->ReceiveContactDamage(5);if(P->Health!=70) {Fail(TEXT("Glass did not double contact damage"));return;}
        Capture(TEXT("Challenge-GlassProtocol.png"));Next(4);
    }
    else if(Phase==4 && Age>.3)
    {
        H->TickComponent(46,LEVELTICK_All,nullptr);Victim->ReturnToPool();
        if(H->IsContractActive() || H->GetIncomingDamageMultiplier()!=1 || H->ModifySpellSnapshot(FSpellModifiers()).PiercingCount!=0) {Fail(TEXT("Glass modifiers did not clear"));return;}
        if(!Configure(ENeonChallenge::JackpotCourier)) {Fail(TEXT("Courier offer unavailable"));return;}
        Capture(TEXT("Challenge-CourierOffer.png"));Next(5);
    }
    else if(Phase==5 && Age>.3)
    {
        if(!Accept() || H->bCarryingCache || H->GetPursuerSpeedMultiplier()!=1) {Fail(TEXT("Courier acceptance or initial speed failed"));return;}
        P->SetActorLocation(H->ObjectiveLocation+FVector(0,0,55));P->VerticalSpeed=0;H->TickComponent(.01f,LEVELTICK_All,nullptr);
        if(!H->bCarryingCache || H->GetPursuerSpeedMultiplier()!=1.25f) {Fail(TEXT("Reachable cache pickup did not boost pursuers"));return;}
        Next(6);
    }
    else if(Phase==6 && Age>.4)
    {
        Capture(TEXT("Challenge-JackpotCourier.png"));Next(7);
    }
    else if(Phase==7 && Age>.3)
    {
        Frozen=H->GetElapsed();C->ToggleSettings();TArray<int32> Counts;Counts.Init(0,10);Counts[0]=5;
        if(!C->StartTestingSession(TEXT("PulseDart"),Counts,true)) {Fail(TEXT("Practice entry failed while carrying cache"));return;}Next(8);
    }
    else if(Phase==8 && Age>.6)
    {
        C->ExitTestingArea();P=Cast<AMVPPlayer>(C->GetPawn());H=P->Challenges.Get();
        if(P!=Original.Get() || !H->bCarryingCache || FMath::Abs(H->GetElapsed()-Frozen)>.06f) {Fail(TEXT("Practice corrupted courier state or advanced deadline"));return;}
        BeforeDrafts=H->BonusDraftsAwarded;auto* Station=H->ActiveStation.Get();P->SetActorLocation(Station->GetActorLocation()+FVector(-230,0,55));P->VerticalSpeed=0;
        H->TickComponent(.01f,LEVELTICK_All,nullptr);
        if(H->IsContractActive() || H->bCarryingCache || H->GetPursuerSpeedMultiplier()!=1 || H->BonusDraftsAwarded!=BeforeDrafts+1 || !G->bChoosingCard || !C->IsPaused()) {Fail(TEXT("Delivery did not clear speed and pay exactly one bonus draft"));return;}
        if(!FinishDrafts()) {Fail(TEXT("Courier reward draft did not close"));return;}
        H->TickComponent(.1f,LEVELTICK_All,nullptr);if(H->BonusDraftsAwarded!=BeforeDrafts+1) {Fail(TEXT("Delivery paid twice"));return;}
        if(!Configure(ENeonChallenge::JackpotCourier) || !Accept()) {Fail(TEXT("Courier failure fixture unavailable"));return;}
        P->Health=100;H->TickComponent(36,LEVELTICK_All,nullptr);
        if(P->Health!=65 || H->IsContractActive() || H->GetPursuerSpeedMultiplier()!=1 || H->BonusDraftsAwarded!=BeforeDrafts+1) {Fail(TEXT("Courier timeout penalty or reward suppression failed"));return;}
        if(!Configure(ENeonChallenge::LockdownDuel) || !Accept()) {Fail(TEXT("Duel could not find an elite spawn"));return;}
        if(!H->DuelTarget.IsValid() || H->DuelTarget->EnemyTier!=2 || G->Spawner->GetActiveEnemyCount()>G->Spawner->MaxActiveEnemies) {Fail(TEXT("Duel enemy was not a pooled elite"));return;}
        H->DuelTarget->SetActorTickEnabled(false);Next(9);
    }
    else if(Phase==9 && Age>.4)
    {
        Capture(TEXT("Challenge-LockdownDuel.png"));Next(10);
    }
    else if(Phase==10 && Age>.3)
    {
        BeforeDrafts=H->BonusDraftsAwarded;Victim=H->DuelTarget;
        Victim->TakeSpellDamage(100000,FVector::ZeroVector);
        if(H->IsContractActive() || H->BonusDraftsAwarded!=BeforeDrafts+1 || !G->bChoosingCard) {Fail(TEXT("Marked elite kill did not grant duel reward"));return;}
        if(!FinishDrafts()) {Fail(TEXT("Duel reward drafts did not close"));return;}
        H->OnEnemyKilled(Victim.Get(),Victim->Generation);if(H->BonusDraftsAwarded!=BeforeDrafts+1) {Fail(TEXT("Duel paid twice"));return;}
        if(!Configure(ENeonChallenge::LockdownDuel) || !Accept()) {Fail(TEXT("Duel escape fixture failed"));return;}
        P->Health=100;const FVector Center=H->DuelCenter;
        P->SetActorLocation(Center+FVector(1000,0,55));H->TickComponent(1,LEVELTICK_All,nullptr);
        if(!H->IsContractActive() || H->OutsideTime<.99f) {Fail(TEXT("Duel lacked exit grace"));return;}
        P->SetActorLocation(Center+FVector(0,0,55));H->TickComponent(.1f,LEVELTICK_All,nullptr);
        if(H->OutsideTime!=0) {Fail(TEXT("Returning to the ring did not reset grace"));return;}
        P->SetActorLocation(Center+FVector(1000,0,55));H->TickComponent(2.05f,LEVELTICK_All,nullptr);
        if(H->IsContractActive() || P->Health!=65 || H->BonusDraftsAwarded!=BeforeDrafts+1) {Fail(TEXT("Leaving duel ring did not fail once for 35% max HP"));return;}
        if(!Configure(ENeonChallenge::LockdownDuel) || !Accept()) {Fail(TEXT("Duel timeout fixture failed"));return;}
        P->Health=100;H->TickComponent(41,LEVELTICK_All,nullptr);
        if(H->IsContractActive() || P->Health!=65) {Fail(TEXT("Duel deadline did not fail"));return;}
        if(!Configure(ENeonChallenge::LockdownDuel) || !Accept()) {Fail(TEXT("Duel life-generation fixture failed"));return;}
        P->Health=100;Victim=H->DuelTarget;Victim->ReturnToPool();
        G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(-600,0,0));H->TickComponent(.01f,LEVELTICK_All,nullptr);
        if(H->IsContractActive() || H->BonusDraftsAwarded!=BeforeDrafts+1 || P->Health!=65) {Fail(TEXT("Pooled target loss or reuse granted a free duel win"));return;}
        if(!Configure(ENeonChallenge::DeadmanBattery) || !Accept()) {Fail(TEXT("Lethal battery fixture failed"));return;}
        P->Health=.5f;H->TickComponent(.5f,LEVELTICK_All,nullptr);
        if(P->Health!=0 || !G->bRunEnded || H->IsContractActive() || !C->IsPaused()) {Fail(TEXT("Battery could not kill or failed death cleanup"));return;}
        UE_LOG(LogTemp,Display,TEXT("SEVEN_CHALLENGES_PASS: random seven-card pool, Meshy sign, battery drain/kill healing, glass shared damage/penetration, courier route/delivery/timeout/practice, duel victory/grace/deadline/pool generation, death cleanup"));
        FPlatformMisc::RequestExit(false);
    }
#endif
}
