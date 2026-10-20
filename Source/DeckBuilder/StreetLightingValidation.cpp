#include "MVPGame.h"
#include "NeonStreetLighting.h"
#include "NeonChallenge.h"
#include "Components/LightComponent.h"
#include "EnemySpawner.h"
#include "CodexDeckComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

/** Fixed-camera comparisons plus budget and practice-range isolation checks. */
void RunStreetLightingSmoke(AMVPController* Controller)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0;
    static double Since=FPlatformTime::Seconds();
    AMVPArenaGameMode* Game=Cast<AMVPArenaGameMode>(Controller->GetWorld()->GetAuthGameMode());
    AMVPPlayer* ArenaPlayer=Cast<AMVPPlayer>(Controller->GetPawn());
    if(!Game || !ArenaPlayer || !Game->Spawner) return;
    ANeonStreetLighting* Street=nullptr;
    for(TActorIterator<ANeonStreetLighting> It(Controller->GetWorld());It;++It) {Street=*It;break;}
    const bool bBefore=FParse::Param(FCommandLine::Get(),TEXT("StreetLightingBefore"));
    auto Fail=[](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("STREET_LIGHTING_FAIL: %s"),Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Next=[&](int32 Step){Phase=Step;Since=FPlatformTime::Seconds();};
    auto Capture=[&](const TCHAR* Area)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("StreetLighting-%s-%s.png"),Area,bBefore?TEXT("Before"):TEXT("After")),true,false);
    };
    const double Age=FPlatformTime::Seconds()-Since;
    if(Age>30) {Fail(TEXT("Capture timed out"));return;}
    if(Phase==0)
    {
        if((!Street && !bBefore) || (Street && bBefore)) {Fail(TEXT("Comparison setup did not match command line"));return;}
        Game->StartingCards={TEXT("PrismBurst"),TEXT("PulseDart")};
        if(!Game->StartSelectedRun()) {Fail(TEXT("Cannot enter run"));return;}
        // Random challenge locations must not change the comparison's scenery/light sources.
        if(ArenaPlayer->Challenges)
        {
            ArenaPlayer->Challenges->SetTestingSuspended(true);
            for(ANeonChallengeStation* Station:ArenaPlayer->Challenges->Stations)
                if(Station)
                {
                    TInlineComponentArray<ULightComponent*> SignLights(Station);
                    for(ULightComponent* Light:SignLights) Light->SetVisibility(false);
                }
        }
        Game->Spawner->StopWaves();ArenaPlayer->bBenchmarkInvulnerable=true;
        for(ASpellBase* Spell:ArenaPlayer->Deck->ActiveWeapons) if(Spell) Spell->StopCasting();
        if(Controller->GetHUD()) Controller->GetHUD()->bShowHUD=false;
        ArenaPlayer->PreferredCameraDistance=2700;ArenaPlayer->CameraBoom->TargetArmLength=2700;
        ArenaPlayer->CameraBoom->SetRelativeRotation(FRotator(-45,-35,0));
        ArenaPlayer->CameraBoom->TargetOffset=FVector(0,0,100);
        ArenaPlayer->FollowCamera->SetFieldOfView(60);
        ArenaPlayer->SetActorLocation(FVector(0,-450,40));Next(1);
    }
    else if(Phase==1 && Age>5) {Capture(TEXT("Plaza"));Next(2);}
    else if(Phase==2 && Age>.5) {ArenaPlayer->SetActorLocation(FVector(-3250,-900,40));Next(3);}
    else if(Phase==3 && Age>4) {Capture(TEXT("Market"));Next(4);}
    else if(Phase==4 && Age>.5) {ArenaPlayer->SetActorLocation(FVector(0,-3600,40));Next(5);}
    else if(Phase==5 && Age>4)
    {
        if(Street && (Street->GetFixtureCount()!=27 || Street->GetActiveLightCount()>10 || Street->GetShadowLightCount()>2 || Street->GetActiveLightCount()<1)) {Fail(TEXT("Fixture or light budget mismatch"));return;}
        Capture(TEXT("Subway"));Next(6);
    }
    else if(Phase==6 && Age>.5) {ArenaPlayer->SetActorLocation(FVector(0,0,20000));Next(7);}
    else if(Phase==7 && Age>.25)
    {
        if(Street && Street->GetActiveLightCount()!=0) {Fail(TEXT("Street lights leaked into elevated testing range"));return;}
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);
        UE_LOG(LogTemp,Display,TEXT("STREET_LIGHTING_PASS: three fixed-camera views, 27 fixtures, <=10 local lights, <=2 shadow lights, distant-range culling; before=%d"),bBefore);
        FPlatformMisc::RequestExit(false);
    }
#endif
}
