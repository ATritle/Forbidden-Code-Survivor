#include "MVPGame.h"
#include "NeonCityDirector.h"
#include "NeonAudioDirector.h"
#include "CodexPreferencesSave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "EnemySpawner.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "CodexCardSelectionWidget.h"
#include "MortisTerrain.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"

void AMVPController::TickCitySmokeTest()
{
#if !UE_BUILD_SHIPPING
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    auto* ArenaPlayer=Cast<AMVPPlayer>(GetPawn());
    ANeonCityDirector* City=nullptr;for(TActorIterator<ANeonCityDirector> It(GetWorld());It;++It){City=*It;break;}
    if(!Game || !ArenaPlayer || !City || !Game->Spawner) return;
    auto Fail=[&](const TCHAR* Error){UE_LOG(LogTemp,Error,TEXT("CITY_SMOKE_FAIL phase=%d: %s"),SmokePhase,Error);bSmokeTest=false;FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Advance=[&](int32 Phase){SmokePhase=Phase;SmokePhaseTime=FPlatformTime::Seconds();UE_LOG(LogTemp,Display,TEXT("CITY_SMOKE_PHASE %d"),Phase);};
    auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);};
    const double Elapsed=FPlatformTime::Seconds()-SmokePhaseTime;
    if(SmokePhase>0 && Elapsed>45.f){Fail(TEXT("Timeout"));return;}
    static TWeakObjectPtr<AMVPEnemy> Chaser;
    static FVector BeforeMove;
    if(SmokePhase==0)
    {
        if(!Game->bInCodex || !IsPaused()) {Fail(TEXT("City skipped the pre-run deck"));return;}
        if(!Game->AudioDirector || !Game->AudioDirector->IsReady() || !Game->AudioDirector->IsMenuMusic()) {Fail(TEXT("Menu music/audio bank unavailable"));return;}
        if(FParse::Param(FCommandLine::Get(),TEXT("CityAudioCapture"))) UAudioMixerBlueprintLibrary::StartRecordingOutput(this,30.f);
        // Deliberately use only the automation slot to validate old-save
        // migration, the new far clamp, and exact v2 round-trip persistence.
        auto* OldZoom=Cast<UCodexPreferencesSave>(UGameplayStatics::CreateSaveGameObject(UCodexPreferencesSave::StaticClass()));
        OldZoom->Version=1;OldZoom->CameraDistance=1300;
        UGameplayStatics::SaveGameToSlot(OldZoom,TEXT("CodexPreferences_Automation"),0);
        ArenaPlayer->LoadCameraPreference();
        if(ArenaPlayer->PreferredCameraDistance!=2100 || !ArenaPlayer->SaveCameraPreference()) {Fail(TEXT("Wide camera migration failed"));return;}
        Game->StartingCards={TEXT("PrismBurst"),TEXT("KineticCards")};
        if(!Game->StartSelectedRun()) {Fail(TEXT("Energy deck failed to enter city"));return;}
        if(Game->AudioDirector->IsMenuMusic() || Game->Spawner->MaxActiveEnemies!=180 || Game->Spawner->EnemiesPerWave!=8 || !FMath::IsNearlyEqual(Game->Spawner->WaveInterval,1.4f)) {Fail(TEXT("Gameplay music/wave pacing incorrect"));return;}
        ArenaPlayer->ZoomCamera(-100);ArenaPlayer->SaveCameraPreference();ArenaPlayer->LoadCameraPreference();
        if(ArenaPlayer->PreferredCameraDistance!=3600) {Fail(TEXT("Wide zoom clamp/persistence failed"));return;}
        ArenaPlayer->PreferredCameraDistance=2100;ArenaPlayer->SaveCameraPreference();ArenaPlayer->LoadCameraPreference();
        if(ArenaPlayer->PreferredCameraDistance!=2100) {Fail(TEXT("Zoom migration repeated on a new save"));return;}
        Game->Spawner->StopWaves();ArenaPlayer->bBenchmarkInvulnerable=true;
        if(FParse::Param(FCommandLine::Get(),TEXT("CityPreview")))
        {
            if(GetHUD()) GetHUD()->bShowHUD=false;
            City->bEnableBuildingCutaway=false;City->Rain->SetWorldScale3D(FVector(3,3,1));
            ArenaPlayer->PreferredCameraDistance=24500;ArenaPlayer->CameraBoom->TargetArmLength=24500;
            ArenaPlayer->CameraBoom->SetRelativeRotation(FRotator(-48.f,-42.f,0));ArenaPlayer->CameraBoom->TargetOffset.Z=450;
            ArenaPlayer->FollowCamera->SetFieldOfView(43);
            Advance(70);return;
        }
        auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        if(!Nav || City->WalkableDestinations.Num()!=6 || City->Rain->GetInstanceCount()!=City->RainDrops) {Fail(TEXT("City nav/weather setup missing"));return;}
        for(const FVector Goal:City->WalkableDestinations)
        {
            auto* Path=Nav->FindPathToLocationSynchronously(Game,FVector(0,0,-15),Goal);
            if(!Path || !Path->IsValid() || Path->IsPartial()) {UE_LOG(LogTemp,Error,TEXT("CITY_ROUTE_FAILED %s"),*Goal.ToString());Fail(TEXT("A city elevation is unreachable"));return;}
        }
        for(ASpellBase* Spell:ArenaPlayer->Deck->ActiveWeapons) Spell->StopCasting();
        // Physically climb the real shopping ramp using normal movement and floor queries.
        ArenaPlayer->SetActorLocation(FVector(600,650,40));ArenaPlayer->Movement->StopMovementImmediately();
        for(int32 I=0;I<245;++I)
        {
            ArenaPlayer->AddMovementInput(FVector::ForwardVector);ArenaPlayer->Movement->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);ArenaPlayer->UpdateGroundPhysics(1.f/60.f);
        }
        const FVector Raised=ArenaPlayer->GetActorLocation();UE_LOG(LogTemp,Display,TEXT("CITY_RAMP_RESULT %s"),*Raised.ToString());
        if(Raised.X<2200 || FMath::Abs(Raised.Z-375)>12) {Fail(TEXT("ArenaPlayer did not physically climb onto shopping promenade"));return;}
        // Solid building walls must block movement; no-collision sign faces must not block the street.
        ArenaPlayer->SetActorLocation(FVector(2400,-1600,40));ArenaPlayer->Movement->StopMovementImmediately();
        for(int32 I=0;I<90;++I) {ArenaPlayer->AddMovementInput(FVector::ForwardVector);ArenaPlayer->Movement->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);ArenaPlayer->UpdateGroundPhysics(1.f/60.f);}
        if(ArenaPlayer->GetActorLocation().X>2730) {Fail(TEXT("ArenaPlayer passed through an industrial tower"));return;}
        ArenaPlayer->Movement->StopMovementImmediately();ArenaPlayer->SetActorLocation(FVector(0,0,900));
        const int32 Landing=ArenaPlayer->LandingCount;
        for(int32 I=0;I<150;++I) ArenaPlayer->UpdateGroundPhysics(1.f/60.f);
        if(ArenaPlayer->bFalling || ArenaPlayer->LandingCount!=Landing+1 || FMath::Abs(ArenaPlayer->GetActorLocation().Z-40)>3) {Fail(TEXT("City landing/gravity failed"));return;}
        BeforeMove=ArenaPlayer->GetActorLocation();ArenaPlayer->MoveForward(1.f);ArenaPlayer->Movement->TickComponent(0.1f,LEVELTICK_All,nullptr);ArenaPlayer->Movement->StopMovementImmediately();
        if(FVector::DotProduct(ArenaPlayer->GetActorLocation()-BeforeMove,FRotator(0,-35,0).Vector())<=0) {Fail(TEXT("Isometric movement not camera-relative"));return;}
        ArenaPlayer->SetActorLocation(FVector(0,0,40));
        // Keep five reachable fixture positions; their gem values total the current draft cost.
        const int32 Required=Game->GetXPRequirement();
        for(int32 I=0;I<5;++I)
            if(auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(280+I*70,0,40)))))
            {Enemy->EssenceReward=Required/5+(I<Required%5 ? 1 : 0);Enemy->SetActorTickEnabled(false);}
        for(ASpellBase* Spell:ArenaPlayer->Deck->ActiveWeapons) Spell->StartCasting();
        Advance(1);
    }
    else if(SmokePhase==1)
    {
        if(Game->bChoosingCard)
        {
            Game->AudioDirector->UpdateMusicState();
            if(Game->AudioDirector->IsMenuMusic() || Game->AudioDirector->AttackPlays==0 || Game->AudioDirector->ImpactPlays==0 || Game->AudioDirector->EnemyPlays!=0) {Fail(TEXT("Combat sound hooks, reserved enemy isolation or gameplay music failed"));return;}
            if(!IsPaused() || !Game->SelectionWidget || ArenaPlayer->Deck->CurrentHand.Num()!=3) {Fail(TEXT("City XP/three-card selection flow failed"));return;}
            TSet<FName> IDs;for(UCardDataAsset* Card:ArenaPlayer->Deck->CurrentHand) IDs.Add(Card->CardID);
            if(IDs.Num()!=3 || !Game->ChooseCard(0) || IsPaused()) {Fail(TEXT("City drafting duplicate/resume failed"));return;}
            Game->AudioDirector->UpdateMusicState();
            if(Game->AudioDirector->IsMenuMusic()) {Fail(TEXT("Gameplay music did not resume after drafting"));return;}
            for(ASpellBase* Spell:ArenaPlayer->Deck->ActiveWeapons) Spell->StopCasting();
            ArenaPlayer->ClearTalismans();
            for(TActorIterator<AMVPProjectile> It(GetWorld());It;++It) It->Deactivate();
            ArenaPlayer->SetActorLocation(FVector(1700,-1500,40));
            Chaser=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(3450,-650,40))));
            if(!Chaser.IsValid()) {Fail(TEXT("Missing city chase target"));return;}
            Chaser->ApplyEnemyArchetype(9); // largest footprint must also get around the shopping block
            Chaser->Health=100000;Chaser->BaseMovementSpeed=950;
            Advance(2);return;
        }
        for(TActorIterator<AMVPGem> It(GetWorld());It;++It) if(It->bActive) {ArenaPlayer->SetActorLocation(It->GetActorLocation());break;}
    }
    else if(SmokePhase==2 && Elapsed>8.f)
    {
        if(!Chaser.IsValid() || FVector::Dist2D(Chaser->GetActorLocation(),ArenaPlayer->GetActorLocation())>150) {Fail(TEXT("Boss could not chase around shopping block collision"));return;}
        Chaser->ReturnToPool();ArenaPlayer->SetActorLocation(FVector(0,0,40));
        Game->Spawner->SpawnWave();
        if(Game->Spawner->GetActiveEnemyCount()==0) {Fail(TEXT("City offscreen wave spawn failed"));return;}
        ANeonAudioDirector* Audio=Game->AudioDirector;
        for(int32 I=0;I<200;++I) {Audio->PlayAttack(TEXT("Pulse"),ArenaPlayer->GetActorLocation());Audio->PlayImpact(ArenaPlayer->GetActorLocation());}
        if(!Audio->IsReady() || Audio->GetVoiceCapacity()!=18 || Audio->GetPlayingEffectCount()>18 || Audio->UIPlays==0 || Audio->MusicTransitions!=1) {Fail(TEXT("Audio voice budget/music continuity test failed"));return;}
        UE_LOG(LogTemp,Display,TEXT("CITY_AUDIO_PASS: attacks=%d impacts=%d enemies=%d UI=%d transitions=%d effects=%d/18; zoom migration/3600 clamp/persistence; 8 enemies/1.4s, cap 180"),Audio->AttackPlays,Audio->ImpactPlays,Audio->EnemyPlays,Audio->UIPlays,Audio->MusicTransitions,Audio->GetPlayingEffectCount());
        if(FParse::Param(FCommandLine::Get(),TEXT("CityAudioCapture"))) UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("CityMix"),FPaths::ProjectSavedDir()/TEXT("AudioQA"));
        UE_LOG(LogTemp,Display,TEXT("CITY_SMOKE_PASS: six reachable elevations, physical ramp ascent, wall collision, weighted landing, camera-relative movement, XP/unique draft, actual enemy chase, offscreen waves, bounded rain"));
        Capture(TEXT("NeonCity-Gameplay.png"));Advance(3);
    }
    else if(SmokePhase==3 && Elapsed>0.6f)
    {UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexPreferences_Automation"),0);bSmokeTest=false;FPlatformMisc::RequestExit(false);}
    else if(SmokePhase==70 && Elapsed>8.f) {Capture(TEXT("NeonCity-Overview.png"));Advance(71);}
    else if(SmokePhase==71 && Elapsed>1.f)
    {
        ArenaPlayer->SetActorLocation(FVector(-3250,-900,40));ArenaPlayer->PreferredCameraDistance=2200;ArenaPlayer->CameraBoom->TargetArmLength=2200;
        ArenaPlayer->CameraBoom->SetRelativeRotation(FRotator(-45.f,-35.f,0));ArenaPlayer->CameraBoom->TargetOffset.Z=100;
        ArenaPlayer->FollowCamera->SetFieldOfView(60);City->bEnableBuildingCutaway=true;City->Rain->SetWorldScale3D(FVector::OneVector);
        for(int32 I=0;I<8;++I) if(auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(-1200+I*100,-800+(I%3)*150,40))))) {Enemy->Health=100000;Enemy->SetActorTickEnabled(false);}
        Advance(72);
    }
    else if(SmokePhase==72 && Elapsed>4.f) {Capture(TEXT("NeonCity-Market.png"));Advance(73);}
    else if(SmokePhase==73 && Elapsed>1.f)
    {
        ArenaPlayer->SetActorLocation(FVector(0,3550,755));ArenaPlayer->PreferredCameraDistance=2000;ArenaPlayer->CameraBoom->TargetArmLength=2000;Advance(74);
    }
    else if(SmokePhase==74 && Elapsed>3.f) {Capture(TEXT("NeonCity-Shrine.png"));Advance(75);}
    else if(SmokePhase==75 && Elapsed>1.f)
    {
        ArenaPlayer->SetActorLocation(FVector(0,-3600,40));ArenaPlayer->PreferredCameraDistance=2200;ArenaPlayer->CameraBoom->TargetArmLength=2200;Advance(76);
    }
    else if(SmokePhase==76 && Elapsed>3.f) {Capture(TEXT("NeonCity-Subway.png"));Advance(77);}
    else if(SmokePhase==77 && Elapsed>1.f)
    {
        ArenaPlayer->SetActorLocation(FVector(1700,1900,40));ArenaPlayer->PreferredCameraDistance=7000;ArenaPlayer->CameraBoom->TargetArmLength=7000;
        ArenaPlayer->CameraBoom->SetRelativeRotation(FRotator(-38.f,-42.f,0));ArenaPlayer->CameraBoom->TargetOffset=FVector(1650,1700,1600);
        ArenaPlayer->FollowCamera->SetFieldOfView(45);City->bEnableBuildingCutaway=false;Advance(78);
    }
    else if(SmokePhase==78 && Elapsed>3.f) {Capture(TEXT("NeonCity-ShoppingBlock.png"));Advance(3);}
#endif
}

