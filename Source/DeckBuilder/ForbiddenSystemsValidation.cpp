#include "MVPGame.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenDistrictDirector.h"
#include "ForbiddenEnemyBrain.h"
#include "ForbiddenProfile.h"
#include "ForbiddenSpirit.h"
#include "ForbiddenScoreService.h"
#include "NeonArsenal.h"
#include "NeonSpellImpactDirector.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "EnemySpawner.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace
{
struct FForbiddenSystemsQA
{
    bool FullRun=false,SeenLane=false;
    int32 Phase=0,Waypoint=0,ProgressSecond=-1;
    double Since=FPlatformTime::Seconds(),Started=Since;
    TWeakObjectPtr<AMVPEnemy> Probe;
    TWeakObjectPtr<AActor> Wall;
    TArray<TWeakObjectPtr<AMVPEnemy>> Targets;
    TArray<TSharedPtr<FJsonValue>> Checks;
    float Prior=0;
    FString Directory;
    void Next(int32 Step) {Phase=Step;Since=FPlatformTime::Seconds();}
    void Check(const TCHAR* Name) {Checks.Add(MakeShared<FJsonValueString>(Name));UE_LOG(LogTemp,Display,TEXT("FORBIDDEN_SYSTEM_CHECK %s"),Name);}
    bool End(bool Pass,const FString& Detail,AMVPArenaGameMode* G)
    {
        IFileManager::Get().MakeDirectory(*Directory,true);auto JSON=MakeShared<FJsonObject>();
        JSON->SetStringField(TEXT("result"),Pass ? TEXT("FORBIDDEN_SYSTEMS_PASS") : TEXT("FORBIDDEN_SYSTEMS_FAIL"));JSON->SetStringField(TEXT("detail"),Detail);JSON->SetNumberField(TEXT("phase"),Phase);JSON->SetNumberField(TEXT("runSeconds"),G->RunSeconds);JSON->SetNumberField(TEXT("kills"),G->Kills);JSON->SetNumberField(TEXT("level"),G->Level);JSON->SetNumberField(TEXT("activePrograms"),G->GetArenaPlayer()->Deck->ActiveWeapons.Num());JSON->SetArrayField(TEXT("checks"),Checks);
        FString Text;FJsonSerializer::Serialize(JSON,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("report.json")));
        UE_LOG(LogTemp,Display,TEXT("%s %s"),Pass ? TEXT("FORBIDDEN_SYSTEMS_PASS") : TEXT("FORBIDDEN_SYSTEMS_FAIL"),*Detail);FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
    }
    void Clear(AMVPArenaGameMode* G)
    {
        const auto Active=G->Spawner->GetActiveEnemies().Array();for(APooledEnemy* E:Active) E->ReturnToPool();Targets.Reset();Probe.Reset();
    }
    AMVPEnemy* Target(AMVPArenaGameMode* G,FVector Position)
    {
        auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Position));if(!E) return nullptr;
        E->ApplyEnemyArchetype(0);E->Health=E->MaxHealth=1000;E->bTrainingTarget=true;E->SetActorLocation(Position);E->SetActorTickEnabled(false);Targets.Add(E);return E;
    }
    void Drive(AMVPArenaGameMode* G)
    {
        auto* P=G->GetArenaPlayer();auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(G->GetWorld());
        static const FVector Route[]={FVector(-1200,-2500,40),FVector(1200,-2500,40),FVector(1600,-1200,40),FVector(1300,500,40),FVector(0,900,40),FVector(-1400,400,40),FVector(-1700,-1200,40)};
        FVector Goal=Route[Waypoint%UE_ARRAY_COUNT(Route)];
        for(APooledEnemy* A:G->Spawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E->EnemyArchetype==9)
        {Goal=E->GetActorLocation();P->MarkedEnemy=E;P->MarkedGeneration=E->Generation;P->TargetPriority=2;break;}
        if(FVector::DistSquared2D(P->GetActorLocation(),Goal)<FMath::Square(G->bBroadcastBossSpawned ? 400.f : 180.f)) {if(!G->bBroadcastBossSpawned) ++Waypoint;return;}
        auto* Path=Nav ? Nav->FindPathToLocationSynchronously(G,P->GetActorLocation(),Goal) : nullptr;
        FVector NextPoint=Goal;
        if(Path && Path->IsValid() && Path->PathPoints.Num()>1) {NextPoint=Path->PathPoints[1];if(FVector::DistSquared2D(NextPoint,P->GetActorLocation())<FMath::Square(60.f) && Path->PathPoints.Num()>2) NextPoint=Path->PathPoints[2];}
        P->AddMovementInput((NextPoint-P->GetActorLocation()).GetSafeNormal2D());
    }
    bool Tick(AMVPController* PC)
    {
        auto* G=Cast<AMVPArenaGameMode>(PC->GetWorld()->GetAuthGameMode());auto* P=G ? G->GetArenaPlayer() : nullptr;
        if(!G || !P || !G->Spawner || !G->ArsenalDirector || P->Deck->UnlockedCards.Num()!=16) return true;
        const double Age=FPlatformTime::Seconds()-Since;
        if(!FullRun && Age>30) return End(false,TEXT("Systems phase timeout"),G);
        if(FullRun && FPlatformTime::Seconds()-Started>660) return End(false,TEXT("Full run exceeded eleven real minutes"),G);
        if(Phase==0)
        {
            auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile->Data->UnlockedPrograms=UForbiddenProfile::ProgramIDs();Profile->Data->Settings.Add("District",0);Profile->Data->Settings.Add("Board",0);
            if(!G->StartSelectedRun()) return End(false,TEXT("Run start failed"),G);P->bBenchmarkInvulnerable=true;G->SpellImpactDirector->SetImpactIntensity(1.f);
            IFileManager::Get().MakeDirectory(*Directory,true);
            if(FullRun) {Next(100);return true;}
            G->Spawner->StopWaves();P->Deck->ResetActiveSpells();Clear(G);P->SetActorLocation(FVector(0,0,55));G->Level=1000;Next(1);
        }
        else if(Phase==100)
        {
            const int32 Interval=FMath::FloorToInt(G->RunSeconds/30.f);
            if(Interval!=ProgressSecond)
            {
                ProgressSecond=Interval;const FString Progress=FString::Printf(TEXT("{\"seconds\":%.1f,\"kills\":%d,\"level\":%d,\"programs\":%d,\"activeEnemies\":%d,\"x\":%.1f,\"y\":%.1f}"),G->RunSeconds,G->Kills,G->Level,P->Deck->ActiveWeapons.Num(),G->Spawner->GetActiveEnemyCount(),P->GetActorLocation().X,P->GetActorLocation().Y);FFileHelper::SaveStringToFile(Progress,*(Directory/TEXT("progress.json")));
            }
            if(G->bRunEnded) {if(!G->bVictory || G->RunSeconds<355 || G->Kills<100) return End(false,TEXT("Full run did not reach timed boss victory through combat"),G);FScreenshotRequest::RequestScreenshot(Directory/TEXT("Victory.png"),true,false);Next(101);}
            else if(G->DecisionWidget) G->DecisionWidget->Select(0);
            else if(G->bChoosingCard)
            {
                int32 Pick=0;
                for(int32 I=0;I<P->Deck->CurrentHand.Num();++I) if((P->Deck->ActiveWeapons.Num()<6)!=bool(P->Deck->FindOwnedSpell(P->Deck->CurrentHand[I]->CardID))) {Pick=I;break;}
                G->ChooseCard(Pick);
            }
            else Drive(G);
        }
        else if(Phase==101 && Age>0.6) {Check(TEXT("Real-time automated six-minute district run, earned essence drafts, natural timed boss kill and victory"));return End(true,TEXT("Complete rendered run; invulnerable QA rider; normal enemy timing, movement, attacks, XP and card progression"),G);}
        else if(Phase==1)
        {
            auto* Barrier=G->GetWorld()->SpawnActor<AActor>();auto* Collision=NewObject<UBoxComponent>(Barrier);Barrier->AddInstanceComponent(Collision);Barrier->SetRootComponent(Collision);Collision->SetBoxExtent(FVector(10,220,180));Collision->SetCollisionObjectType(ECC_WorldStatic);Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Collision->SetCollisionResponseToAllChannels(ECR_Block);Collision->RegisterComponent();Barrier->SetActorLocation(FVector(160,0,150));
            P->Movement->Velocity=FVector(300,0,0);P->Boost();for(int32 I=0;I<5;++I) P->Tick(0.05f);
            if(P->GetActorLocation().X<90 || P->GetActorLocation().X>122 || P->BoostCooldown<2.5f) return End(false,TEXT("Hover boost bypassed the wall or did not consume cooldown"),G);
            Barrier->Destroy();P->Movement->StopMovementImmediately();P->SetActorLocation(FVector(0,0,700));Probe=Target(G,FVector(150,0,55));P->bFalling=false;P->LandingRewardCooldown=0;
            const int32 Landings=P->LandingCount;for(int32 I=0;I<120;++I) P->UpdateGroundPhysics(1.f/60.f);
            if(P->bFalling || P->LandingCount!=Landings+1 || Probe->StaggerRemaining<0.79f) return End(false,TEXT("Weighted landing failed to stagger a nearby enemy"),G);
            Probe->StaggerRemaining=0;P->SetActorLocation(FVector(0,0,700));for(int32 I=0;I<120;++I) P->UpdateGroundPhysics(1.f/60.f);
            if(Probe->StaggerRemaining>0) return End(false,TEXT("Repeated landings bypassed the anti-farming cooldown"),G);Clear(G);P->SetActorLocation(FVector(0,0,55));
            Check(TEXT("Swept boost respects solid walls and cooldown; weighted landing staggers with anti-farming limit"));
            P->Health=50;G->RunSalvage=15;P->SetActorLocation(FVector(-2150,-900,55));
            if(!G->DistrictDirector->Interact() || P->Health!=75 || G->RunSalvage!=0) return End(false,TEXT("Clinic did not consume salvage for healing"),G);
            P->SetActorLocation(FVector(-1100,500,55));if(!G->DistrictDirector->Interact() || G->DistrictDirector->FoundCaches!=1 || G->RunSalvage!=10) return End(false,TEXT("Spirit cache did not award exploration salvage"),G);
            P->SetActorLocation(FVector(0,0,55));Check(TEXT("Street clinic and spirit cache have real salvage/healing/essence effects"));
            auto* SavedProfile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();auto* Copy=DuplicateObject<UForbiddenProfileSave>(SavedProfile->Data,SavedProfile);
            const int32 Currency=SavedProfile->Data->CodeFragments;FForbiddenRunRecord Record;Record.ID=FGuid::NewGuid().ToString(EGuidFormats::Digits);Record.Kills=100;Record.Level=3;Record.Seconds=360;Record.Contracts=2;Record.Victory=true;
            if(!SavedProfile->RecordRun(Record,false) || SavedProfile->RecordRun(Record,false) || SavedProfile->Data->CodeFragments!=Currency+38) return End(false,TEXT("Profile banking was not idempotent"),G);
            auto* Reload=Cast<UForbiddenProfileSave>(UGameplayStatics::LoadGameFromSlot(SavedProfile->SlotName(),0));
            if(!Reload || Reload->History.IsEmpty() || Reload->History[0].Score!=27600 || Reload->Decks.Num()!=3) return End(false,TEXT("Profile roundtrip or shared score formula failed"),G);
            SavedProfile->Data=Copy;SavedProfile->Save();Check(TEXT("Isolated profile save roundtrip, unique run banking, deck presets and shared score formula"));
            const int32 Original=P->TargetPriority;P->CycleTargetPriority();auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();
            if(int32(Profile->Data->Settings.FindRef("TargetPriority"))!=P->TargetPriority) return End(false,TEXT("Target preference not persisted"),G);P->TargetPriority=Original;Check(TEXT("Saved targeting mode"));
            for(float X:{450.f,700.f,960.f,1600.f}) if(!Target(G,FVector(X,0,55))) return End(false,TEXT("Target pool admission failed"),G);
            FSpellModifiers Snapshot;Snapshot.ProgramID="ArcRelay";G->ArsenalDirector->FirePattern(0,P,Targets[0].Get(),22,Snapshot);
            if(Targets[0]->Health>=1000 || Targets[1]->Health>=1000 || Targets[2]->Health>=1000 || Targets[3]->Health!=1000) return End(false,TEXT("Chain reach failed to exclude disconnected target"),G);
            Check(TEXT("Chain damage respects physical jump spacing"));Clear(G);
            Probe=Target(G,FVector(500,0,55));auto* Actor=G->GetWorld()->SpawnActor<AActor>();auto* Body=NewObject<UBoxComponent>(Actor);Actor->AddInstanceComponent(Body);Actor->SetRootComponent(Body);Body->SetBoxExtent(FVector(15,280,180));Body->SetCollisionObjectType(ECC_WorldStatic);Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Body->SetCollisionResponseToAllChannels(ECR_Block);Body->RegisterComponent();Actor->SetActorLocation(FVector(1000,0,180));Wall=Actor;
            Snapshot.ProgramID="RicochetSeal";Snapshot.Evolution=1;G->ArsenalDirector->FirePattern(4,P,Probe.Get(),24,Snapshot);Prior=G->ArsenalDirector->WallBounces;Next(2);
        }
        else if(Phase==2 && Age>1.65)
        {
            if(G->ArsenalDirector->WallBounces<=Prior || Probe->Health>943.7f) return End(false,FString::Printf(TEXT("Bank shot failed: bounces=%d health=%.2f"),G->ArsenalDirector->WallBounces,Probe->Health),G);
            Wall->Destroy();Check(TEXT("One physical wall bounce, amplified return hit, finite lifetime"));Clear(G);Probe=Target(G,FVector(450,0,55));
            FSpellModifiers Snapshot;Snapshot.ProgramID="OrbitRazor";Snapshot.Evolution=1;G->ArsenalDirector->FirePattern(3,P,Probe.Get(),28,Snapshot);Next(3);
        }
        else if(Phase==3 && Age>1.5)
        {
            if(Probe->Health>952.5f) return End(false,TEXT("Recall edge did not hit on both outbound and return legs"),G);Check(TEXT("Returning blade hits once per leg with reduced return damage"));Clear(G);
            Probe=Target(G,FVector(500,0,55));Probe->bTrainingTarget=false;Probe->ApplyEnemyArchetype(9);Probe->Health=Probe->MaxHealth*0.5f;Probe->ContactDamage=0;
            auto* Brain=Probe->FindComponentByClass<UForbiddenEnemyBrain>();Brain->ApplySlow(1.f,3.f);if(Brain->SlowMultiplier<0.919f) return End(false,TEXT("Boss slow cap failed"),G);
            Brain->BossPhase=1;Brain->State=1;Brain->AttackPoint=P->GetActorLocation(); // Cross the health threshold during a previous-phase wind-up.
            Probe->SetActorTickEnabled(true);Next(4);
        }
        else if(Phase==4)
        {
            auto* Brain=Probe->FindComponentByClass<UForbiddenEnemyBrain>();
            if(Brain->State==5 && FVector::DistSquared2D(Brain->AttackPoint,Probe->GetActorLocation())>FMath::Square(50.f)) return End(false,TEXT("Boss phase transition inherited the previous attack origin"),G);
            if(Brain->State==5 && !SeenLane) FScreenshotRequest::RequestScreenshot(Directory/TEXT("BroadcastLane.png"),true,false);
            SeenLane|=Brain->State==5;
            if(Age>7)
            {
                if(!SeenLane || Brain->BossPhase!=2 || G->DistrictDirector->BroadcastPhase!=2) return End(false,TEXT("Boss sweep or district reaction failed"),G);
                Check(TEXT("Boss phase two sweeps a persistent telegraphed lane and changes district markers"));Clear(G);
                auto* Bishop=Target(G,FVector(500,0,55));Bishop->bTrainingTarget=false;Bishop->ApplyEnemyArchetype(6);Brain=Bishop->FindComponentByClass<UForbiddenEnemyBrain>();FVector SupportGoal=P->GetActorLocation();float SupportSpeed=150;Brain->Advance(3.f,P,SupportGoal,SupportSpeed);
                if(Brain->State!=1 || FVector::DistSquared(Brain->AttackPoint,Bishop->GetActorLocation())>1.f) return End(false,TEXT("Support telegraph did not identify its caster"),G);
                Brain->CancelChannel();if(Brain->State!=3) return End(false,TEXT("Bishop channel was not interruptible"),G);
                Check(TEXT("Null Bishop interruption exposes a recovery window"));Clear(G);
                P->SetActorLocation(FVector(2410,700,375));if(!G->DistrictDirector->Interact()) return End(false,TEXT("Promenade relay interaction failed"),G);G->DistrictDirector->Tick(2.6f);
                P->SetActorLocation(FVector(-3150,1600,615));if(!G->DistrictDirector->Interact()) return End(false,TEXT("Market relay interaction failed"),G);G->DistrictDirector->Tick(2.6f);
                if(G->DistrictDirector->HackedRelays!=2) return End(false,TEXT("Two relay hacks did not complete"),G);
                G->Spawner->MaxActiveEnemies=8;
                while(G->Spawner->GetActiveEnemyCount()<8) if(!Target(G,FVector(2200,2200,55))) break;
                G->RunSeconds=180;G->bBroadcastBossSpawned=false;G->DistrictDirector->Tick(1.f);
                if(!G->bBossRequested || !G->bBroadcastBossSpawned) return End(false,TEXT("Two relays did not admit early boss at three minutes"),G);
                Check(TEXT("Two navigable relay hacks trigger early boss admission even at a full active cap"));Clear(G);G->Spawner->MaxActiveEnemies=180;P->SetActorLocation(FVector(0,0,55));
                P->SetActorLocation(FVector(0,3400,755));G->DistrictDirector->Tick(20.1f);
                if(!G->DistrictDirector->WardComplete || !G->bChoosingCard || G->RunSalvage<20) return End(false,TEXT("Rooftop ward did not defend/reward a draft"),G);
                Check(TEXT("Rooftop ward spawns opposition, completes its hold and awards salvage plus a real draft"));Next(7);
            }
        }
        else if(Phase==7 && Age>0.7)
        {
            FScreenshotRequest::RequestScreenshot(Directory/TEXT("WardDraft.png"),true,false);Next(9);
        }
        else if(Phase==9 && Age>0.5)
        {
            if(!G->ChooseCard(0) || PC->IsPaused()) return End(false,TEXT("Ward draft did not resume play"),G);
            Clear(G);P->Deck->ResetActiveSpells();Probe=Target(G,P->GetActorLocation()+FVector(350,0,0));G->OfferCapturedCode(6);
            if(!G->DecisionWidget) return End(false,TEXT("Captured code choice did not open"),G);G->DecisionWidget->Select(0);Next(8);
        }
        else if(Phase==8 && Age>2.5)
        {
            if(G->CapturedRoutine!=6 || !G->SupportSpirit || G->SupportSpirit->IsHidden() || G->RunRecord.DamageByCard.FindRef("CapturedRoutine")<=0) return End(false,TEXT("Captured digital spirit did not assist with attributed damage"),G);
            Check(TEXT("Captured-code popup activates the Meshy support spirit and its damage is attributed to the stolen routine"));Clear(G);G->GetGameInstance()->GetSubsystem<UForbiddenScoreService>()->FetchBoard();PC->OpenLeaderboard();Next(5);
        }
        else if(Phase==5 && Age>2)
        {
            auto* Scores=G->GetGameInstance()->GetSubsystem<UForbiddenScoreService>();if(Scores->Loading) return true;
            if(!Scores->Status.Contains(TEXT("Windows edition"))) return End(false,TEXT("Native HTTPS leaderboard read failed: ")+Scores->Status,G);
            if(Scores->Eligible || Scores->Submit(TEXT("QAPlayer"))) return End(false,TEXT("QA run could post to public board"),G);
            FScreenshotRequest::RequestScreenshot(Directory/TEXT("WindowsLeaderboard.png"),true,false);Check(TEXT("Native Windows HTTPS board reads successfully; QA public submission refused"));Next(6);
        }
        else if(Phase==6 && Age>0.6) return End(true,TEXT("Structural card/enemy/objective/settings and live native score-read checks"),G);
        return true;
    }
};
}
void InstallForbiddenSystemsValidation(AMVPController* PC)
{
    const bool Full=FParse::Param(FCommandLine::Get(),TEXT("FullRunSmoke"));
    if(!Full && !FParse::Param(FCommandLine::Get(),TEXT("SystemsSmoke"))) return;
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))) {UE_LOG(LogTemp,Error,TEXT("Systems validation requires -ForbiddenQA"));return;}
    const auto State=MakeShared<FForbiddenSystemsQA>();State->FullRun=Full;State->Directory=FPaths::ProjectSavedDir()/(Full ? TEXT("Review/OverhaulFullRun") : TEXT("Review/OverhaulSystems"));
    const TWeakObjectPtr<AMVPController> Controller(PC);FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Controller,State](float){return Controller.IsValid() && State->Tick(Controller.Get());}));
}
