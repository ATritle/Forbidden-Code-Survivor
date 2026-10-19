#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

/** Real audio-component integration check; UI health state outlives the audible warning. */
void RunHealthAlarmSmoke(AMVPController* Controller)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0;static double Since=FPlatformTime::Seconds();static float PausedAge=0;
    auto* Game=Cast<AMVPArenaGameMode>(Controller->GetWorld()->GetAuthGameMode());auto* Player=Cast<AMVPPlayer>(Controller->GetPawn());
    if(!Game || !Player || !Game->AudioDirector || !Game->Spawner) return;
    auto* Audio=Game->AudioDirector.Get();const double Elapsed=FPlatformTime::Seconds()-Since;
    auto Next=[&](int32 Step){Phase=Step;Since=FPlatformTime::Seconds();};
    auto Fail=[&](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("HEALTH_ALARM_FAIL phase=%d age=%.3f gain=%.3f: %s"),Phase,Audio->GetHealthAlarmElapsed(),Audio->GetHealthAlarmGain(),Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    if(Elapsed>12) {Fail(TEXT("Timeout"));return;}
    if(Phase==0)
    {
        Game->StartingCards={TEXT("PulseDart"),TEXT("PrismBurst")};if(!Game->StartSelectedRun() || !Audio->IsReady()) {Fail(TEXT("Run or audio unavailable"));return;}
        Game->Spawner->StopWaves();for(ASpellBase* Spell:Player->Deck->ActiveWeapons) Spell->StopCasting();Player->bBenchmarkInvulnerable=true;
        Player->Health=30;Audio->SetMusicVolume(0);Audio->SetSFXVolume(0.5f);
        UAudioMixerBlueprintLibrary::StartRecordingOutput(Controller,15.f);Next(1);
    }
    else if(Phase==1 && Elapsed>0.3)
    {
        if(Audio->IsCriticalHealth() || Audio->LowHealthAlarmPlays!=0) {Fail(TEXT("Alarm triggered at 30 HP"));return;}
        Player->Health=29;Next(2);
    }
    else if(Phase==2 && Audio->GetHealthAlarmElapsed()>=2.f)
    {
        if(!Audio->IsHealthAlarmPlaying() || Audio->GetHealthAlarmGain()!=1 || Audio->LowHealthAlarmPlays!=1) {Fail(TEXT("Initial warning missing"));return;}
        PausedAge=Audio->GetHealthAlarmElapsed();Controller->SetPause(true);Next(3);
    }
    else if(Phase==3 && Elapsed>0.6)
    {
        if(!Audio->IsCriticalHealth() || FMath::Abs(Audio->GetHealthAlarmElapsed()-PausedAge)>0.05f) {Fail(TEXT("Pause consumed the warning duration"));return;}
        Controller->SetPause(false);Next(4);
    }
    else if(Phase==4 && Audio->GetHealthAlarmElapsed()>=4.7f)
    {
        if(Audio->GetHealthAlarmGain()!=1 || !Audio->IsHealthAlarmPlaying()) {Fail(TEXT("Alarm faded before five seconds"));return;}
        Next(5);
    }
    else if(Phase==5 && Audio->GetHealthAlarmElapsed()>=5.25f)
    {
        if(Audio->GetHealthAlarmGain()<=0 || Audio->GetHealthAlarmGain()>=0.9f || !Audio->IsHealthAlarmPlaying()) {Fail(TEXT("Expected a gradual fade after five seconds"));return;}
        const float Gain=Audio->GetHealthAlarmGain();Audio->SetSFXVolume(0.25f);
        if(Audio->GetHealthAlarmGain()!=Gain) {Fail(TEXT("Volume adjustment reset the fade"));return;}
        UE_LOG(LogTemp,Display,TEXT("HEALTH_ALARM_FADING age=%.3f gain=%.3f"),Audio->GetHealthAlarmElapsed(),Gain);Next(6);
    }
    else if(Phase==6 && Audio->GetHealthAlarmElapsed()>=6.1f)
    {
        if(Audio->IsHealthAlarmPlaying() || Audio->GetHealthAlarmGain()!=0 || !Audio->IsCriticalHealth() || Player->Health!=29) {Fail(TEXT("Audio did not stop independently of the health warning"));return;}
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/LowHealth-SilentWarning.png"),true,false);Next(7);
    }
    else if(Phase==7 && Elapsed>1.0)
    {
        if(Audio->LowHealthAlarmPlays!=1 || Audio->IsHealthAlarmPlaying() || !Audio->IsCriticalHealth()) {Fail(TEXT("Alarm restarted while health remained low"));return;}
        UAudioMixerBlueprintLibrary::StopRecordingOutput(Controller,EAudioRecordingExportType::WavFile,TEXT("HealthAlarmFade"),FPaths::ProjectSavedDir()/TEXT("AudioQA"));
        Player->Health=30;Next(8);
    }
    else if(Phase==8 && Elapsed>0.2)
    {
        if(Audio->IsCriticalHealth() || Audio->IsHealthAlarmPlaying()) {Fail(TEXT("Recovery did not clear the warning"));return;}
        Player->Health=29;Next(9);
    }
    else if(Phase==9 && Elapsed>0.25)
    {
        if(Audio->LowHealthAlarmPlays!=2 || !Audio->IsCriticalHealth() || !Audio->IsHealthAlarmPlaying() || Audio->GetHealthAlarmGain()!=1) {Fail(TEXT("A new low-health episode did not rearm"));return;}
        Player->Health=30;Next(10);
    }
    else if(Phase==10 && Elapsed>0.2)
    {
        if(Audio->IsHealthAlarmPlaying() || Audio->IsCriticalHealth()) {Fail(TEXT("Early recovery did not stop the alarm"));return;}
        UE_LOG(LogTemp,Display,TEXT("HEALTH_ALARM_PASS: exact <30 threshold; full five seconds; 0.75s fade; persistent silent visual warning; no repeat; pause-safe duration; SFX volume during fade; recovery and rearm"));
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexAudio_Automation"),0);
        FPlatformMisc::RequestExit(false);Next(11);
    }
#endif
}
