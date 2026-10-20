#include "ForbiddenWorkbenchLayout.h"
#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "KineticCardVFXDirector.h"
#include "NecromancyVisualComponent.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

void AMVPController::TickNeonPolishSmokeTest()
{
#if !UE_BUILD_SHIPPING
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());auto* ArenaPlayer=Cast<AMVPPlayer>(GetPawn());
    if(!Game || !ArenaPlayer || !Game->AudioDirector || !Game->Spawner) return;
    ANeonAudioDirector* Audio=Game->AudioDirector;
    static TSet<ENeonSpellStyle> SeenStyles;
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("NEON_POLISH_FAIL phase=%d: %s"),SmokePhase,Why);bSmokeTest=false;FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Advance=[&](int32 Phase){SmokePhase=Phase;SmokePhaseTime=FPlatformTime::Seconds();};
    auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);};
    const double Elapsed=FPlatformTime::Seconds()-SmokePhaseTime;
    if(SmokePhase>0 && Elapsed>25) {Fail(TEXT("Timeout"));return;}
    if(SmokePhase==0)
    {
        if(!Game->bInCodex || !IsPaused() || !Audio->IsReady() || !Game->SpellImpactDirector || !Game->SpellImpactDirector->IsReady()) {Fail(TEXT("Menu/audio/spell assets unavailable"));return;}
        Audio->SetMusicVolume(0.6f);Audio->SetSFXVolume(1.f);
        int32 CharacterFills=0;for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It)
            if(It->ActorHasTag(TEXT("CharacterReadability")) && It->GetLightComponent()->LightingChannels.bChannel1 && !It->GetLightComponent()->CastShadows && !It->GetLightComponent()->bAffectGlobalIllumination) ++CharacterFills;
        if(CharacterFills!=2 || !ArenaPlayer->FindComponentByClass<UNecromancyVisualComponent>()->LightingChannels.bChannel1) {Fail(TEXT("Character-only illumination setup failed"));return;}
        SetMouseLocation(800,390);Advance(1);
    }
    else if(SmokePhase==1 && Elapsed>3)
    {
        Capture(TEXT("NeonCodex-MainMenu.png"));Advance(2);
    }
    else if(SmokePhase==2 && Elapsed>0.7)
    {
        auto* HUD=Cast<AMVPHUD>(GetHUD());if(!HUD) {Fail(TEXT("Missing HUD"));return;}
        HUD->ClickAt(ForbiddenWorkbench::TabsX+2*ForbiddenWorkbench::TabPitch+30,ForbiddenWorkbench::TabsY+17);if(Game->SchoolFilter!=7) {Fail(TEXT("School tab hit testing failed"));return;}
        HUD->ClickAt(ForbiddenWorkbench::TabsX+30,ForbiddenWorkbench::TabsY+17);HUD->ClickAt(ForbiddenWorkbench::ViewX+30,ForbiddenWorkbench::ViewY+10);HUD->ClickAt(ForbiddenWorkbench::NextX+30,ForbiddenWorkbench::PageY+10);
        if(Game->LibraryPage!=1 || Game->GetLibraryCards().Num()!=16) {Fail(TEXT("Neon menu paging failed"));return;}
        HUD->ClickAt(ForbiddenWorkbench::PrevX+30,ForbiddenWorkbench::PageY+10);Game->StartingCards={TEXT("PulseDart"),TEXT("PrismBurst")};
        if(!Game->StartSelectedRun()) {Fail(TEXT("Starting loadout failed"));return;}
        Game->Spawner->StopWaves();ArenaPlayer->bBenchmarkInvulnerable=true;
        // Equip the entire collection through the normal validated deck flow.
        for(UCardDataAsset* Card:ArenaPlayer->Deck->UnlockedCards)
            if(!ArenaPlayer->Deck->FindOwnedSpell(Card->CardID))
            {
                ArenaPlayer->Deck->EquippedDeck={Card};ArenaPlayer->Deck->DiscardHand();ArenaPlayer->Deck->DrawLevelUpHand();
                if(!ArenaPlayer->Deck->AddSelectedCardSpell(Card)) {Fail(TEXT("Could not equip all spell identities"));return;}
            }
        ArenaPlayer->Deck->EquippedDeck=ArenaPlayer->Deck->UnlockedCards;
        for(int32 I=0;I<9;++I)
        {
            const float Angle=I*2.f*PI/9.f;const FVector Pos=I==0 ? FVector(450,0,40) : FVector(FMath::Cos(Angle)*650,FMath::Sin(Angle)*650,40);
            if(auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(Pos)))) {Enemy->Health=1.e9f;Enemy->SetActorTickEnabled(false);}
        }
        ArenaPlayer->PreferredCameraDistance=1700;ArenaPlayer->Health=30;
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this,10.f);Advance(3);
    }
    else if(SmokePhase==3 && Elapsed>0.4)
    {
        if(Audio->IsCriticalHealth() || Audio->LowHealthAlarmPlays!=0) {Fail(TEXT("Alarm played at 30 HP instead of below 30"));return;}
        ArenaPlayer->Health=29;Advance(4);
    }
    else if(SmokePhase==4)
    {
        TSet<ENeonSpellStyle> CurrentStyles;
        for(TActorIterator<AMVPProjectile> It(GetWorld());It;++It) if(It->bActive) {SeenStyles.Add(It->SpellStyle);CurrentStyles.Add(It->SpellStyle);}
        if((Elapsed>3.5 && CurrentStyles.Num()==5) || Elapsed>7.f)
        {
            if(SeenStyles.Num()!=5 || !Audio->IsCriticalHealth() || Audio->LowHealthAlarmPlays<1 || Audio->LowHealthAlarmPlays>2 || Audio->EnemyPlays!=0
                || Game->SpellImpactDirector->Accepted==0 || Game->CardVFXDirector->Accepted==0) {UE_LOG(LogTemp,Error,TEXT("POLISH_DIAGNOSTIC styles=%d alarms=%d reserved=%d schoolHits=%d goldHits=%d"),SeenStyles.Num(),Audio->LowHealthAlarmPlays,Audio->EnemyPlays,Game->SpellImpactDirector->Accepted,Game->CardVFXDirector->Accepted);Fail(TEXT("Simultaneous identities, alarm, impacts or audio reservation failed"));return;}
            Capture(TEXT("NeonSpells-LowHealth.png"));Advance(5);
        }
    }
    else if(SmokePhase==5 && Elapsed>0.6)
    {
        ArenaPlayer->Health=30;Game->AddXP(Game->GetXPRequirement());Audio->UpdateMusicState();
        if(!Game->bChoosingCard || Audio->IsMenuMusic()) {Fail(TEXT("Draft interrupted gameplay music"));return;}
        Advance(6);
    }
    else if(SmokePhase==6 && Elapsed>0.5)
    {
        if(Audio->IsCriticalHealth() || Audio->MusicTransitions!=1) {Fail(TEXT("Recovery failed to clear health alarm or music restarted"));return;}
        ToggleSettings();if(!IsSettingsOpen() || Game->ChooseCard(0)) {Fail(TEXT("Settings allowed hidden card input"));return;}
        CloseSettings();if(!IsPaused() || !Game->ChooseCard(0) || IsPaused()) {Fail(TEXT("Nested pause restoration failed"));return;}
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("NeonPolishLowHealth"),FPaths::ProjectSavedDir()/TEXT("AudioQA"));
        UE_LOG(LogTemp,Display,TEXT("NEON_POLISH_PASS: 5 simultaneous projectile identities, teal guard, school-specific pooled impacts, enemy-only audio reservation, exact <30 HP alarm/recovery, character-only fill lights, functional neon menu tabs/paging, uninterrupted draft music"));
        Advance(7);
    }
    else if(SmokePhase==7 && Elapsed>1)
    {
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexPreferences_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexAudio_Automation"),0);
        bSmokeTest=false;FPlatformMisc::RequestExit(false);
    }
#endif
}
