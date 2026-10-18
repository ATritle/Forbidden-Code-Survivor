#include "MVPGame.h"
#include "NeonChallenge.h"
#include "NeonChallengeWidget.h"
#include "NeonSpellImpactDirector.h"
#include "MortisTestingDirector.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

/** Standalone integration checks: actual map navigation, UI, pooled damage and practice isolation. */
void RunChallengeSmoke(AMVPController* C)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0;static double Since=FPlatformTime::Seconds();
    static TWeakObjectPtr<AMVPEnemy> Victim;static float FrozenTime=0,FrozenRun=0;
    auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());auto* P=Cast<AMVPPlayer>(C->GetPawn());
    if(!G || !P || !G->Spawner || !P->Challenges) return;auto* H=P->Challenges.Get();
    const double Age=FPlatformTime::Seconds()-Since;
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("CHALLENGE_FAIL phase=%d: %s"),Phase,Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Next=[&](int32 Step){Phase=Step;Since=FPlatformTime::Seconds();};
    auto Station=[&](ENeonChallenge Kind)->ANeonChallengeStation*
    {for(ANeonChallengeStation* S:H->Stations) if(S && S->Definition->Contract==Kind) return S;return nullptr;};
    auto Approach=[&](ANeonChallengeStation* S)
    {if(!S) return false;P->SetActorLocation(S->GetActorLocation()+FVector(-230,0,55));P->VerticalSpeed=0;return H->GetNearbyStation()==S;};
    auto Click=[&](FName Name)
    {auto* B=H->OfferWidget ? Cast<UButton>(H->OfferWidget->WidgetTree->FindWidget(Name)) : nullptr;if(!B) return false;B->OnClicked.Broadcast();return true;};
    auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);};
    if(Age>35) {Fail(TEXT("Timed out"));return;}
    if(Phase==0)
    {
        if(H->IsContractActive() || !H->Stations.IsEmpty() || H->Definitions.Num()!=7) {Fail(TEXT("Contracts spawned into the initial menu, or library is incomplete"));return;}
        G->StartingCards={TEXT("PulseDart")};if(!G->StartSelectedRun()) {Fail(TEXT("Run did not start"));return;}
        G->Spawner->StopWaves();for(ASpellBase* S:P->Deck->ActiveWeapons) if(S) S->StopCasting();Next(1);
    }
    else if(Phase==1 && Age>1)
    {
        if(H->Stations.Num()!=3) {Fail(TEXT("Could not place three reachable random stations"));return;}
        for(ANeonChallengeStation* S:H->Stations)
        {
            if(!S || !S->GetActorEnableCollision() || FVector::DistSquared2D(S->GetActorLocation(),FVector::ZeroVector)<FMath::Square(1400.f) || !S->StatusText->GetMaterial(0)) {Fail(TEXT("Bad station location, collision, or neon material"));return;}
        }
        // Preserve the original three-contract regression as controlled fixtures after sampling production offers.
        for(int32 I=0;I<3;++I) H->Stations[I]->Configure(H->Definitions[I]);
        if(H->Interact()) {Fail(TEXT("Remote station accepted interaction"));return;}
        if(!Approach(Station(ENeonChallenge::OverloadOrb))) {Fail(TEXT("Sign has no clear approach or interaction line"));return;}
        Capture(TEXT("Challenge-Sign.png"));Next(2);
    }
    else if(Phase==2 && Age>0.3)
    {
        C->InteractChallenge();
        if(!H->OfferWidget || !C->IsPaused() || !C->IsChallengeOpen() || H->IsContractActive()) {Fail(TEXT("Interaction did not open a paused offer"));return;}
        FrozenRun=G->RunSeconds;Next(3);
    }
    else if(Phase==3 && Age>0.5)
    {
        if(G->RunSeconds!=FrozenRun || H->Interact()) {Fail(TEXT("Offer leaked simulation or reopened"));return;}
        Capture(TEXT("Challenge-Contract.png"));Next(4);
    }
    else if(Phase==4 && Age>0.3)
    {
        if(!Click(TEXT("DeclineChallenge")) || C->IsPaused() || H->OfferWidget || H->IsContractActive() || Station(ENeonChallenge::OverloadOrb)->State!=ENeonContractState::Ready) {Fail(TEXT("Decline consumed the sign or left the game paused"));return;}
        if(!H->Interact() || !Click(TEXT("AcceptChallenge")) || !H->IsContractActive() || C->IsPaused() || H->GetDamageMultiplier()!=3 || H->AcceptOffer()) {Fail(TEXT("Accept was not a single atomic activation"));return;}
        Victim=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(650,0,0)));
        if(!Victim.IsValid()) {Fail(TEXT("Pooled test enemy unavailable"));return;}
        Victim->SetActorTickEnabled(false);Victim->Health=1000;Victim->TakeSpellDamage(10,FVector::ZeroVector);
        if(Victim->Health!=970 || P->Deck->ActiveWeapons[0]->Damage!=30 || H->Interact()) {Fail(TEXT("Orb did not triple shared damage, changed card data, or allowed stacking"));return;}
        P->Health=100;P->ReceiveContactDamage(1);Next(5);
    }
    else if(Phase==5 && Age>1.15)
    {
        Capture(TEXT("Challenge-OverloadOrb.png"));Next(55);
    }
    else if(Phase==55 && Age>0.3)
    {
        P->ReceiveContactDamage(1);const float Before=P->Health;
        Victim->SetActorLocation(H->GetOrbLocation());H->ResolveOrbContact(Victim.Get());H->ResolveOrbContact(Victim.Get());
        if(P->Health!=Before-50 || H->OrbRuptures!=1 || H->IsContractActive() || H->GetDamageMultiplier()!=1 || Station(ENeonChallenge::OverloadOrb)->State!=ENeonContractState::Failed) {Fail(TEXT("Orb penalty was not once-only 50% max HP through contact immunity"));return;}
        Victim->ReturnToPool();
        if(!Approach(Station(ENeonChallenge::UnstableEcho)) || !H->Interact() || !Click(TEXT("AcceptChallenge"))) {Fail(TEXT("Echo contract unavailable after orb"));return;}
        Victim=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(-100,0,0)));Victim->SetActorTickEnabled(false);Victim->Health=1000;
        G->SpellImpactDirector->SetImpactIntensity(0);P->Health=100;
        for(int32 I=0;I<3;++I) Victim->TakeSpellDamage(10,FVector::ZeroVector);
        if(H->GetPendingEchoCount()!=1 || Victim->Health!=970) {Fail(TEXT("Three hits did not arm exactly one delayed echo"));return;}
        Next(6);
    }
    else if(Phase==6 && Age>0.45)
    {
        Capture(TEXT("Challenge-UnstableEcho.png"));Next(65);
    }
    else if(Phase==65 && Age>0.25)
    {
        FrozenTime=H->GetElapsed();C->ToggleSettings();
        if(!C->IsSettingsOpen() || !C->IsPaused()) {Fail(TEXT("Settings did not pause a live contract"));return;}Next(7);
    }
    else if(Phase==7 && Age>0.5)
    {
        if(H->GetElapsed()!=FrozenTime || H->EchoesDetonated!=0) {Fail(TEXT("Contract or armed blast advanced during settings"));return;}
        C->CloseSettings();Next(8);
    }
    else if(Phase==8 && Age>0.8)
    {
        if(H->EchoesDetonated!=1 || H->GetPendingEchoCount()!=0 || Victim->Health!=950 || P->Health!=80) {Fail(TEXT("Echo damage, player penalty, or recursion protection failed"));return;}
        H->TickComponent(41,LEVELTICK_All,nullptr);
        if(H->IsContractActive() || Station(ENeonChallenge::UnstableEcho)->State!=ENeonContractState::Succeeded) {Fail(TEXT("Temporary contract did not expire"));return;}
        Victim->ReturnToPool();
        if(!Approach(Station(ENeonChallenge::HostileBroadcast)) || !H->Interact() || !Click(TEXT("AcceptChallenge"))) {Fail(TEXT("Broadcast contract unavailable"));return;}
        if(H->ModifyEssenceReward(5)!=10) {Fail(TEXT("Broadcast reward was not doubled"));return;}
        // Fill the actual existing pool cap, then demand two elite replacements.
        while(G->Spawner->GetActiveEnemyCount()<G->Spawner->MaxActiveEnemies)
        {
            auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(3500,0,0)));
            if(!E) break;E->SetActorTickEnabled(false);
        }
        H->TickComponent(15.05f,LEVELTICK_All,nullptr);P->bBenchmarkInvulnerable=true;Next(9);
    }
    else if(Phase==9 && Age>1.6)
    {
        if(H->HuntersSpawned!=2 || G->Spawner->GetActiveEnemyCount()>G->Spawner->MaxActiveEnemies || H->HuntWarnings<1) {Fail(TEXT("Broadcast failed to deliver two elites within the existing cap"));return;}
        Capture(TEXT("Challenge-Broadcast.png"));Next(95);
    }
    else if(Phase==95 && Age>0.3)
    {
        FrozenTime=H->GetElapsed();C->ToggleSettings();
        TArray<int32> Counts;Counts.Init(0,10);Counts[0]=5;
        if(!C->StartTestingSession(TEXT("PulseDart"),Counts,true)) {Fail(TEXT("Could not enter testing during a contract"));return;}
        Next(10);
    }
    else if(Phase==10 && Age>1)
    {
        auto* Range=G->TestingDirector.Get();if(!Range || !Range->IsActive()) {Fail(TEXT("Practice session disappeared"));return;}
        C->ExitTestingArea();P=Cast<AMVPPlayer>(C->GetPawn());H=P->Challenges.Get();
        if(!H->IsContractActive() || FMath::Abs(H->GetElapsed()-FrozenTime)>0.05f || H->Stations[0]->IsHidden()) {Fail(TEXT("Practice did not preserve suspended challenge state"));return;}
        H->TickComponent(61,LEVELTICK_All,nullptr);
        if(H->IsContractActive() || H->ModifyEssenceReward(5)!=5 || H->GetDamageMultiplier()!=1) {Fail(TEXT("Finished contract leaked its modifiers"));return;}
        // Re-arm a consumed station only inside this opt-in harness to exercise the lethal path.
        for(TActorIterator<AMVPEnemy> It(C->GetWorld());It;++It) if(It->IsPoolActive()) It->ReturnToPool();
        Station(ENeonChallenge::OverloadOrb)->SetContractState(ENeonContractState::Ready);
        P->bBenchmarkInvulnerable=false;P->Health=40;
        if(!Approach(Station(ENeonChallenge::OverloadOrb)) || !H->Interact() || !Click(TEXT("AcceptChallenge"))) {Fail(TEXT("Could not prepare lethal rupture check"));return;}
        H->TickComponent(1.01f,LEVELTICK_All,nullptr);
        Victim=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(H->GetOrbLocation()));
        H->ResolveOrbContact(Victim.Get());
        if(P->Health!=0 || !G->bRunEnded || H->IsContractActive() || H->OfferWidget || !C->IsPaused()) {Fail(TEXT("Lethal contract penalty did not cleanly end the run"));return;}
        // A second run will perform a separate placement sample; logs expose its random seed.
        G->SpellImpactDirector->SetImpactIntensity(1);G->SpellImpactDirector->SaveVFXSettings();
        UE_LOG(LogTemp,Display,TEXT("CHALLENGE_PASS: 3 random reachable stations, offer/decline, pause, x3 damage, one-shot max-HP rupture, echo friendly hazard, elite cap, practice suspension, expiry"));
        FPlatformMisc::RequestExit(false);
    }
#endif
}
