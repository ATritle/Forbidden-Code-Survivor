#include "MVPGame.h"
#include "NeonSpellImpactDirector.h"
#include "NeonArsenal.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

/** Standalone integration test: real cooldowns, sweeps, health loss, feedback and occluded targeting. */
void RunCombatFeedbackSmoke(AMVPController* Controller)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0,Index=0;static double Since=FPlatformTime::Seconds();
    static TArray<TWeakObjectPtr<AMVPEnemy>> Targets;static TWeakObjectPtr<ASpellBase> Weapon;
    static TWeakObjectPtr<AActor> Blocker;static float BlockedHealth=0,VisibleHealth=0;
    static const FName IDs[]={TEXT("PulseDart"),TEXT("PrismBurst"),TEXT("HaloNova"),TEXT("PhotonLance"),TEXT("KineticCards"),TEXT("ArcRelay"),TEXT("GhostLink"),TEXT("NeonCataclysm"),TEXT("OrbitRazor"),TEXT("RicochetSeal"),TEXT("StrobeRail"),TEXT("EchoPetals"),TEXT("EventHorizon"),TEXT("ShatterSigil"),TEXT("SatelliteChoir"),TEXT("TalismanGuard")};
    auto* Game=Cast<AMVPArenaGameMode>(Controller->GetWorld()->GetAuthGameMode());auto* Player=Cast<AMVPPlayer>(Controller->GetPawn());
    if(!Game || !Player || !Game->Spawner || !Game->SpellImpactDirector || !Game->ArsenalDirector) return;
    auto* FX=Game->SpellImpactDirector.Get();const double Elapsed=FPlatformTime::Seconds()-Since;
    auto Next=[&](int32 Step){Phase=Step;Since=FPlatformTime::Seconds();};
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("COMBAT_FEEDBACK_FAIL phase=%d card=%s %s"),Phase,*IDs[FMath::Clamp(Index,0,15)].ToString(),Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    auto ClearShots=[&](){for(TActorIterator<AMortisProjectileBase> It(Controller->GetWorld());It;++It) It->Deactivate();Game->ArsenalDirector->Tick(2.5f);FX->Tick(2.f);};
    if(Elapsed>8) {Fail(TEXT("Timed out waiting for actual damage and feedback"));return;}
    if(Phase==0)
    {
        if(!FX->IsReady()) {Fail(TEXT("Missing authored feedback assets"));return;}
        Game->StartingCards={TEXT("PulseDart")};if(!Game->StartSelectedRun()) {Fail(TEXT("Run did not start"));return;}
        Game->Spawner->StopWaves();Player->bBenchmarkInvulnerable=true;
        for(UCardDataAsset* Card:Player->Deck->UnlockedCards)
            if(!Player->Deck->FindOwnedSpell(Card->CardID))
            {
                Player->Deck->EquippedDeck={Card};Player->Deck->DiscardHand();Player->Deck->DrawLevelUpHand();
                if(!Player->Deck->AddSelectedCardSpell(Card)) {Fail(TEXT("Deck rejected spell"));return;}
            }
        for(ASpellBase* W:Player->Deck->ActiveWeapons) W->StopCasting();Player->ClearTalismans();
        Player->SetActorLocation(FVector(0,0,55));Player->PreferredCameraDistance=2400;Player->CameraBoom->TargetArmLength=2400;
        for(int32 I=0;I<12;++I)
        {
            const FVector Location=I<6 ? FVector(230+I*140,0,55) : FVector(FMath::Cos((I-6)*PI/3)*230,FMath::Sin((I-6)*PI/3)*230,55);
            auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->SpawnEnemyFromPool(Location));if(!Enemy) {Fail(TEXT("Enemy pool exhausted"));return;}
            Enemy->ApplyEnemyArchetype(0);Enemy->Health=100000;Enemy->SetActorTickEnabled(false);Targets.Add(Enemy);
        }
        Next(1);
    }
    else if(Phase==1 && Elapsed>0.12)
    {
        ClearShots();Weapon=Player->Deck->FindOwnedSpell(IDs[Index]);if(!Weapon.IsValid()) {Fail(TEXT("Missing equipped spell"));return;}
        Weapon->Cooldown=0.18f;
        if(Index==15) {Player->AddTalismans(3);Targets[0]->SetActorLocation(Player->GetTalismanLocation(0));Targets[0]->Health=36;}
        else Weapon->StartCasting();
        Next(2);
    }
    else if(Phase==2)
    {
        const int32 Style=int32(NeonSpellPalette::ForCard(IDs[Index]));
        if(FX->ConfirmedHits[Style]>0)
        {
            Weapon->StopCasting();
            if(FX->ConfirmedDamage[Style]<=0 || FX->ImpactBursts[Style]<=0) {Fail(TEXT("Hit did not produce a GPU collision burst"));return;}
            UE_LOG(LogTemp,Display,TEXT("COMBAT_FEEDBACK_CARD %s hits=%d actual_damage=%.3f impact_bursts=%d flight_bursts=%d"),*IDs[Index].ToString(),FX->ConfirmedHits[Style],FX->ConfirmedDamage[Style],FX->ImpactBursts[Style],FX->FlightBursts[Style]);
            Next(3);
        }
    }
    else if(Phase==3 && Elapsed>0.1)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("Feedback-%s.png"),*IDs[Index].ToString()),false,false);Next(4);
    }
    else if(Phase==4 && Elapsed>0.12)
    {
        if(++Index<16) {Next(1);return;}
        ClearShots();Player->ClearTalismans();
        for(const auto& Enemy:Targets) if(Enemy.IsValid() && Enemy->IsPoolActive()) Enemy->ReturnToPool();
        Targets.Reset();
        for(FVector Location:{FVector(230,0,55),FVector(0,300,55)})
        {
            auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->SpawnEnemyFromPool(Location));if(!Enemy) {Fail(TEXT("Target pool exhausted"));return;}
            Enemy->ApplyEnemyArchetype(0);Enemy->Health=100000;Enemy->SetActorTickEnabled(false);Targets.Add(Enemy);
        }
        auto* Wall=Controller->GetWorld()->SpawnActor<AActor>();Blocker=Wall;auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(20,150,180));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();Wall->SetActorLocation(FVector(110,0,80));
        if(Game->Spawner->FindNearestEnemy(Player->GetActorLocation(),1200,true)!=Targets[1].Get()) {Fail(TEXT("Occluded target starved visible target"));return;}
        BlockedHealth=Targets[0]->Health;VisibleHealth=Targets[1]->Health;Weapon=Player->Deck->FindOwnedSpell(TEXT("PulseDart"));Weapon->StartCasting();Next(5);
    }
    else if(Phase==5 && Elapsed>0.7)
    {
        Weapon->StopCasting();
        if(Targets[0]->Health!=BlockedHealth || Targets[1]->Health>=VisibleHealth) {Fail(TEXT("Real spell timer failed visible-target fallback"));return;}
        Blocker->Destroy();
        if(FX->VisiblePulses<16) {Fail(TEXT("Missing hit pulses"));return;}
        ClearShots();
        // Deliberately exhaust Pulse's four reserved and 32 shared GPU slots.
        for(int32 I=0;I<36;++I) FX->PlayImpact(ENeonSpellStyle::Pulse,FVector(1000+(I%6)*120,1000+(I/6)*120,80));
        const int32 PriorPulses=FX->VisiblePulses,PriorDrops=FX->Dropped;auto* Enemy=Targets[1].Get();
        const float Before=Enemy->Health;Enemy->TakeSpellDamage(7,FVector::ZeroVector);
        FX->ConfirmDamage(ENeonSpellStyle::Pulse,Enemy,Before-Enemy->Health);
        if(Enemy->Health!=Before-7 || FX->VisiblePulses!=PriorPulses+1 || FX->Dropped!=PriorDrops+1) {Fail(TEXT("GPU saturation suppressed damage or the fallback pulse"));return;}
        UE_LOG(LogTemp,Display,TEXT("COMBAT_FEEDBACK_PASS: all 16 cards confirmed actual health loss and GPU bursts; visible-target fallback; %d pulses; %d dense bursts; %d coalesced; %d GPU admission drops"),FX->VisiblePulses,FX->Accepted,FX->CoveredHits,FX->Dropped);
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexAudio_Automation"),0);
        FPlatformMisc::RequestExit(false);Next(6);
    }
#endif
}
