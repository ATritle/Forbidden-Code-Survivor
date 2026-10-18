#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "NeonSettingsWidget.h"
#include "KineticCardVFXDirector.h"
#include "CodexCardSelectionWidget.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/WidgetTree.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

void AMVPController::TickSettingsSmokeTest()
{
#if !UE_BUILD_SHIPPING
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    auto* ArenaPlayer=Cast<AMVPPlayer>(GetPawn());
    if(!Game || !ArenaPlayer || !Game->AudioDirector || !Game->Spawner) return;
    ANeonAudioDirector* Audio=Game->AudioDirector;
    static bool bRestarted=false,bSawGoldCard=false;
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("SETTINGS_CARD_FAIL phase=%d: %s"),SmokePhase,Why);bSmokeTest=false;FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Advance=[&](int32 Next){SmokePhase=Next;SmokePhaseTime=FPlatformTime::Seconds();};
    auto CloseButton=[&]()
    {
        if(!SettingsWidget) return false;
        auto* Button=Cast<UButton>(SettingsWidget->WidgetTree->FindWidget(TEXT("CloseSettings")));
        if(!Button) return false;Button->OnClicked.Broadcast();return !IsSettingsOpen();
    };
    const double Elapsed=FPlatformTime::Seconds()-SmokePhaseTime;
    if(SmokePhase>0 && Elapsed>20) {Fail(TEXT("Timeout"));return;}
    if(SmokePhase==0)
    {
        if(bRestarted)
        {
            if(!FMath::IsNearlyEqual(Audio->GetMusicVolume(),0.37f) || !FMath::IsNearlyEqual(Audio->GetSFXVolume(),0.22f) || !Audio->IsMenuMusic()) {Fail(TEXT("Saved volumes did not survive world restart"));return;}
            UGameplayStatics::DeleteGameInSlot(TEXT("CodexAudio_Automation"),0);
            UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);
            UE_LOG(LogTemp,Display,TEXT("SETTINGS_CARD_PASS: HUD settings button, live slider bindings, independent mute, automatic save, persistence across world restart, nested draft pause/focus, one menu-to-game music transition, spinning gold card/trail, separate bounded impact pool"));
            bSmokeTest=false;FPlatformMisc::RequestExit(false);return;
        }
        if(!Game->bInCodex || !IsPaused() || !Audio->IsReady()) {Fail(TEXT("Initial menu/audio missing"));return;}
        auto* HUD=Cast<AMVPHUD>(GetHUD());if(!HUD) {Fail(TEXT("Missing HUD"));return;}
        HUD->ClickAt(1410,158);
        if(!IsSettingsOpen() || !IsPaused()) {Fail(TEXT("Main-menu settings button failed"));return;}
        auto* Music=Cast<USlider>(SettingsWidget->WidgetTree->FindWidget(TEXT("MusicVolumeSlider")));
        auto* SFX=Cast<USlider>(SettingsWidget->WidgetTree->FindWidget(TEXT("SFXVolumeSlider")));
        if(!Music || !SFX) {Fail(TEXT("Volume sliders missing"));return;}
        Music->SetValue(0.37f);Music->OnValueChanged.Broadcast(0.37f);SFX->SetValue(0.22f);SFX->OnValueChanged.Broadcast(0.22f);
        if(!FMath::IsNearlyEqual(Audio->GetMusicVolume(),0.37f) || !FMath::IsNearlyEqual(Audio->GetSFXVolume(),0.22f)) {Fail(TEXT("Slider changes did not apply live"));return;}
        Advance(1);
    }
    else if(SmokePhase==1 && Elapsed>0.8)
    {
        // Verify the debounced write happened while gameplay is paused.
        Audio->LoadAudioSettings();
        if(!FMath::IsNearlyEqual(Audio->GetMusicVolume(),0.37f) || !FMath::IsNearlyEqual(Audio->GetSFXVolume(),0.22f)) {Fail(TEXT("Paused autosave failed"));return;}
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/NeonSettings.png"),true,false);Advance(2);
    }
    else if(SmokePhase==2 && Elapsed>0.5)
    {
        if(!CloseButton() || !IsPaused()) {Fail(TEXT("Closing settings unpaused the main menu"));return;}
        Game->StartingCards={TEXT("KineticCards"),TEXT("PrismBurst")};
        if(!Game->StartSelectedRun()) {Fail(TEXT("Could not start energy deck"));return;}
        Game->Spawner->StopWaves();ArenaPlayer->bBenchmarkInvulnerable=true;
        ToggleSettings();
        if(!IsPaused() || !CloseButton() || IsPaused()) {Fail(TEXT("Gameplay settings pause/resume failed"));return;}
        Game->AddXP(Game->GetXPRequirement());Audio->UpdateMusicState();
        if(!Game->bChoosingCard || Audio->IsMenuMusic()) {Fail(TEXT("Draft changed to menu music"));return;}
        ToggleSettings();
        if(!IsSettingsOpen() || Game->ChooseCard(0)) {Fail(TEXT("Settings allowed a hidden draft choice"));return;}
        if(!CloseButton() || !IsPaused() || !Game->SelectionWidget->GetIsEnabled() || !Game->ChooseCard(0) || IsPaused()) {Fail(TEXT("Nested settings damaged draft focus/pause"));return;}
        Audio->UpdateMusicState();if(Audio->MusicTransitions!=1) {Fail(TEXT("Draft/settings restarted menu music"));return;}
        Audio->SetSFXVolume(0);Audio->SetMusicVolume(0);
        TArray<UAudioComponent*> Components;Audio->GetComponents(Components);
        for(auto* Component:Components) if(Component->VolumeMultiplier!=0) {Fail(TEXT("Volume mute failed"));return;}
        Audio->SetMusicVolume(0.5f);
        for(auto* Component:Components) if(Component->GetName().StartsWith(TEXT("Voice_")) && Component->VolumeMultiplier!=0) {Fail(TEXT("Music control changed SFX volume"));return;}
        Audio->SetMusicVolume(0.37f);Audio->SetSFXVolume(0.22f);Audio->SaveAudioSettings();
        for(ASpellBase* Spell:ArenaPlayer->Deck->ActiveWeapons) if(!Spell->IsA<AMVPLightningSpell>()) Spell->StopCasting();
        if(!Game->CardVFXDirector || !Game->CardVFXDirector->IsReady()) {Fail(TEXT("Gold Niagara impact pool missing"));return;}
        if(auto* Enemy=Cast<AMVPEnemy>(Game->Spawner->ActivateEnemyFromPool(FTransform(FVector(600,0,40))))) {Enemy->Health=1.e9f;Enemy->SetActorTickEnabled(false);}
        ArenaPlayer->PreferredCameraDistance=1100;
        Advance(3);
    }
    else if(SmokePhase==3)
    {
        for(TActorIterator<AMVPProjectile> It(GetWorld());It;++It)
            if(It->bActive && It->ProjectileType==EProjectileType::PhysicalMesh)
            {
                if(!It->Visual->GetMaterial(0) || It->Visual->GetMaterial(0)->GetName()!=TEXT("M_KineticCard") || It->Visual->GetRelativeRotation().IsNearlyZero()) {Fail(TEXT("Gold card material/animation missing"));return;}
                TArray<UNiagaraComponent*> FX;It->GetComponents(FX);bool bTrail=false;
                for(auto* Component:FX) if(Component->GetAsset() && Component->GetAsset()->GetName()==TEXT("NS_KineticTrail") && Component->IsActive()) bTrail=true;
                if(!bTrail) {Fail(TEXT("Gold card segmented trail missing"));return;}bSawGoldCard=true;
            }
        if(Elapsed>1.8 && bSawGoldCard && Game->CardVFXDirector->Accepted>0)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/KineticCards-Gold.png"),true,false);Advance(4);
        }
    }
    else if(SmokePhase==4 && Elapsed>0.7)
    {
        const int32 Before=Game->CardVFXDirector->Accepted;
        for(int32 I=0;I<40;++I) Game->CardVFXDirector->PlayImpact(ArenaPlayer->GetActorLocation()+FVector(150,0,10));
        if(Game->CardVFXDirector->Accepted-Before>12 || Game->CardVFXDirector->Dropped==0) {Fail(TEXT("Gold impact pool exceeded its budget"));return;}
        bRestarted=true;SetPause(false);UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this)));
    }
#endif
}
