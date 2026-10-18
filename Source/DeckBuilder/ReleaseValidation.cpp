#include "MVPGame.h"
#include "NeonStreetLayout.h"
#include "NeonStreetLighting.h"
#include "NecromancyVisualComponent.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

/** Opt-in packaged verification uses real time, including the paused initial menu. */
void InstallReleaseValidation(AMVPController* Controller)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ReleaseSmoke"))) return;
    FString Directory=FPaths::ProjectSavedDir()/TEXT("ReleaseSmoke");
    FParse::Value(FCommandLine::Get(),TEXT("ReleaseSmokeDir="),Directory);
    Directory=FPaths::ConvertRelativePathToFull(Directory);
    IFileManager::Get().MakeDirectory(*Directory,true);
    const TWeakObjectPtr<AMVPController> WeakController(Controller);
    struct FReleaseState {int32 Phase=0;double Since=FPlatformTime::Seconds();};
    const TSharedRef<FReleaseState> State=MakeShared<FReleaseState>();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakController,Directory,State](float)
    {
        if(!WeakController.IsValid()) return false;
        UWorld* World=WeakController->GetWorld();
        auto* Game=Cast<AMVPArenaGameMode>(World->GetAuthGameMode());
        auto* ArenaPlayer=Game?Game->GetArenaPlayer():nullptr;
        auto* Art=ArenaPlayer?ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>():nullptr;
        const double Now=FPlatformTime::Seconds(),Age=Now-State->Since;
        auto Advance=[&](){++State->Phase;State->Since=Now;};
        if(State->Phase==0)
        {
            if(ArenaPlayer && Art && Art->IsArtReady() && ArenaPlayer->Deck->UnlockedCards.Num()==16) Advance();
        }
        else if(State->Phase==1 && Age>3)
        {FScreenshotRequest::RequestScreenshot(Directory/TEXT("ForbiddenCode-Windows-Menu.png"),true,false);Advance();}
        else if(State->Phase==2 && Age>1)
        {Game->StartingCards={TEXT("PrismBurst"),TEXT("KineticCards")};ArenaPlayer->bBenchmarkInvulnerable=true;Game->StartSelectedRun();Advance();}
        else if(State->Phase==3 && Age>6)
        {FScreenshotRequest::RequestScreenshot(Directory/TEXT("ForbiddenCode-Windows-Gameplay.png"),true,false);Advance();}
        else if(State->Phase==4 && Age>1.5)
        {
        ANeonStreetLayout* Layout=nullptr;for(TActorIterator<ANeonStreetLayout> It(World);It;++It){Layout=*It;break;}
        ANeonStreetLighting* Lights=nullptr;for(TActorIterator<ANeonStreetLighting> It(World);It;++It){Lights=*It;break;}
        const bool bMenu=IFileManager::Get().FileSize(*(Directory/TEXT("ForbiddenCode-Windows-Menu.png")))>10000;
        const bool bCombat=IFileManager::Get().FileSize(*(Directory/TEXT("ForbiddenCode-Windows-Gameplay.png")))>10000;
        const bool bReady=ArenaPlayer && Art && Art->IsArtReady() && ArenaPlayer->Deck->UnlockedCards.Num()==16 && ArenaPlayer->Deck->ActiveWeapons.Num()==2;
        const bool bPass=bMenu && bCombat && bReady && Game && !Game->bInCodex && Layout && Lights && Lights->GetFixtureCount()==27 && Game->Spawner->GetActiveEnemyCount()>0;
        const FString Report=FString::Printf(TEXT("{\"result\":\"%s\",\"menuScreenshot\":%s,\"gameplayScreenshot\":%s,\"artAndDeckReady\":%s,\"sidewalks\":%d,\"curbs\":%d,\"lamps\":%d,\"activeEnemies\":%d}"),bPass?TEXT("PACKAGED_RELEASE_PASS"):TEXT("PACKAGED_RELEASE_FAIL"),bMenu?TEXT("true"):TEXT("false"),bCombat?TEXT("true"):TEXT("false"),bReady?TEXT("true"):TEXT("false"),Layout?Layout->GetSidewalkCount():0,Layout?Layout->GetCurbCount():0,Lights?Lights->GetFixtureCount():0,Game&&Game->Spawner?Game->Spawner->GetActiveEnemyCount():0);
        FFileHelper::SaveStringToFile(Report,*(Directory/TEXT("report.json")));
        FPlatformMisc::RequestExitWithStatus(false,bPass?0:1);return false;
        }
        if(Age>60){FFileHelper::SaveStringToFile(TEXT("{\"result\":\"PACKAGED_RELEASE_TIMEOUT\"}"),*(Directory/TEXT("report.json")));FPlatformMisc::RequestExitWithStatus(false,1);return false;}
        return true;
    }));
}
