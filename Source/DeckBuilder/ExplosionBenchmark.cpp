#include "ExplosionBenchmark.h"
#include "CyberEnemyRoster.h"
#include "MVPGame.h"
#include "EnemySpawner.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "CinematicExplosionDirector.h"
#include "NeonSpellImpactDirector.h"
#include "NeonArsenal.h"
#include "ForbiddenCombatSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"

AExplosionBenchmark::AExplosionBenchmark()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
}
void AExplosionBenchmark::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!Game || !Game->Spawner || !Game->GetArenaPlayer()) return;
    const double Now=FPlatformTime::Seconds();
    auto* Player=Game->GetArenaPlayer();
    if(!bStarted)
    {
        if(!Game->ExplosionDirector || !Game->ExplosionDirector->HasAuthoredEffects()) { UE_LOG(LogTemp,Error,TEXT("EXPLOSION_BENCH_FAIL missing assets")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        FParse::Value(FCommandLine::Get(),TEXT("ExplosionRate="),Rate); Rate=FMath::Clamp(Rate,0.f,60.f);
        FParse::Value(FCommandLine::Get(),TEXT("ExplosionBurst="),Burst); Burst=FMath::Clamp(Burst,1,32);
        FParse::Value(FCommandLine::Get(),TEXT("BenchWarmup="),Warmup); Warmup=FMath::Max(5.f,Warmup);
        FParse::Value(FCommandLine::Get(),TEXT("BenchDuration="),Duration); Duration=FMath::Max(5.f,Duration);
        FParse::Value(FCommandLine::Get(),TEXT("BenchName="),OutputName); OutputName=FPaths::MakeValidFileName(OutputName);
        Game->StartingCards={TEXT("PulseDart")};
        if(!Game->StartSelectedRun()) return;
        Player->bBenchmarkInvulnerable=true;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchAllSpells")))
        {
            Player->Deck->MaximumActiveWeapons=0; // Explicit stress mode; ordinary runs retain six slots.
            for(UCardDataAsset* Card:Player->Deck->UnlockedCards)
                if(Card->CardID!=TEXT("TalismanGuard") && !Player->Deck->FindOwnedSpell(Card->CardID))
                {
                    Player->Deck->EquippedDeck={Card};Player->Deck->DiscardHand();Player->Deck->DrawLevelUpHand();Player->Deck->AddSelectedCardSpell(Card);
                }
            Player->Deck->EquippedDeck=Player->Deck->UnlockedCards;
            if(FParse::Param(FCommandLine::Get(),TEXT("BenchEvolved")))
                for(ASpellBase* Spell:Player->Deck->ActiveWeapons)
                {
                    const auto Rank=FGameplayTag::RequestGameplayTag(TEXT("Mutation.Program.Rank"));Spell->ApplyMutation(Rank,1);Spell->ApplyMutation(Rank,1);
                    Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(FParse::Param(FCommandLine::Get(),TEXT("BenchBranchB")) ? TEXT("Mutation.Program.EvolutionB") : TEXT("Mutation.Program.EvolutionA")),1);
                }
        }
        else for(ASpellBase* Spell:Player->Deck->ActiveWeapons) Spell->StopCasting();
        Game->Spawner->StopWaves();
        for(TActorIterator<AMVPEnemy> It(GetWorld());It;++It)
            // Keep enemies alive without losing sub-64 damage to float precision at 1e9 HP.
            if(It->IsPoolActive()) { Enemies.Add(*It); It->Health=1000000.f; }
        for(int32 I=0;I<Enemies.Num();++I)
        {
            if(FParse::Param(FCommandLine::Get(),TEXT("BenchCyberRoster")))
            {
                const int32 Archetype=I==0 ? 9 : I%10==0 ? 6+(I%3) : I%4==0 ? 4+((I/4)%2) : (I/4)%4;
                Enemies[I]->ApplyEnemyArchetype(Archetype);Enemies[I]->Health=1000000.f;
            }
            const float Angle=I*2.39996323f;
            const float Radius=260.f+470.f*FMath::Sqrt(float(I+1)/FMath::Max(1,Enemies.Num()));
            const float Height=CyberEnemies::Roster()[Enemies[I]->EnemyArchetype].CollisionRadius+21;
            Enemies[I]->SetActorLocation(FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,Height));
        }
        Start=Previous=Now; NextBlast=Now+1; bStarted=true;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchCyberRoster")))
        {
            int32 Counts[10]={};for(const auto& E:Enemies) if(E.IsValid()) ++Counts[E->EnemyArchetype];
            UE_LOG(LogTemp,Display,TEXT("CYBER_BENCH_MIX basic=%d,%d,%d,%d medium=%d,%d elite=%d,%d,%d boss=%d"),Counts[0],Counts[1],Counts[2],Counts[3],Counts[4],Counts[5],Counts[6],Counts[7],Counts[8],Counts[9]);
        }
        UE_LOG(LogTemp,Display,TEXT("EXPLOSION_BENCH_START enemies=%d rate=%.1f burst=%d warmup=%.1f sample=%.1f"),Enemies.Num(),Rate,Burst,Warmup,Duration);
        return;
    }
    const double Elapsed=Now-Start;
    // Keep the crowd spread out during the post-measurement visual captures too.
    for(int32 I=0;I<Enemies.Num();++I)
        if(Enemies[I].IsValid() && Enemies[I]->GetActorLocation().SizeSquared2D()<FMath::Square(220.f))
        {
            const float Angle=I*2.39996323f;
            const float Height=CyberEnemies::Roster()[Enemies[I]->EnemyArchetype].CollisionRadius+21;
            Enemies[I]->SetActorLocation(FVector(FMath::Cos(Angle)*730.f,FMath::Sin(Angle)*730.f,Height));
        }
    if(bFinished)
    {
        const double After=Elapsed-Warmup-Duration;
        if(CapturePhase==0 && After>4.25) { Game->ExplosionDirector->TriggerExplosion(FVector(350,0,35),450); ++CapturePhase; }
        const double CaptureTimes[]={4.5,5.1,6.5};
        if(CapturePhase>=1 && CapturePhase<=3 && After>CaptureTimes[CapturePhase-1])
        { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("%s-%d.png"),*OutputName,CapturePhase),false,false); ++CapturePhase; }
        if(After>7.5) { UE_LOG(LogTemp,Display,TEXT("EXPLOSION_BENCH_PASS %s"),*OutputName); FPlatformMisc::RequestExit(false); }
        return;
    }
    if(Rate>0 && Now>=NextBlast)
    {
        for(int32 I=0;I<Burst;++I)
        {
            const float Angle=Serial++*2.39996323f;
            Game->ExplosionDirector->TriggerExplosion(FVector(350*FMath::Cos(Angle),350*FMath::Sin(Angle),35),300);
        }
        NextBlast=Now+1.0/Rate;
    }
    if(Elapsed>=Warmup && Elapsed<Warmup+Duration)
    {
        const FStatUnitData* Unit=GetWorld()->GetGameViewport()->GetStatUnitData();
        Rows+=FString::Printf(TEXT("%.6f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,%d\n"),Elapsed,(Now-Previous)*1000,
            Unit?Unit->RawGameThreadTime:0,Unit?Unit->RawRenderThreadTime:0,Unit?Unit->RawGPUFrameTime[0]:0,
            Game->Spawner->GetActiveEnemyCount(),Game->ExplosionDirector->Accepted,Game->ExplosionDirector->Dropped,Game->ExplosionDirector->FluidAccepted);
    }
    Previous=Now;
    if(Elapsed>=Warmup+Duration)
    {
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("Benchmarks");
        IFileManager::Get().MakeDirectory(*Directory,true);
        if(!FFileHelper::SaveStringToFile(Rows,*(Directory/(OutputName+TEXT(".csv")))))
        { UE_LOG(LogTemp,Error,TEXT("EXPLOSION_BENCH_FAIL could not save CSV")); FPlatformMisc::RequestExitWithStatus(false,1); }
        bFinished=true;
        // Shipping engines omit stat/log categories. Keep admission telemetry in a normal
        // report file so performance validation never depends on development logging.
        const FString Admission=FString::Printf(TEXT("{\"result\":\"EXPLOSION_BENCH_SAMPLED\",\"enemies\":%d,\"weapons\":%d,\"projectileDrops\":%llu,\"arsenalDrops\":%d,\"fieldDrops\":%d,\"cosmeticDrops\":%d,\"warmupSeconds\":%.1f,\"sampleSeconds\":%.1f}"),Game->Spawner->GetActiveEnemyCount(),Player->Deck->ActiveWeapons.Num(),Game->DroppedProjectiles,Game->ArsenalDirector ? Game->ArsenalDirector->DroppedGameplay : 0,GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()->DroppedFields,Game->SpellImpactDirector ? Game->SpellImpactDirector->Dropped : 0,Warmup,Duration);
        FFileHelper::SaveStringToFile(Admission,*(Directory/(OutputName+TEXT(".json"))));
        UE_LOG(LogTemp,Display,TEXT("GAMEPLAY_ADMISSION projectile_drops=%llu arsenal_drops=%d fields_drops=%d"),Game->DroppedProjectiles,Game->ArsenalDirector ? Game->ArsenalDirector->DroppedGameplay : 0,GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()->DroppedFields);
        if(auto* FX=Game->SpellImpactDirector.Get())
        {
            for(int32 I=0;I<16;++I) UE_LOG(LogTemp,Display,TEXT("FEEDBACK_BENCH style=%d hits=%d damage=%.2f impact_bursts=%d flight_bursts=%d"),I,FX->ConfirmedHits[I],FX->ConfirmedDamage[I],FX->ImpactBursts[I],FX->FlightBursts[I]);
            UE_LOG(LogTemp,Display,TEXT("FEEDBACK_BENCH_TOTAL pulses=%d gpu_bursts=%d coalesced=%d gpu_admission_drops=%d"),FX->VisiblePulses,FX->Accepted,FX->CoveredHits,FX->Dropped);
        }
    }
}
